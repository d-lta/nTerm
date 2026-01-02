#include "nterm.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#define DFLT_HOME "/documents"

// basic filesystem stuff that should probably be in io.c but whatever

int get_cwd(lua_State *L) {
    // where the heck are we right now
    char buf[1024];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        lua_pushstring(L, DFLT_HOME);  // couldn't get it, just say /documents
    } else {
        lua_pushstring(L, buf);
    }
    return 1;
}

int ch_dir(lua_State *L) {
    // change directory please
    const char *path = luaL_checkstring(L, 1);
    if (chdir(path) == 0) {
        lua_pushboolean(L, 1);
        return 1;
    }
    return nterm_push_error(L, strerror(errno));  // nope
}

int list_dir(lua_State *L) {
    // list what's in a directory
    const char *path = luaL_checkstring(L, 1);
    DIR *dir = opendir(path);
    if (!dir) return nterm_push_error(L, strerror(errno));

    lua_newtable(L);
    int i = 1;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        // get the full path so we can stat it
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0) continue;  // can't stat it? aw man
        lua_newtable(L);
        lua_pushstring(L, "name");  lua_pushstring(L, ent->d_name); lua_settable(L, -3);
        lua_pushstring(L, "size");  lua_pushnumber(L, (lua_Number)st.st_size); lua_settable(L, -3);
        lua_pushstring(L, "isdir"); lua_pushboolean(L, S_ISDIR(st.st_mode));  lua_settable(L, -3);
        lua_pushstring(L, "mtime"); lua_pushnumber(L, (lua_Number)st.st_mtim.tv_sec); lua_settable(L, -3);

        lua_rawseti(L, -2, i++);
    }
    closedir(dir);
    return 1;
}
