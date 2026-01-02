#include "nterm.h"
#include <lauxlib.h>
#include <string.h>
#include <stdlib.h>

static int nb_lua_eval(lua_State *L) {
    const char *code = luaL_checkstring(L, 1);
    size_t len = strlen(code);
    char *wrapped_code = (char*)malloc(len + 10);

    if (!wrapped_code) {
        lua_pushstring(L, "nterm.eval: Out of memory for expression wrapping.");
        return lua_error(L);
    }

    strcpy(wrapped_code, "return (");
    strcat(wrapped_code, code);
    strcat(wrapped_code, ")");
    int stack_top_before = lua_gettop(L);

    if (luaL_dostring(L, wrapped_code)) {
        // failed so clean up memory and then propagate error
        free(wrapped_code);
        return lua_error(L);
    }
    free(wrapped_code);

    return lua_gettop(L) - stack_top_before;
}


static int nb_run_script(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);

    if (luaL_dofile(L, filename)) {
        return lua_error(L);
    }

    // don't tell me if its a script
    return 0;
}

// note to self - stop redeclaring stuff

static const nterm_reg_t LUA_FUNCS[] = {
    {"run", nb_run_script},
    {"eval", nb_lua_eval},
    {NULL, NULL}
};

void nterm_register_lua_tools(lua_State *L) {
    for (const nterm_reg_t *r = LUA_FUNCS; r->name; ++r) {
        lua_pushcfunction(L, r->fn);
        lua_setfield(L, -2, r->name);
    }
}
