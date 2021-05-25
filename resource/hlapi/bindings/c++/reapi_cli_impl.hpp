/*****************************************************************************\
 *  Copyright (c) 2019 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

#ifndef REAPI_CLI_IMPL_HPP
#define REAPI_CLI_IMPL_HPP

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
}

#include "resource/hlapi/bindings/c++/reapi_cli.hpp"
#include "resource/policies/dfu_match_policy_factory.hpp"

namespace Flux {
namespace resource_model {
namespace detail {

const int NOT_YET_IMPLEMENTED = -1;

static std::shared_ptr<f_resource_graph_t> create_filtered_graph (
            std::shared_ptr<resource_context_t> &ctx)
{
    std::shared_ptr<f_resource_graph_t> fg = nullptr;

    resource_graph_t &g = ctx->db->resource_graph;
    vtx_infra_map_t vmap = get (&resource_pool_t::idata, g);
    edg_infra_map_t emap = get (&resource_relation_t::idata, g);
    const multi_subsystemsS &filter = ctx->matcher->subsystemsS ();
    subsystem_selector_t<vtx_t, f_vtx_infra_map_t> vtxsel (vmap, filter);
    subsystem_selector_t<edg_t, f_edg_infra_map_t> edgsel (emap, filter);

    try {
        fg = std::make_shared<f_resource_graph_t> (g, edgsel, vtxsel);
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        std::cerr << "ERROR: out of memory allocating f_resource_graph_t"
                  << std::endl;
        fg = nullptr;
    }

    return fg;
}



static int do_remove (std::shared_ptr<resource_context_t> &ctx, uint64_t jobid)
{
    int rc = -1;
    if ((rc = ctx->traverser->remove ((int64_t)jobid)) == 0) {
        if (ctx->jobs.find (jobid) != ctx->jobs.end ()) {
           std::shared_ptr<job_info_t> info = ctx->jobs[jobid];
           info->state = job_lifecycle_t::CANCELED;
        }
    } else {
        std::cout << ctx->traverser->err_message ();
        ctx->traverser->clear_err_message ();
    }
    return rc;
}

std::shared_ptr<resource_context_t> reapi_cli_t::initialize (
                                                 const std::string &jgf)
{
    std::shared_ptr<resource_context_t> rctx = nullptr;
    std::stringstream buffer{};
    std::shared_ptr<resource_reader_base_t> rd;

    try {
        rctx = std::make_shared<resource_context_t> ();
        rctx->db = std::make_shared<resource_graph_db_t> ();
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        goto out;
    }

    rctx->perf.min = DBL_MAX;
    rctx->perf.max = 0.0f;
    rctx->perf.accum = 0.0f;
    rctx->params.load_file = "conf/default";
    rctx->params.load_format = "jgf";
    rctx->params.load_allowlist = "";
    rctx->params.matcher_name = "CA";
    rctx->params.matcher_policy = "first";
    rctx->params.o_fname = "";
    rctx->params.r_fname = "";
    rctx->params.o_fext = "dot";
    rctx->params.match_format = "jgf";
    rctx->params.o_format = emit_format_t::GRAPHVIZ_DOT;
    rctx->params.prune_filters = "ALL:core";
    rctx->params.reserve_vtx_vec = 0;
    rctx->params.elapse_time = false;
    rctx->params.disable_prompt = false;

    if ( !(rctx->matcher = create_match_cb (rctx->params.matcher_policy))) {
        rctx = nullptr;
        goto out;
    }

    if (rctx->params.reserve_vtx_vec != 0)
        rctx->db->resource_graph.m_vertices.reserve (
            rctx->params.reserve_vtx_vec);
    if ( (rd = create_resource_reader (rctx->params.load_format)) == nullptr) {
        goto out;
    }
    if (rctx->params.load_allowlist != "") {
        if (rd->set_allowlist (rctx->params.load_allowlist) < 0)
            std::cout << "ERROR: Can't set allowlist" << "\n";
        if (!rd->is_allowlist_supported ())
            std::cout << "WARN: allowlist unsupported" << "\n";
    }

    if (rctx->db->load (jgf, rd) != 0) {
        std::cerr << "ERROR: " << rd->err_message () << "\n";
        std::cerr << "ERROR: error in generating resources" << "\n";
        rctx = nullptr;
        goto out;
    }

    rctx->matcher->set_matcher_name ("CA");
    rctx->matcher->add_subsystem ("containment", "*");

    if ( !(rctx->fgraph = create_filtered_graph (rctx))) {
        rctx = nullptr;
        goto out;
    }

    rctx->jobid_counter = 1;
    if (rctx->params.prune_filters != ""
        && rctx->matcher->set_pruning_types_w_spec (rctx->matcher->dom_subsystem (),
                                                    rctx->params.prune_filters)
                                                    < 0) {
        rctx = nullptr;
        goto out;
    }

    try {
        rctx->traverser = std::make_shared<dfu_traverser_t> ();
    } catch (std::bad_alloc &e) {
        rctx = nullptr;
        goto out;
    }

    if (rctx->traverser->initialize (rctx->fgraph, rctx->db,
                                           rctx->matcher) != 0) {
        rctx = nullptr;
        goto out;
    }

    if ( !(rctx->writers = match_writers_factory_t::create (
                              match_format_t::JGF))) {
        rctx = nullptr;
        goto out;
    }

out:
    return rctx;
}

std::string reapi_cli_t::get_node (std::shared_ptr<resource_context_t> &rctx) {
    return rctx->writers->get_node ();
}

int reapi_cli_t::match_allocate (std::shared_ptr<resource_context_t> &rctx, 
                                 bool orelse_reserve,
                                 const std::string &jobspec,
                                 uint64_t &jobid, bool &reserved,
                                 std::string &R, int64_t &at, double &ov)
{
    int rc = -1;
    at = 0;
    ov = 0.0f;
    double elapse = 0.0f;
    job_lifecycle_t st;
    jobid = rctx->jobid_counter;
    try {
        Flux::Jobspec::Jobspec job {jobspec};
        std::stringstream o;

        if (orelse_reserve)
            rc = rctx->traverser->run (job, rctx->writers, match_op_t::
                                      MATCH_ALLOCATE_ORELSE_RESERVE, 
                                      (int64_t)jobid, &at);
        else
            rc = rctx->traverser->run (job, rctx->writers, match_op_t::
                                      MATCH_ALLOCATE, (int64_t)jobid, &at);

        if (rctx->traverser->err_message () != "") {
            std::cerr << "ERROR: " << rctx->traverser->err_message ();
            rctx->traverser->clear_err_message ();
            rc = -1;
            goto out;
        }
        if ( (rc = rctx->writers->emit (o)) < 0) {
            std::cerr << "ERROR: match writer emit: " << strerror (errno) << "\n";
            goto out;
        }

       std::cout << "Node name: " << rctx->writers->get_node () << "\n";
    //    nodename = rctx->writers->get_node ();

        R = o.str ();
        reserved = (at != 0)? true : false;
        st = (reserved)? job_lifecycle_t::RESERVED : job_lifecycle_t::ALLOCATED;

    } catch (Flux::Jobspec::parse_error &e) {
        std::cerr << "ERROR: Jobspec error for " << rctx->jobid_counter <<": "
                  << e.what () << "\n";
        rc = -1;
        goto out;
    }

    if (reserved)
        rctx->reservations[jobid] = jobid;
    else
        rctx->allocations[jobid] = jobid;
    rctx->jobs[jobid] = std::make_shared<job_info_t> (jobid, st, at,
                                                      "", "", elapse);
    rctx->jobid_counter++;

out:
    return rc;
}

int reapi_cli_t::update_allocate (void *h, const uint64_t jobid,
                                  const std::string &R, int64_t &at, double &ov,
                                  std::string &R_out)
{
    return NOT_YET_IMPLEMENTED;
}

int reapi_cli_t::match_allocate_multi (void *h, bool orelse_reserve,
                                       const char *jobs,
                                       queue_adapter_base_t *adapter)
{
    return NOT_YET_IMPLEMENTED;
}

int reapi_cli_t::cancel (std::shared_ptr<resource_context_t> &rctx,
                         const uint64_t jobid, bool noent_ok)
{
    int rc = -1;
    if (rctx->allocations.find (jobid) != rctx->allocations.end ()) {
        if ( (rc = do_remove (rctx, jobid)) == 0)
            rctx->allocations.erase (jobid);
    } else if (rctx->reservations.find (jobid) != rctx->reservations.end ()) {
        if ( (rc = do_remove (rctx, jobid)) == 0)
            rctx->reservations.erase (jobid);
    } else {
        std::cerr << "ERROR: nonexistent job " << jobid << "\n";
        goto out;
    }

    if (rc != 0) {
        std::cerr << "ERROR: error encountered while removing job "
                  << jobid << "\n";
    }

out:
    return rc;
}

int reapi_cli_t::info (std::shared_ptr<resource_context_t> &rctx, 
                       const uint64_t jobid, bool &reserved, int64_t &at, 
                       double &ov)
{
    if (rctx->jobs.find (jobid) == rctx->jobs.end ()) {
       std::cout << "ERROR: jobid doesn't exist: " << jobid << "\n";
       return -1;
    }

    std::string mode;
    std::shared_ptr<job_info_t> info = rctx->jobs.at (jobid);
    get_jobstate_str (info->state, mode);
    std::cout << "INFO: " << info->jobid << ", " << mode << ", "
              << info->scheduled_at << ", " << info->jobspec_fn << ", "
              << info->overhead << "\n";

    reserved = (info->state == job_lifecycle_t::RESERVED)? true : false;
    at = info->scheduled_at;
    ov = info->overhead;

    return 0;
}

int reapi_cli_t::stat (void *h, int64_t &V, int64_t &E,int64_t &J,
                       double &load, double &min, double &max, double &avg)
{
    return NOT_YET_IMPLEMENTED;
}


} // namespace Flux::resource_model::detail
} // namespace Flux::resource_model
} // namespace Flux

#endif // REAPI_CLI_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
