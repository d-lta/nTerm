//nano because i would like to write files
// all the ui for this is in the lua file so this is relatively barebones
#include "nterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int nb_readfile(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    FILE *f = fopen(path, "rb");
    if (!f) {
        lua_pushliteral(L, "");
        return 1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return nterm_push_error(L, "Failed to seek");
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return nterm_push_error(L, "Failed to tell size");
    }
    rewind(f);

    char *buf = (char*)malloc((size_t)sz);
    if (!buf && sz > 0) {
        fclose(f);
        return nterm_push_error(L, "Out of memory");
    }

    size_t rd = (sz > 0) ? fread(buf, 1, (size_t)sz, f) : 0;
    fclose(f);

    if (rd != (size_t)sz) {
        free(buf);
        return nterm_push_error(L, "Short read");
    }

    lua_pushlstring(L, buf ? buf : "", rd);
    free(buf);
    return 1;
}

static int nb_nano(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);
    size_t len = 0;
    const char *content = luaL_optlstring(L, 2, "", &len);

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        return nterm_push_error(L, strerror(errno));
    }

    if (len) {
        size_t n = fwrite(content, 1, len, fp);
        if (n != len) {
            fclose(fp);
            return nterm_push_error(L, "Failed to write complete content");
        }
    }

    if (fclose(fp) != 0) {
        return nterm_push_error(L, "Failed to close file");
    }

    lua_pushboolean(L, 1);
    return 1;
}

static const nterm_reg_t NANO_FUNCS[] = {
    {"readfile", nb_readfile},
    {"nano",     nb_nano},
    {NULL,       NULL}
};

void nterm_register_nano(lua_State *L) {
    for (const nterm_reg_t *r = NANO_FUNCS; r->name; ++r) {
        lua_pushcfunction(L, r->fn);
        lua_setfield(L, -2, r->name);
    }
}
