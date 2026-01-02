#include "nterm.h"
#include <zlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
// compression stuff because we're not savages who store everything uncompressed
static int nb_compress_file(lua_State *L) {
    const char *input_file = luaL_checkstring(L, 1);
    const char *output_file = luaL_checkstring(L, 2);
    
    FILE *in = fopen(input_file, "rb");
    if (!in) {
        lua_pushnil(L);
        lua_pushfstring(L, "Cannot open input file: %s", strerror(errno));
        return 2;
    }
    
    gzFile out = gzopen(output_file, "wb");
    if (!out) {
        fclose(in);
        lua_pushnil(L);
        lua_pushfstring(L, "Cannot create output file: %s", strerror(errno));
        return 2;
    }
    
    char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (gzwrite(out, buffer, bytes) != (int)bytes) {
            fclose(in);
            gzclose(out);
            lua_pushnil(L);
            lua_pushstring(L, "Compression error: gzwrite failed");
            return 2;
        }
    }
    
    fclose(in);
    gzclose(out);
    lua_pushboolean(L, 1);
    return 1;
}


static int nb_decompress_file(lua_State *L) {
    const char *input_file = luaL_checkstring(L, 1);
    const char *output_file = luaL_checkstring(L, 2);
    
    gzFile in = gzopen(input_file, "rb");
    if (!in) {
        lua_pushnil(L);
        lua_pushfstring(L, "Cannot open compressed file: %s", strerror(errno));
        return 2;
    }
    
    FILE *out = fopen(output_file, "wb");
    if (!out) {
        gzclose(in);
        lua_pushnil(L);
        lua_pushfstring(L, "Cannot create output file: %s", strerror(errno));
        return 2;
    }
    
    char buffer[1024];
    int bytes;
    while ((bytes = gzread(in, buffer, sizeof(buffer))) > 0) {
        if (fwrite(buffer, 1, bytes, out) != (size_t)bytes) {
            gzclose(in);
            fclose(out);
            lua_pushnil(L);
            lua_pushstring(L, "Decompression error: fwrite failed");
            return 2;
        }
    }

    if (bytes == -1) {
        const char* gz_error = gzerror(in, NULL);
        lua_pushnil(L);
        lua_pushfstring(L, "Decompression error: %s", gz_error);
        gzclose(in);
        fclose(out);
        return 2;
    }
    
    gzclose(in);
    fclose(out);
    lua_pushboolean(L, 1);
    return 1;
}

static const nterm_reg_t ZIP_FUNCS[] = {
    {"zip", nb_compress_file},
    {"unzip", nb_decompress_file},
    {NULL, NULL}
};

void nterm_register_zip(lua_State *L) {
    for (const nterm_reg_t *r = ZIP_FUNCS; r->name; ++r) {
        lua_pushcfunction(L, r->fn);
        lua_setfield(L, -2, r->name);
    }
}

