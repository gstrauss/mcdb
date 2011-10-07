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

#include <mcdb/mcdb_make.h>
#include <mcdb/mcdb_makefn.h>

#define MCDBLUA_MAKE "mcdb_make"

static int
mcdblua_make_add(lua_State * const restrict L)
{
    struct mcdb_make * const restrict mk = luaL_checkudata(L, 1, MCDBLUA_MAKE);
    size_t klen, dlen;
    const char * const k = luaL_checklstring(L, 2, &klen);
    const char * const d = luaL_checklstring(L, 3, &dlen);
    if (mcdb_make_add(mk, k, klen, d, dlen) == 0)
        return 0;
    else {
        lua_pushstring(L, strerror(errno));
        return 1;
    }
}

static int
mcdblua_make_newindex(lua_State * const restrict L)
{
    return (mcdblua_make_add(L) == 0)
      ? 0
      : luaL_error(L, "mcdb assignment failed: %s", lua_tostring(L, -1));
}

static int
mcdblua_make_finish(lua_State * const restrict L)
{
    struct mcdb_make * const restrict mk = luaL_checkudata(L, 1, MCDBLUA_MAKE);
    int rv;
    if (mcdb_make_finish(mk) == 0 && mcdb_makefn_finish(mk, true) == 0)
        rv = 0;
    else {
        lua_pushstring(L, strerror(errno));
        mcdb_make_destroy(mk);
        rv = 1;
    }
    mcdb_makefn_cleanup(mk);
    lua_pushnil(L);
    lua_setmetatable(L, -2);
    return rv;
}

static int
mcdblua_make_gc(lua_State * const restrict L)
{
    struct mcdb_make * const restrict mk = luaL_checkudata(L, 1, MCDBLUA_MAKE);
    mcdb_make_destroy(mk);
    mcdb_makefn_cleanup(mk);
    lua_pushnil(L);
    lua_setmetatable(L, -2);
    return 0;
}

static int
mcdblua_make_tostring(lua_State * const restrict L)
{
    struct mcdb_make * const restrict mk = luaL_checkudata(L, 1, MCDBLUA_MAKE);
    lua_pushfstring(L, "mcdb_make %p", mk);
    return 1;
}

static int
mcdblua_make_init(lua_State * const restrict L)
{
    const char * const fname = luaL_checkstring(L, 1);
    struct mcdb_make mk;
    if (mcdb_makefn_start(&mk, fname, malloc, free) == 0
        && mcdb_make_start(&mk, mk.fd, malloc, free) == 0) {
        struct mcdb_make * const restrict mklua =
          lua_newuserdata(L, sizeof(struct mcdb_make));
        memcpy(mklua, &mk, sizeof(struct mcdb_make));
        luaL_getmetatable(L, MCDBLUA_MAKE);

        lua_pushliteral(L, "__gc");
        lua_pushcclosure(L, mcdblua_make_gc, 0);
        lua_rawset(L, -3);
        lua_pushliteral(L, "__newindex");  /* lua table assignment t[x] = y */
        lua_pushcclosure(L, mcdblua_make_newindex, 0);
        lua_rawset(L, -3);
        lua_pushliteral(L, "__tostring");
        lua_pushcclosure(L, mcdblua_make_tostring, 0);
        lua_rawset(L, -3);

        lua_setmetatable(L, -2);
        return 1;
    }
    else {
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        mcdb_makefn_cleanup(&mk);
        return 2;
    }
}

static const struct luaL_Reg mcdblua_make_methods[] =
{
  { "add",        mcdblua_make_add },
  { "finish",     mcdblua_make_finish },
  { "cancel",     mcdblua_make_gc },
  { NULL, NULL }
};

static const struct luaL_Reg mcdblua_make_meta[] =
{
  { "init",       mcdblua_make_init },
  { NULL, NULL }
};

int
luaopen_mcdb_make(lua_State * const restrict L)
{
    lua_newtable(L);
    luaL_register(L, NULL, mcdblua_make_meta);
    luaL_newmetatable(L, MCDBLUA_MAKE);
    luaL_register(L, MCDBLUA_MAKE, mcdblua_make_methods);
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
