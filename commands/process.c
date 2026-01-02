// execute stuff
// because this is a shell
// NOTE; python is still mostly broken
#include "nterm.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libndls.h>

#ifndef LUA_OK
#define LUA_OK 0
#endif

#ifndef MP_HEAP_SIZE
#define MP_HEAP_SIZE (512 * 1024)
#endif
#include "py/stackctrl.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/compile.h"
#include "py/obj.h"
#include "py/lexer.h"
#include "py/parse.h"
#include "py/misc.h"
#include "py/mpstate.h"
#include "py/qstr.h"
#include "py/stream.h"
#include "py/nlr.h"
#include "py/parsehelper.h"

static int g_last_exec_rc = 0;
static void my_mp_print(void *env, const char *fmt, ...) {
    (void)env;
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

void nlr_jump_fail(void *val) {
    fprintf(stderr, "FATAL: uncaught NLR %p\n", val);
    g_last_exec_rc = 1;
}


static int has_ext_icase(const char *path, const char *ext) {
    size_t lp = strlen(path), le = strlen(ext);
    if (lp < le) return 0;
    const char *tail = path + (lp - le);
    for (size_t i = 0; i < le; ++i) {
        char a = tail[i], b = ext[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return 0;
    }
    return 1;
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}


static void push_tostring(lua_State *L, int idx) {
    if (idx < 0) idx = lua_gettop(L) + 1 + idx; // absolute index
    lua_getglobal(L, "tostring"); // ..., tostring
    lua_pushvalue(L, idx);        // ..., tostring, value
    lua_call(L, 1, 1);            // ..., "string"
}

static int read_file_all(const char *path, char **out, size_t *out_sz) {
    *out = NULL; *out_sz = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return 0; }
    rewind(f);
    if (sz == 0) { fclose(f); return 1; }
    char *buf = (char*)malloc((size_t)sz);
    if (!buf) { fclose(f); return 0; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(buf); return 0; }
    *out = buf; *out_sz = (size_t)sz;
    return 1;
}

static int run_lua_file(lua_State *L, const char *path, int first_arg_index, int top_before) {
    int argc = (top_before - first_arg_index + 1);

    lua_getglobal(L, "arg");
    int had_old_arg = !lua_isnil(L, -1);
    int old_arg_ref = LUA_NOREF;
    if (had_old_arg) {
        old_arg_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
        lua_pop(L, 1);
    }
    lua_newtable(L);
    int argtbl = lua_gettop(L);

    lua_pushstring(L, path);
    lua_rawseti(L, argtbl, 0);
    for (int i = 0; i < argc; ++i) {
        if (lua_type(L, first_arg_index + i) == LUA_TSTRING) {
            size_t len; const char *s = lua_tolstring(L, first_arg_index + i, &len);
            lua_pushlstring(L, s, len);
        } else {
            push_tostring(L, first_arg_index + i);
        }
        lua_rawseti(L, argtbl, i + 1);
    }
    lua_pushinteger(L, argc);
    lua_setfield(L, argtbl, "n");
    lua_setglobal(L, "arg");

    // READ IT YOU FOOL
    char *buf = NULL; size_t sz = 0;
    if (!read_file_all(path, &buf, &sz)) {
        if (had_old_arg) { lua_rawgeti(L, LUA_REGISTRYINDEX, old_arg_ref); lua_setglobal(L, "arg"); luaL_unref(L, LUA_REGISTRYINDEX, old_arg_ref); }
        else { lua_pushnil(L); lua_setglobal(L, "arg"); }
        g_last_exec_rc = 1;
        return nterm_push_error(L, "cannot read file");
    }

    int status = luaL_loadbuffer(L, buf ? buf : "", sz, path);
    if (buf) free(buf);

    if (status != LUA_OK) {
        if (had_old_arg) { lua_rawgeti(L, LUA_REGISTRYINDEX, old_arg_ref); lua_setglobal(L, "arg"); luaL_unref(L, LUA_REGISTRYINDEX, old_arg_ref); }
        else { lua_pushnil(L); lua_setglobal(L, "arg"); }
        g_last_exec_rc = 1;
        return nterm_push_error(L, lua_tostring(L, -1));
    }

    status = lua_pcall(L, 0, 0, 0);

    if (had_old_arg) { lua_rawgeti(L, LUA_REGISTRYINDEX, old_arg_ref); lua_setglobal(L, "arg"); luaL_unref(L, LUA_REGISTRYINDEX, old_arg_ref); }
    else { lua_pushnil(L); lua_setglobal(L, "arg"); }

    if (status != LUA_OK) {
        g_last_exec_rc = 1;
        return nterm_push_error(L, lua_tostring(L, -1));
    }

    g_last_exec_rc = 0; // success
    lua_pushboolean(L, 1);
    return 1;
}

// use nl_exec to execute .tns files
static int run_tns_exec(lua_State *L, const char *path, int first_arg_index, int top_before) {
    int argsn = (top_before - first_arg_index + 1);
    char **args = NULL;

    if (argsn > 0) {
        args = (char**)malloc(sizeof(char*) * argsn);
        if (!args) return nterm_push_error(L, "out of memory");

        for (int i = 0; i < argsn; ++i) {
            const char *s = NULL; size_t len = 0;

            if (lua_type(L, first_arg_index + i) == LUA_TSTRING) {
                s = lua_tolstring(L, first_arg_index + i, &len);
                args[i] = (char*)malloc(len + 1);
                if (!args[i]) { argsn = i; goto oom; }
                memcpy(args[i], s, len); args[i][len] = '\0';
            } else {
                push_tostring(L, first_arg_index + i);
                s = lua_tolstring(L, -1, &len);
                args[i] = (char*)malloc(len + 1);
                if (!args[i]) { lua_pop(L,1); argsn = i; goto oom; }
                memcpy(args[i], s, len); args[i][len] = '\0';
                lua_pop(L, 1);
            }
        }
    }

    int rc = nl_exec(path, argsn, args);

    for (int i = 0; i < argsn; ++i) free(args[i]);
    free(args);

    g_last_exec_rc = rc;
    lua_pushboolean(L, 1);
    return 1;

oom:
    for (int i = 0; i < argsn; ++i) if (args[i]) free(args[i]);
    free(args);
    g_last_exec_rc = 1;
    return nterm_push_error(L, "out of memory");
}

// the pythony bits
//NOTE; repeat of message at the top but this is mostly broken
static char *py_heap;


static int run_py_exec(lua_State *L, const char *path, int first_arg_index, int top_before) {
    mp_obj_list_init(mp_sys_argv, 0);
    mp_obj_list_append(mp_sys_argv, MP_OBJ_NEW_QSTR(qstr_from_str(path)));
    int argc = top_before - first_arg_index + 1;
    for (int i = 0; i < argc; ++i) {
        push_tostring(L, first_arg_index + i);
        const char *arg_str = lua_tostring(L, -1);
        mp_obj_list_append(mp_sys_argv, MP_OBJ_NEW_QSTR(qstr_from_str(arg_str)));
        lua_pop(L, 1);
    }
    mp_lexer_t *lex = mp_lexer_new_from_file(path);
    if (lex == NULL) {
        g_last_exec_rc = 1;
        return nterm_push_error(L, "File not found or cannot be read.");
    }

    int ret_code = 0;
    mp_parse_error_kind_t parse_error_kind;
    mp_parse_node_t pn = mp_parse(lex, MP_PARSE_FILE_INPUT, &parse_error_kind);

    if (pn == MP_PARSE_NODE_NULL) {
        mp_parse_show_exception(lex, parse_error_kind);
        ret_code = 1;
    } else {
        mp_obj_t module_fun = mp_compile(pn, qstr_from_str(path), MP_EMIT_OPT_NONE, false);
        if (module_fun == mp_const_none) {
            ret_code = 1;
        } else {
            nlr_buf_t nlr;
            if (nlr_push(&nlr) == 0) {
                mp_call_function_0(module_fun);
                nlr_pop();
            } else {
                mp_obj_t exc = (mp_obj_t)nlr.ret_val;
                if (mp_obj_is_subclass_fast(mp_obj_get_type(exc), &mp_type_SystemExit)) {
                    ret_code = mp_obj_get_int(mp_obj_exception_get_value(exc));
                } else {
                    mp_obj_print_exception(my_mp_print, NULL, exc);
                    ret_code = 1;
                }
            }
        }
    }

    fflush(stdout);
    fflush(stderr);

    g_last_exec_rc = ret_code;
    lua_pushboolean(L, 1);
    return 1;
}

void nterm_init_py_vm() {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    #if defined(mp_stack_ctrl_init)
    mp_stack_ctrl_init();
    #endif
    mp_stack_set_limit(32768);

    py_heap = (char*)malloc(MP_HEAP_SIZE);
    if (!py_heap) {
        fprintf(stderr, "FATAL: Failed to allocate MicroPython heap.\n");
        return;
    }
    gc_init(py_heap, py_heap + MP_HEAP_SIZE - 1);
    mp_init();
    mp_obj_list_init(mp_sys_path, 0);
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR_));
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(qstr_from_str("/documents/ndless")));
    mp_obj_list_init(mp_sys_argv, 0);
}

mp_import_stat_t mp_import_stat(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode))  return MP_IMPORT_STAT_DIR;
        if (S_ISREG(st.st_mode))  return MP_IMPORT_STAT_FILE;
    }
    return MP_IMPORT_STAT_NO_EXIST;
}

static int nb_dofile(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    return run_lua_file(L, path, lua_gettop(L) + 1, lua_gettop(L));
}

static int nb_exec(lua_State *L) {
    int top = lua_gettop(L);
    if (top < 1) return nterm_push_error(L, "usage: exec(path, [args...])");
    const char *path = luaL_checkstring(L, 1);

    if (!file_exists(path)) {
        g_last_exec_rc = 1;
        return nterm_push_error(L, "file not found");
    }

    if (has_ext_icase(path, ".lua") || has_ext_icase(path, "nspirerc")) {
        return run_lua_file(L, path, 2, top);
    }
    if (has_ext_icase(path, ".tns")) {
        return run_tns_exec(L, path, 2, top);
    }
    if (has_ext_icase(path, ".py")) {
        return run_py_exec(L, path, 2, top);
    }
    g_last_exec_rc = 1;
    return nterm_push_error(L, "unsupported file type (only .lua, .tns, and .py)");
}

static int nb_proc_info(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    FILE *f = fopen(path, "rb");
    if (!f) {
        return nterm_push_error(L, "File not found");
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return nterm_push_error(L, "Seek failed");
    }
    long size = ftell(f);
    fclose(f);

    lua_newtable(L);

    lua_pushstring(L, path);
    lua_setfield(L, -2, "path");

    lua_pushinteger(L, size >= 0 ? size : 0);
    lua_setfield(L, -2, "size");

    const char *type = "file";
    if (has_ext_icase(path, ".lua")) type = "lua";
    else if (has_ext_icase(path, ".tns")) type = "tns";
    else if (has_ext_icase(path, ".py")) type = "python";
    lua_pushstring(L, type);
    lua_setfield(L, -2, "type");

    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "exists");

    return 1;
}

static int nb_list_tasks(lua_State *L) {
    lua_newtable(L);
    lua_pushstring(L, "Task listing not implemented");
    lua_setfield(L, -2, "error");
    return 1;
}

static int nb_kill_task(lua_State *L) {
    (void)luaL_checkstring(L, 1);
    return nterm_push_error(L, "Task killing not implemented");
}

static int nb_is_executable(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    if (has_ext_icase(path, ".lua") || has_ext_icase(path, "nspirerc")) { lua_pushboolean(L, 1); return 1; }
    if (has_ext_icase(path, ".tns")) { lua_pushboolean(L, 1); return 1; }
    if (has_ext_icase(path, ".py"))  { lua_pushboolean(L, 1); return 1; }

    FILE *f = fopen(path, "rb");
    if (!f) { lua_pushboolean(L, 0); return 1; }
    unsigned char h[4] = {0};
    size_t r = fread(h, 1, 4, f);
    fclose(f);
    if (r == 4 && h[0]==0x7F && h[1]=='E' && h[2]=='L' && h[3]=='F') {
        lua_pushboolean(L, 1);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int nb_sleep(lua_State *L) {
    int ms = luaL_checkinteger(L, 1);
    if (ms < 0) return nterm_push_error(L, "Sleep time cannot be negative");

    clock_t start = clock();
    if (CLOCKS_PER_SEC <= 0) {
        for (volatile int i = 0; i < ms * 10000; ++i) {}
        lua_pushboolean(L, 1);
        return 1;
    }
    while (((clock() - start) * 1000 / CLOCKS_PER_SEC) < (clock_t)ms) {}
    lua_pushboolean(L, 1);
    return 1;
}

static int nb_last_status(lua_State *L) {
    lua_pushinteger(L, g_last_exec_rc);
    return 1;
}

/* --- Registration --- */
static const nterm_reg_t PROCESS_FUNCS[] = {
    {"exec",          nb_exec},
    {"dofile",        nb_dofile},
    {"proc_info",     nb_proc_info},
    {"list_tasks",    nb_list_tasks},
    {"kill_task",     nb_kill_task},
    {"is_executable", nb_is_executable},
    {"sleep",         nb_sleep},
    {"last_status",   nb_last_status},
    {NULL, NULL}
};

void nterm_register_process(lua_State *L) {
    nterm_init_py_vm();

    for (const nterm_reg_t *r = PROCESS_FUNCS; r->name; ++r) {
        lua_pushcfunction(L, r->fn);
        lua_setfield(L, -2, r->name);
    }
}
