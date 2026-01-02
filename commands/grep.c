#include "nterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#ifndef LUA_OK
#define LUA_OK 0
#endif

// finds stuff

static int nb_grep(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    const char *pattern = luaL_checkstring(L, 2);

    lua_getglobal(L, "nterm");
    if (!lua_istable(L, -1)) {
        return nterm_push_error(L, "nterm table not found");
    }
    lua_getfield(L, -1, "readfile");
    if (!lua_isfunction(L, -1)) {
        return nterm_push_error(L, "nterm.readfile function not found");
    }
    lua_pushstring(L, path);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        return nterm_push_error(L, lua_tostring(L, -1));
    }
    size_t file_len;
    const char *file_content = lua_tolstring(L, -1, &file_len);
    if (!file_content || file_len == 0) {
        lua_pushliteral(L, "");
        return 1;
    }

    luaL_Buffer b;
    luaL_buffinit(L, &b);

    const char *line_start = file_content;
    const char *line_end = file_content;

    while (line_end < file_content + file_len) {
        line_end = strchr(line_start, '\n');
        if (!line_end) {
            line_end = file_content + file_len;
        }

        size_t line_len = line_end - line_start;
        const char *match = strstr(line_start, pattern);

        if (match && (match < line_end || (match == line_start && line_len > 0))) {
            luaL_addlstring(&b, line_start, line_len);
            luaL_addchar(&b, '\n');
        }

        if (line_end == file_content + file_len) {
            break;
        }
        line_start = line_end + 1;
    }
    lua_pop(L, 1); // pop the readfile content
    luaL_pushresult(&b);
    return 1;
}


static const nterm_reg_t GREP_FUNCS[] = {
    {"grep", nb_grep},
    {NULL, NULL}
};

void nterm_register_grep(lua_State *L) {
    for (const nterm_reg_t *r = GREP_FUNCS; r->name; ++r) {
        lua_pushcfunction(L, r->fn);
        lua_setfield(L, -2, r->name);
    }
}
