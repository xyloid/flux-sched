package fluxcli

// #include "reapi_cli.h"
import "C"

type (
	ReapiCtx C.struct_reapi_cli_ctx_t
)

/*! Create and initialize reapi_cli context
reapi_cli_ctx_t *reapi_cli_new ();
*/

func NewReapiCli() *ReapiCtx {
	return (*ReapiCtx)(C.reapi_cli_new())
}

/*! Destroy reapi cli context
*
* \param ctx           reapi_cli_ctx_t context object
void reapi_cli_destroy (reapi_cli_ctx_t *ctx);
*/

func ReapiCliDestroy(ctx *ReapiCtx) {
	C.reapi_cli_destroy((*C.struct_reapi_cli_ctx)(ctx))
}

/* int reapi_module_match_allocate (reapi_module_ctx_t *ctx, bool orelse_reserve,
   const char *jobspec, const uint64_t jobid,
   bool *reserved,
   char **R, int64_t *at, double *ov);*/

func ReapiCliMatchAllocate(ctx *ReapiCtx, orelse_reserve bool,
	jobspec string, jobid int) (reserved bool, allocated string, at int64, overhead float64, err int) {
	// var atlong C.long = (C.long)(at)
	var r = C.CString("teststring")

	err = (int)(C.reapi_cli_match_allocate((*C.struct_reapi_cli_ctx)(ctx),
		(C.bool)(orelse_reserve),
		C.CString(jobspec),
		(C.ulong)(jobid),
		(*C.bool)(&reserved),
		&r,
		(*C.long)(&at),
		(*C.double)(&overhead)))
	allocated = C.GoString(r)
	return reserved, allocated, at, overhead, err
}

/*! Get the information on the allocation or reservation corresponding
 *  to jobid.
 *
 *  \param ctx       reapi_cli_ctx_t context object
 *  \param jobid     const jobid of the uint64_t type.
 *  \param reserved  Boolean into which to return true if this job has been
 *                   reserved instead of allocated.
 *  \param at        If allocated, 0 is returned; if reserved, actual time
 *                   at which the job is reserved.
 *  \param ov        Double into which to return performance overhead
 *                   in terms of elapse time needed to complete
 *                   the match operation.
 *  \return          0 on success; -1 on error.

 int reapi_cli_info (reapi_cli_ctx_t *ctx, const uint64_t jobid,
	bool *reserved, int64_t *at, double *ov);
*/

func ReapiCliInfo(ctx *ReapiCtx, jobid int64) (reserved bool, at int64, overhead float64, err int) {
	err = (int)(C.reapi_cli_info((*C.struct_reapi_cli_ctx)(ctx),
		(C.ulong)(jobid),
		(*C.bool)(&reserved),
		(*C.long)(&at),
		(*C.double)(&overhead)))
	return reserved, at, overhead, err
}

/*! Cancel the allocation or reservation corresponding to jobid.
 *
 *  \param ctx       reapi_cli_ctx_t context object
 *  \param jobid     jobid of the uint64_t type.
 *  \param noent_ok  don't return an error on nonexistent jobid
 *  \return          0 on success; -1 on error.

 int reapi_cli_cancel (reapi_cli_ctx_t *ctx,
	const uint64_t jobid, bool noent_ok);*/

func ReapiCliCancel(ctx *ReapiCtx, jobid int64, noent_ok bool) (err int) {
	err = (int)(C.reapi_cli_cancel((*C.struct_reapi_cli_ctx)(ctx),
		(C.ulong)(jobid),
		(C.bool)(noent_ok)))
	return
}
