#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include <json/json.h>

#include "cmb.h"

#include "json-lua.h"
#include "lutil.h"

static int l_kvsdir_commit (lua_State *L);

static kvsdir_t lua_get_kvsdir (lua_State *L, int index)
{
    kvsdir_t *dirp = luaL_checkudata (L, index, "CMB.kvsdir");
    return (*dirp);
}

int l_kvsdir_instantiate (lua_State *L)
{
    luaL_getmetatable (L, "CMB.kvsdir");
    lua_setmetatable (L, -2);
    return (1);
}

static int l_kvsdir_destroy (lua_State *L)
{
    kvsdir_t d = lua_get_kvsdir (L, -1);
    if (d)
        kvsdir_destroy (d);
    return (0);
}

int l_push_kvsdir (lua_State *L, kvsdir_t dir)
{
    kvsdir_t *new = lua_newuserdata (L, sizeof (*new));
    *new = dir;
    return l_kvsdir_instantiate (L);
}

static int l_kvsdir_kvsdir_new (lua_State *L)
{
    const char *key;
    kvsdir_t new;
    kvsdir_t d;

    d = lua_get_kvsdir (L, 1);
    key = luaL_checkstring (L, 2);

    if (kvsdir_get_dir (d, &new, key) < 0)
        return lua_pusherror (L, "kvsdir_get_dir: %s", strerror (errno));

    return l_push_kvsdir (L, new);
}

static int l_kvsdir_tostring (lua_State *L)
{
    kvsdir_t d = lua_get_kvsdir (L, 1);
    lua_pushstring (L, kvsdir_key (d));
    return (1);
}

static int l_kvsdir_newindex (lua_State *L)
{
    int rc;
    kvsdir_t d = lua_get_kvsdir (L, 1);
    const char *key = lua_tostring (L, 2);

    /*
     *  Process value;
     */
    if (lua_isnil (L, 3))
        rc = kvsdir_put (d, key, NULL);
    else if (lua_isnumber (L, 3)) {
        double val = lua_tonumber (L, 3);
        if (floor (val) == val)
            rc = kvsdir_put_int64 (d, key, (int64_t) val);
        else
            rc = kvsdir_put_double (d, key, val);
    }
    else if (lua_isboolean (L, 3))
        rc = kvsdir_put_boolean (d, key, lua_toboolean (L, 3));
    else if (lua_isstring (L, 3))
        rc = kvsdir_put_string (d, key, lua_tostring (L, 3));
    else if (lua_istable (L, 3)) {
        rc = kvsdir_put (d, key, lua_value_to_json (L, 3));
    }
    else {
        return luaL_error (L, "Unsupported type for kvs assignment: %s",
                            lua_typename (L, lua_type (L, 3)));
    }
    if (rc < 0)
        return lua_pusherror (L, "kvsdir_put (key=%s, type=%s): %s",
                           key, lua_typename (L, lua_type (L, 3)),
                           strerror (errno));
    return (0);
}

static kvsitr_t lua_to_kvsitr (lua_State *L, int index)
{
    kvsitr_t *iptr = luaL_checkudata (L, index, "CMB.kvsitr");
    return (*iptr);
}

/* gc metamethod for iterator */
static int l_kvsitr_destroy (lua_State *L)
{
    kvsitr_t i = lua_to_kvsitr (L, 1);
    kvsitr_destroy (i);
    return (0);
}

static int l_kvsdir_iterator (lua_State *L)
{
    const char *key;
    kvsitr_t i;

    /* Get kvsitr from upvalue index on stack:
     */
    i = lua_to_kvsitr (L, lua_upvalueindex (1));
    if (i == NULL)
        return (luaL_error (L, "Invalid kvsdir iterator"));

    if ((key = kvsitr_next (i))) {
        lua_pushstring (L, key);
        return (1);
    }

    /* No more keys, return nil */
    return (0);
}

static int l_kvsdir_next (lua_State *L)
{
    kvsdir_t d = lua_get_kvsdir (L, 1);
    kvsitr_t *iptr;

    lua_pop (L, 1);

    /* Push a kvsitr_t onto top of stack and set its metatable.
     */
    iptr = lua_newuserdata (L, sizeof (*iptr));
    *iptr = kvsitr_create (d);
    luaL_getmetatable (L, "CMB.kvsitr");
    lua_setmetatable (L, -2);

    /* Return iterator function as C closure, with 'iterator, nil, nil'
     *  [ iterator, state, starting value ].  Starting value is always
     *  nil, and we push nil state here because our state is
     *  encoded in the kvsitr of the closure.
     */
    lua_pushcclosure (L, l_kvsdir_iterator, 1);
    return (1);
}

static int l_kvsdir_commit (lua_State *L)
{
    kvsdir_t d = lua_get_kvsdir (L, 1);
    if (lua_isnoneornil (L, 2)) {
        if (kvs_commit (kvsdir_handle (d)) < 0)
            return lua_pusherror (L, "kvs_commit: %s", strerror (errno));
    }
    lua_pushboolean (L, true);
    return (1);
}

static int l_kvsdir_watch (lua_State *L)
{
    int rc;
    void *h;
    char *key;
    json_object *o;
    kvsdir_t dir;

    dir = lua_get_kvsdir (L, 1);
    h = kvsdir_handle (dir);
    key = kvsdir_key_at (dir, lua_tostring (L, 2));

    if (lua_isnoneornil (L, 3)) {
        /* Need to fetch initial value */
        if (((rc = kvs_get (h, key, &o)) < 0) && (errno != ENOENT))
            goto err;
    }
    else {
        /*  Otherwise, the alue at top of stack is initial json_object */
        o = lua_value_to_json (L, -1);
    }

    rc = kvs_watch_once (h, key, &o);
err:
    free (key);
    if (rc < 0)
        return lua_pusherror (L, "kvs_watch: %s", strerror (errno));

    json_object_to_lua (L, o);
    json_object_put (o);
    return (1);
}

static int l_kvsdir_watch_dir (lua_State *L)
{
    flux_t h;
    kvsdir_t dir;

    dir = lua_get_kvsdir (L, 1);
    h = kvsdir_handle (dir);

    return l_pushresult (L, kvs_watch_once_dir (h, &dir, kvsdir_key (dir)));
}

static int l_kvsdir_index (lua_State *L)
{
    int rc;
    flux_t f;
    kvsdir_t d;
    const char *key = lua_tostring (L, 2);
    char *fullkey = NULL;
    json_object *o = NULL;

    if (key == NULL)
        return luaL_error (L, "kvsdir: invalid index");
    /*
     *  To support indeces like kvsdir ["a.relative.path"] we have
     *   to pretend that kvsdir objects support this kind of
     *   non-local indexing by using full paths and flux handle :-(
     */
    d = lua_get_kvsdir (L, 1);
    f = kvsdir_handle (d);
    fullkey = kvsdir_key_at (d, key);

    if (kvs_get (f, fullkey, &o) == 0)
        rc = json_object_to_lua (L, o);
    else if (errno == EISDIR)
        rc = l_kvsdir_kvsdir_new (L);
    else {
        /* No key. First check metatable to see if this is a method */
        lua_getmetatable (L, 1);
        lua_getfield (L, -1, key);

        /* If not then return error */
        if (lua_isnil (L, -1)) {
             rc = lua_pusherror (L, "Key not found.");
            goto out;
        }
        rc = 1;
    }
out:
    if (o)
        json_object_put (o);
    free (fullkey);
    return (rc);
}

#if 0
static const struct luaL_Reg kvsdir_functions [] = {
    { "commit",          l_kvsdir_commit    },
    { "keys",            l_kvsdir_next      },
    { "watch",           l_kvsdir_watch     },
    { "watch_dir",       l_kvsdir_watch_dir },
    { NULL,              NULL               },
};
#endif

static const struct luaL_Reg kvsitr_methods [] = {
    { "__gc",           l_kvsitr_destroy   },
    { NULL,              NULL              }
};

static const struct luaL_Reg kvsdir_methods [] = {
    { "__gc",            l_kvsdir_destroy  },
    { "__index",         l_kvsdir_index    },
    { "__newindex",      l_kvsdir_newindex },
    { "__tostring",      l_kvsdir_tostring },
    { "commit",          l_kvsdir_commit   },
    { "keys",            l_kvsdir_next     },
    { "watch",           l_kvsdir_watch    },
    { "watch_dir",       l_kvsdir_watch_dir},
    { NULL,              NULL              }
};

int l_kvsdir_register_metatable (lua_State *L)
{
    luaL_newmetatable (L, "CMB.kvsitr");
    luaL_register (L, NULL, kvsitr_methods);
    luaL_newmetatable (L, "CMB.kvsdir");
    luaL_register (L, NULL, kvsdir_methods);
    return (1);
}

int luaopen_kvs (lua_State *L)
{
    l_kvsdir_register_metatable (L);
    //luaL_register (L, "kvsdir", kvsdir_functions);
    return (1);
}

/*
 * vi: ts=4 sw=4 expandtab
 */
