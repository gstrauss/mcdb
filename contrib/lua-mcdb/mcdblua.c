/*
 * lua-mcdb - Lua interface to create and read mcdb constant databases
 *
 * Copyright (c) 2011, Glue Logic LLC. All rights reserved. code()gluelogic.com
 *
 *  This file is part of lua-mcdb.
 *
 *  lua-mcdb is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  lua-mcdb is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with lua-mcdb.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * mcdb is originally based upon the Public Domain cdb-0.75 by Dan Bernstein
 *
 * The Lua online doc was instrumental to me while writing this extension.
 * Thank you!
 *   http://www.lua.org/manual/5.1/manual.html
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lua.h"
#include "lauxlib.h"

#include <mcdb/mcdb.h>

#define MCDBLUA "mcdb"

struct mcdblua {
    struct mcdb m;            /* must be first element for our implicit casts */
    const char *findkey;      /* used by findnext() */
    uint32_t findklen;        /* used by findnext() */
};

static int
mcdblua_size(lua_State * const restrict L)
{
    struct mcdb * const restrict m = luaL_checkudata(L, 1, MCDBLUA);
    lua_pushnumber(L, (int)mcdb_numrecs(m));
    return 1;
}

static int
mcdblua_contains(lua_State * const restrict L)
{
    size_t klen;
    struct mcdb * const restrict m = luaL_checkudata(L, 1, MCDBLUA);
    const char * const restrict k = lua_tolstring(L, 2, &klen);
    if (mcdb_find(m, k, klen)) {
        lua_pushnil(L);
        return 1;
    }
    return 0;
}

static int
mcdblua_get(lua_State * const restrict L)
{
    size_t klen;
    struct mcdb * const restrict m = luaL_checkudata(L, 1, MCDBLUA);
    const char * const restrict k = luaL_checklstring(L, 2, &klen);
    mcdb_find(m, k, klen)
      ? lua_pushlstring(L, (char *)mcdb_dataptr(m), mcdb_datalen(m))
      : lua_pushnil(L);
    return 1;
}

static int
mcdblua_find(lua_State * const restrict L)
{
    size_t klen;
    struct mcdblua * const restrict mlua = luaL_checkudata(L, 1, MCDBLUA);
    const char * const restrict k = luaL_checklstring(L, 2, &klen);
    mlua->findkey = NULL;
    if (mcdb_find(&mlua->m, k, klen)) {
        mlua->findkey = (char *)mcdb_keyptr(&mlua->m, (mlua->findklen = klen));
        lua_pushlstring(L, (char *)mcdb_dataptr(&mlua->m),
                                   mcdb_datalen(&mlua->m));
        return 1;
    }
    return 0;
}

static int
mcdblua_findnext(lua_State * const restrict L)
{
    struct mcdblua * const restrict mlua = luaL_checkudata(L, 1, MCDBLUA);
    if (mlua->findkey
        && mcdb_findnext(&mlua->m, mlua->findkey, mlua->findklen)) {
        lua_pushlstring(L, (char *)mcdb_dataptr(&mlua->m),
                                   mcdb_datalen(&mlua->m));
        return 1;
    }
    else {
        if (mlua->findkey) {
            mlua->findkey = NULL;
            return 0;
        }
        else
            return
              luaL_error(L, "findnext() called without first calling find()");
    }
}

static int
mcdblua_findall(lua_State * const restrict L)
{
    size_t klen;
    struct mcdb * const restrict m = luaL_checkudata(L, 1, MCDBLUA);
    const char * const restrict k = luaL_checklstring(L, 2, &klen);
    int n = 0;
    if (mcdb_findstart(m, k, klen)) {
        while (mcdb_findnext(m, k, klen)) {
            if (++n > LUA_MINSTACK - 2)
                lua_checkstack(L, 1);
            lua_pushlstring(L, (char *)mcdb_dataptr(m), mcdb_datalen(m));
        }
    }
    return n;
}

static int
mcdblua_getseq(lua_State * const restrict L)
{
    size_t klen;
    struct mcdblua * const restrict mlua = luaL_checkudata(L, 1, MCDBLUA);
    const char * const restrict k = luaL_checklstring(L, 2, &klen);
    int seq = luaL_optint(L, 3, 0);
    bool rc;
    if (mcdb_findstart(&mlua->m, k, klen)) {
        while ((rc = mcdb_findnext(&mlua->m, k, klen)) && seq--)
            ;
        if (rc) {
            mlua->findkey = (char *)mcdb_keyptr(&mlua->m,(mlua->findklen=klen));
            lua_pushlstring(L, (char *)mcdb_dataptr(&mlua->m),
                                       mcdb_datalen(&mlua->m));
            return 1;
        }
    }
    mlua->findkey = NULL;
    return 0;
}

static int
mcdblua_iterkeys_closure(lua_State * const restrict L)
{
    struct mcdb_iter * const restrict iter =
      lua_touserdata(L, lua_upvalueindex(1));
    if (mcdb_iter(iter)) {
        lua_pushlstring(L, (char *)mcdb_iter_keyptr(iter),
                                   mcdb_iter_keylen(iter));
        return 1;
    }
    return 0;
}

static int
mcdblua_iterkeys(lua_State * const restrict L)
{
    struct mcdb * const restrict m = luaL_checkudata(L, 1, MCDBLUA);
    struct mcdb_iter * const restrict iter =
      lua_newuserdata(L, sizeof(struct mcdb_iter));
    mcdb_iter_init(iter, m);
    lua_pushcclosure(L, mcdblua_iterkeys_closure, 1);
    return 1;
}

static int
mcdblua_itervalues_closure(lua_State * const restrict L)
{
    struct mcdb_iter * const restrict iter =
      lua_touserdata(L, lua_upvalueindex(1));
    if (mcdb_iter(iter)) {
        lua_pushlstring(L, (char *)mcdb_iter_dataptr(iter),
                                   mcdb_iter_datalen(iter));
        return 1;
    }
    return 0;
}

static int
mcdblua_itervalues(lua_State * const restrict L)
{
    struct mcdb * const restrict m = luaL_checkudata(L, 1, MCDBLUA);
    struct mcdb_iter * const restrict iter =
      lua_newuserdata(L, sizeof(struct mcdb_iter));
    mcdb_iter_init(iter, m);
    lua_pushcclosure(L, mcdblua_itervalues_closure, 1);
    return 1;
}

static int
mcdblua_iteritems_closure(lua_State * const restrict L)
{
    struct mcdb_iter * const restrict iter =
      lua_touserdata(L, lua_upvalueindex(1));
    if (mcdb_iter(iter)) {
        lua_pushlstring(L, (char *)mcdb_iter_keyptr(iter),
                                   mcdb_iter_keylen(iter));
        lua_pushlstring(L, (char *)mcdb_iter_dataptr(iter),
                                   mcdb_iter_datalen(iter));
        return 2;
    }
    return 0;
}

static int
mcdblua_iteritems(lua_State * const restrict L)
{
    struct mcdb * const restrict m = luaL_checkudata(L, 1, MCDBLUA);
    struct mcdb_iter * const restrict iter =
      lua_newuserdata(L, sizeof(struct mcdb_iter));
    mcdb_iter_init(iter, m);
    lua_pushcclosure(L, mcdblua_iteritems_closure, 1);
    return 1;
}

static int
mcdblua_keys(lua_State * const restrict L)
{
    /* table returned is unsorted array
     * keys duplicated in mcdb are duplicated in table */
    struct mcdb * const restrict m = luaL_checkudata(L, 1, MCDBLUA);
    struct mcdb_iter iter;
    int n = 0;
    lua_createtable(L, mcdb_numrecs(m), 0);
    mcdb_iter_init(&iter, m);
    while (mcdb_iter(&iter)) {
        lua_pushlstring(L, (char *)mcdb_iter_keyptr(&iter),
                                   mcdb_iter_keylen(&iter));
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}

static int
mcdblua_values(lua_State * const restrict L)
{
    /* table returned is unsorted array of all values in mcdb */
    struct mcdb * const restrict m = luaL_checkudata(L, 1, MCDBLUA);
    struct mcdb_iter iter;
    int n = 0;
    lua_createtable(L, mcdb_numrecs(m), 0);
    mcdb_iter_init(&iter, m);
    while (mcdb_iter(&iter)) {
        lua_pushlstring(L, (char *)mcdb_iter_dataptr(&iter),
                                   mcdb_iter_datalen(&iter));
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}

static int
mcdblua_items(lua_State * const restrict L)
{
    /* table returned is associative array (hash) of (key,value) pairs in mcdb
     * Associative array contains only one value per key
     * Note: keys duplicated in mcdb have all but last (key,value) discarded
     * (Use iteritems() to iterate over all (key,value) pairs in mcdb) */
    struct mcdb * const restrict m = luaL_checkudata(L, 1, MCDBLUA);
    struct mcdb_iter iter;
    lua_createtable(L, 0, mcdb_numrecs(m));
    mcdb_iter_init(&iter, m);
    while (mcdb_iter(&iter)) {
        lua_pushlstring(L, (char *)mcdb_iter_keyptr(&iter),
                                   mcdb_iter_keylen(&iter));
        lua_pushlstring(L, (char *)mcdb_iter_dataptr(&iter),
                                   mcdb_iter_datalen(&iter));
        lua_rawset(L, -3);
    }
    return 1;
}

static int
mcdblua_name(lua_State * const restrict L)
{
    struct mcdb * const restrict m = luaL_checkudata(L, 1, MCDBLUA);
    lua_pushstring(L, m->map ? m->map->fname : "<mcdb not open>");
    return 1;
}

static int
mcdblua_tostring(lua_State * const restrict L)
{
    struct mcdb * const restrict m = luaL_checkudata(L, 1, MCDBLUA);
    lua_pushfstring(L, "mcdb %p", m);
    return 1;
}

static int
mcdblua_free(lua_State * const restrict L)
{
    struct mcdb * const restrict m = luaL_checkudata(L, 1, MCDBLUA);
    if (m->map) {
        mcdb_mmap_destroy(m->map);
        m->map = NULL;
    }
    lua_pushnil(L);
    lua_setmetatable(L, -2);
    return 0;
}

static int
mcdblua_init(lua_State * const restrict L)
{
    const char * const restrict fn = luaL_checkstring(L, 1);
    struct mcdb_mmap * restrict map;
    if ((map = mcdb_mmap_create(NULL, NULL, fn, malloc, free))) {
        struct mcdblua * const restrict mlua = 
          lua_newuserdata(L, sizeof(struct mcdblua));
        mlua->m.map = map;
        mlua->findkey = NULL;
        luaL_getmetatable(L, MCDBLUA);

      #if 0  /* disable custom __index */
        lua_pushliteral(L, "__index");  /* lua table query t[x] */
        lua_pushcclosure(L, mcdblua_get, 0);
        lua_rawset(L, -3);
      #endif /* lua overloads __index; insufficient encapsulation */
      #if 0  /* identical to above, but runs __newindex metamethod if present */
        lua_pushcclosure(L, mcdblua_get, 0);
        lua_setfield(L, -2, "__index");
      #endif
        lua_pushliteral(L, "__call");/* lua table query t(x); in lieu of t[x] */
        lua_pushcclosure(L, mcdblua_get, 0);
        lua_rawset(L, -3);
        lua_pushliteral(L, "__gc");
        lua_pushcclosure(L, mcdblua_free, 0);
        lua_rawset(L, -3);
        lua_pushliteral(L, "__len");
        lua_pushcclosure(L, mcdblua_size, 0);
        lua_rawset(L, -3);
        lua_pushliteral(L, "__tostring");
        lua_pushcclosure(L, mcdblua_tostring, 0);
        lua_rawset(L, -3);

        lua_setmetatable(L, -2);
        return 1;
    }
    else {
        const int errnum = errno;
        lua_pushnil(L);
        lua_pushstring(L, strerror(errnum));
        return 2;
    }
}

static const struct luaL_Reg mcdblua_methods[] = {
  { "free",       mcdblua_free },
  { "getn",       mcdblua_size },
  { "size",       mcdblua_size },
  { "contains",   mcdblua_contains },
  { "find",       mcdblua_find },
  { "findnext",   mcdblua_findnext },
  { "findall",    mcdblua_findall },
  { "getseq",     mcdblua_getseq },
  { "iter",       mcdblua_iterkeys },
  { "iterkeys",   mcdblua_iterkeys },
  { "itervalues", mcdblua_itervalues },
  { "iteritems",  mcdblua_iteritems },
  { "pairs",      mcdblua_iteritems },
  { "keys",       mcdblua_keys },
  { "values",     mcdblua_values },
  { "items",      mcdblua_items },
  { "name",       mcdblua_name },
  { NULL, NULL }
};

static const struct luaL_Reg mcdblua_meta[] = {
  { "init",       mcdblua_init },
  { NULL, NULL }
};

int
luaopen_mcdb(lua_State * const restrict L)
{
    lua_newtable(L);
    luaL_register(L, NULL, mcdblua_meta);
    luaL_newmetatable(L, MCDBLUA);
    luaL_register(L, MCDBLUA, mcdblua_methods);
    lua_pushliteral(L, "__metatable");
    lua_pushvalue(L, -2);
    lua_rawset(L, -3);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_rawset(L, -4);
    lua_setmetatable(L, -2);
    lua_pop(L, 1);
    return 1;
}
