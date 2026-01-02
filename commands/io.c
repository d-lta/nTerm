#include "nterm.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#define MAX_PATH 1024
#define MAX_MATCHES 1000

typedef struct {
    char    name[256];
    int     isdir;
    off_t   size;
    time_t  mtime;
} entry_t;

typedef struct {
    char **paths;
    int count;
    int capacity;
} path_list_t;

static int cmp_alpha(const void *a, const void *b) {
    const entry_t *ea = (const entry_t*)a, *eb = (const entry_t*)b;
    return strcasecmp(ea->name, eb->name);
}

static int cmp_dirs_first(const void *a, const void *b) {
    const entry_t *ea = (const entry_t*)a, *eb = (const entry_t*)b;
    if (ea->isdir != eb->isdir) return eb->isdir - ea->isdir;
        return strcasecmp(ea->name, eb->name);
}


static void init_path_list(path_list_t *list) {
    list->paths = NULL;
    list->count = 0;
    list->capacity = 0;
}


static int add_path(path_list_t *list, const char *path) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 32;
        char **new_paths = realloc(list->paths, list->capacity * sizeof(char*));
        if (!new_paths) return -1;
        list->paths = new_paths;
    }

    list->paths[list->count] = malloc(strlen(path) + 1);
    if (!list->paths[list->count]) return -1;
    strcpy(list->paths[list->count], path);
    list->count++;
    return 0;
}


static void free_path_list(path_list_t *list) {
    for (int i = 0; i < list->count; i++) {
        free(list->paths[i]);
    }
    free(list->paths);
    init_path_list(list);
}

/* get basename of a path */
static const char *get_basename(const char *path) {
    const char *base = strrchr(path, '/');
    return base ? base + 1 : path;
}

/* get directory part of a path */
static void get_dirname(const char *path, char *dir, size_t dir_size) {
    const char *slash = strrchr(path, '/');
    if (slash && slash != path) {
        size_t len = slash - path;
        if (len >= dir_size) len = dir_size - 1;
        strncpy(dir, path, len);
        dir[len] = '\0';
    } else if (slash == path) {
        strcpy(dir, "/");
    } else {
        strcpy(dir, ".");
    }
}

/* wildcard matching  */
static int simple_match(const char *pattern, const char *string) {
    const char *p = pattern, *s = string;
    const char *star = NULL, *ss = NULL;

    while (*s) {
        if (*p == '?') {
            p++;
            s++;
        } else if (*p == '*') {
            star = p++;
            ss = s;
        } else if (*p == *s) {
            p++;
            s++;
        } else if (star) {
            p = star + 1;
            s = ++ss;
        } else {
            return 0;
        }
    }

    while (*p == '*') p++;
    return *p == '\0';
}

static int expand_wildcards(const char *pattern, path_list_t *matches) {
    char dir[MAX_PATH];
    const char *filename_pattern;

    // current directory references
    if (strcmp(pattern, ".") == 0) {
        return add_path(matches, ".");
    }
    if (strcmp(pattern, "..") == 0) {
        return add_path(matches, "..");
    }

    // check if pattern contains wildcards
    if (strpbrk(pattern, "*?[") == NULL) {
        // no wildcards
        return add_path(matches, pattern);
    }

    get_dirname(pattern, dir, sizeof(dir));
    filename_pattern = get_basename(pattern);

    DIR *d = opendir(dir);
    if (!d) {
        // it don't exist, treat as literal path
        return add_path(matches, pattern);
    }

    struct dirent *ent;
    int found = 0;
    while ((ent = readdir(d)) != NULL) {
        // skip . and .. unless explicitely asked for
        if ((strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) &&
            strcmp(filename_pattern, "*") != 0) {
            continue;
            }

            if (simple_match(filename_pattern, ent->d_name)) {
                char full_path[MAX_PATH];
                if (strcmp(dir, ".") == 0) {
                    snprintf(full_path, sizeof(full_path), "%s", ent->d_name);
                } else {
                    int needed = snprintf(full_path, sizeof(full_path), "%s/%s", dir, ent->d_name);
                    if (needed >= MAX_PATH) {
                        continue;
                    }
                }

                if (add_path(matches, full_path) == 0) {
                    found = 1;
                }
            }
    }
    closedir(d);

    // no matches
    if (!found) {
        return add_path(matches, pattern);
    }

    return 0;
}

static int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return 0;
}

static int recursive_rm(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }

    if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
        return remove(path);
    }

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (!dir) return -1;

        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }

            char fullpath[MAX_PATH];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);

            if (recursive_rm(fullpath) != 0) {
                closedir(dir);
                return -1;
            }
        }
        closedir(dir);

        return rmdir(path);
    }

    return -1;
}


static int copy_file(const char *src, const char *dst) {
    FILE *source = fopen(src, "rb");
    if (!source) return -1;

    FILE *dest = fopen(dst, "wb");
    if (!dest) {
        fclose(source);
        return -1;
    }

    char buf[4096];
    size_t n;
    int success = 1;

    while ((n = fread(buf, 1, sizeof(buf), source)) > 0) {
        if (fwrite(buf, 1, n, dest) != n) {
            success = 0;
            break;
        }
    }

    fclose(source);
    fclose(dest);

    return success ? 0 : -1;
}


static int recursive_cp(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st) != 0) {
        return -1;
    }

    if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
        return copy_file(src, dst);
    }

    if (S_ISDIR(st.st_mode)) {
        if (mkdir(dst, 0755) != 0 && errno != EEXIST) {
            return -1;
        }

        DIR *dir = opendir(src);
        if (!dir) return -1;

        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }

            char src_path[MAX_PATH];
            char dst_path[MAX_PATH];
            snprintf(src_path, sizeof(src_path), "%s/%s", src, ent->d_name);
            snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, ent->d_name);

            if (recursive_cp(src_path, dst_path) != 0) {
                closedir(dir);
                return -1;
            }
        }
        closedir(dir);
        return 0;
    }

    return -1;
}

/* ze core fs functions */

/* finally added wildcards to mv */
static int nb_mv(lua_State *L) {
    const char *source = luaL_checkstring(L, 1);
    const char *dest = luaL_checkstring(L, 2);

    path_list_t sources;
    init_path_list(&sources);

    if (expand_wildcards(source, &sources) != 0) {
        free_path_list(&sources);
        return nterm_push_error(L, "Failed to expand pattern");
    }

    if (sources.count == 0) {
        free_path_list(&sources);
        return nterm_push_error(L, "No matches found");
    }


    if (sources.count > 1 || is_directory(dest)) {
        if (!is_directory(dest)) {
            free_path_list(&sources);
            return nterm_push_error(L, "Cannot move multiple files to non-directory");
        }

        for (int i = 0; i < sources.count; i++) {
            char dest_path[MAX_PATH];
            snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, get_basename(sources.paths[i]));

            if (rename(sources.paths[i], dest_path) != 0) {
                free_path_list(&sources);
                return nterm_push_error(L, strerror(errno));
            }
        }
    } else {
        if (rename(sources.paths[0], dest) != 0) {
            free_path_list(&sources);
            return nterm_push_error(L, strerror(errno));
        }
    }

    free_path_list(&sources);
    lua_pushboolean(L, 1);
    return 1;
}

/* copy time */
static int nb_cp(lua_State *L) {
    const char *source = luaL_checkstring(L, 1);
    const char *dest = luaL_checkstring(L, 2);

    path_list_t sources;
    init_path_list(&sources);

    if (expand_wildcards(source, &sources) != 0) {
        free_path_list(&sources);
        return nterm_push_error(L, "Failed to expand pattern");
    }

    if (sources.count == 0) {
        free_path_list(&sources);
        return nterm_push_error(L, "No matches found");
    }


    if (sources.count > 1 || is_directory(dest)) {
        if (!is_directory(dest)) {
            free_path_list(&sources);
            return nterm_push_error(L, "Cannot copy multiple files to non-directory");
        }

        for (int i = 0; i < sources.count; i++) {
            char dest_path[MAX_PATH];
            snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, get_basename(sources.paths[i]));

            if (recursive_cp(sources.paths[i], dest_path) != 0) {
                free_path_list(&sources);
                return nterm_push_error(L, strerror(errno));
            }
        }
    } else {

        if (recursive_cp(sources.paths[0], dest) != 0) {
            free_path_list(&sources);
            return nterm_push_error(L, strerror(errno));
        }
    }

    free_path_list(&sources);
    lua_pushboolean(L, 1);
    return 1;
}

/* make the directory */
static int nb_mkdir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);


    if (strcmp(path, ".") == 0) {
        lua_pushboolean(L, 1); // yo that exists already
        return 1;
    }

    if (mkdir(path, 0755) == 0) {
        lua_pushboolean(L, 1);
        return 1;
    }
    return nterm_push_error(L, strerror(errno));
}

/* concatenate*/
static int nb_cat(lua_State *L) {
    const char *pattern = luaL_checkstring(L, 1);

    path_list_t files;
    init_path_list(&files);

    if (expand_wildcards(pattern, &files) != 0) {
        free_path_list(&files);
        return nterm_push_error(L, "Failed to expand pattern");
    }

    if (files.count == 0) {
        free_path_list(&files);
        return nterm_push_error(L, "No matches found");
    }

    for (int i = 0; i < files.count; i++) {
        if (is_directory(files.paths[i])) {
            continue;
        }

        FILE *f = fopen(files.paths[i], "r");
        if (!f) {
            continue; // can't open it, sorry
        }

        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            fwrite(buf, 1, n, stdout);
        }
        fclose(f);
    }

    free_path_list(&files);
    lua_pushboolean(L, 1);
    return 1;
}

/* delete - TODO - needs a fake permission model or else somebody is going to brick their calculator */
static int nb_rm(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_getfield(L, 1, "force");
    int opt_force = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "recursive");
    int opt_recursive = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "paths");
    luaL_checktype(L, -1, LUA_TTABLE);

    size_t len = lua_objlen(L, -1);
    for (size_t i = 1; i <= len; i++) {
        lua_rawgeti(L, -1, i);
        const char *pattern = lua_tostring(L, -1);

        path_list_t files;
        init_path_list(&files);

        if (expand_wildcards(pattern, &files) != 0) {
            free_path_list(&files);
            lua_pop(L, 1);
            continue;
        }

        for (int j = 0; j < files.count; j++) {
            const char *path = files.paths[j];

            struct stat st;
            if (stat(path, &st) != 0) {
                if (opt_force) {
                    continue;
                }
                free_path_list(&files);
                return nterm_push_error(L, "No such file or directory");
            }

            if (S_ISDIR(st.st_mode) && !opt_recursive) {
                if (!opt_force) {
                    free_path_list(&files);
                    return nterm_push_error(L, "is a directory");
                }
                continue;
            }

            if (recursive_rm(path) != 0 && !opt_force) {
                free_path_list(&files);
                return nterm_push_error(L, strerror(errno));
            }
        }

        free_path_list(&files);
        lua_pop(L, 1);
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int nb_touch(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);
    FILE* fp = fopen(filename, "a");
    if (!fp) {
        return nterm_push_error(L, strerror(errno));
    }
    fclose(fp);
    lua_pushboolean(L, 1);
    return 1;
}


static int nb_stat(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    struct stat st;
    if (stat(path, &st) != 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    lua_pushstring(L, "size");  lua_pushnumber(L, (lua_Number)st.st_size);   lua_settable(L, -3);
    lua_pushstring(L, "isdir"); lua_pushboolean(L, S_ISDIR(st.st_mode));       lua_settable(L, -3);
    lua_pushstring(L, "isfile");lua_pushboolean(L, S_ISREG(st.st_mode));       lua_settable(L, -3);
    lua_pushstring(L, "mtime"); lua_pushnumber(L, (lua_Number)st.st_mtim.tv_sec); lua_settable(L, -3);
    return 1;
}

static int nb_write_file(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);
    size_t n;
    const char *content = luaL_checklstring(L, 2, &n);
    FILE *f = fopen(filename, "w");
    if (!f) return nterm_push_error(L, strerror(errno));
    size_t w = fwrite(content, 1, n, f);
    int e = ferror(f);
    fclose(f);
    if (e || w != n) return nterm_push_error(L, "write failed");
    lua_pushboolean(L, 1);
    return 1;
}

static int nb_get_cwd(lua_State *L) {
    char buf[MAX_PATH];
    if (getcwd(buf, sizeof(buf)) == NULL) lua_pushstring(L, DFLT_HOME);
    else lua_pushstring(L, buf);
    return 1;
}

/* change ze directory */
static int nb_ch_dir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);

    if (strcmp(path, "~") == 0) {
        path = DFLT_HOME;
    }

    if (chdir(path) == 0) {
        lua_pushboolean(L, 1);
        return 1;
    }
    return nterm_push_error(L, strerror(errno));
}

static int nb_list_dir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    DIR *dir = opendir(path);
    if (!dir) return nterm_push_error(L, strerror(errno));
    lua_newtable(L);
    int i = 1;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        char fullpath[MAX_PATH];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);
        struct stat st;
        if (stat(fullpath, &st) != 0) continue;
        lua_newtable(L);
        lua_pushstring(L, "name");  lua_pushstring(L, ent->d_name);            lua_settable(L, -3);
        lua_pushstring(L, "size");  lua_pushnumber(L, (lua_Number)st.st_size);   lua_settable(L, -3);
        lua_pushstring(L, "isdir"); lua_pushboolean(L, S_ISDIR(st.st_mode));       lua_settable(L, -3);
        lua_pushstring(L, "mtime"); lua_pushnumber(L, (lua_Number)st.st_mtim.tv_sec); lua_settable(L, -3);
        lua_rawseti(L, -2, i++);
    }
    closedir(dir);
    return 1;
}

static int nb_ls(lua_State *L) {
    const char *path = ".";
    int opt_long = 0, opt_all = 0, opt_dirs_first = 0;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "path");         if (!lua_isnil(L, -1)) path = luaL_checkstring(L, -1); lua_pop(L,1);
        lua_getfield(L, 1, "long");         opt_long = lua_toboolean(L, -1); lua_pop(L,1);
        lua_getfield(L, 1, "all");          opt_all = lua_toboolean(L, -1);  lua_pop(L,1);
        lua_getfield(L, 1, "dirs-first");   opt_dirs_first = lua_toboolean(L, -1); lua_pop(L,1);
    } else if (!lua_isnoneornil(L, 1)) {
        path = luaL_checkstring(L, 1);
    }
    DIR *dir = opendir(path);
    if (!dir) return nterm_push_error(L, strerror(errno));
    entry_t *list = NULL;
    size_t cap = 0, len = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!opt_all && ent->d_name[0] == '.') continue;
        if (len == cap) {
            cap = cap ? cap*2 : 64;
            list = (entry_t*)realloc(list, cap*sizeof(entry_t));
            if (!list) { closedir(dir); return nterm_push_error(L, "out of memory"); }
        }
        entry_t *e = &list[len];
        snprintf(e->name, sizeof(e->name), "%s", ent->d_name);
        char fullpath[MAX_PATH];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);
        struct stat st;
        if (stat(fullpath, &st) != 0) { e->isdir = 0; e->size = 0; e->mtime = 0; }
        else {
            e->isdir = S_ISDIR(st.st_mode) ? 1 : 0;
            e->size  = st.st_size;
            e->mtime = st.st_mtim.tv_sec;
        }
        len++;
    }
    closedir(dir);
    if (len == 0) { free(list); lua_pushstring(L, ""); return 1; }
    qsort(list, len, sizeof(entry_t), opt_dirs_first ? cmp_dirs_first : cmp_alpha);
    luaL_Buffer b; luaL_buffinit(L, &b);
    if (opt_long) {
        for (size_t i=0;i<len;i++) {
            char line[512];
            snprintf(line, sizeof(line), "%c %10lld  %s\n", list[i].isdir ? 'd' : '-', (long long)list[i].size, list[i].name);
            luaL_addstring(&b, line);
        }
    } else {
        for (size_t i=0;i<len;i++) {
            luaL_addstring(&b, list[i].name);
            if (list[i].isdir) luaL_addstring(&b, "/");
            luaL_addstring(&b, "\t");
        }
        luaL_addstring(&b, "\n");
    }
    free(list);
    luaL_pushresult(&b);
    return 1;
}



static const nterm_reg_t IO_FUNCS[] = {
    {"writeFile", nb_write_file}, {"write", nb_write_file},
    {"getcwd", nb_get_cwd},       {"pwd", nb_get_cwd},
    {"chdir", nb_ch_dir},         {"cd", nb_ch_dir},
    {"listdir", nb_list_dir},
    {"ls", nb_ls},
    {"cat", nb_cat},
    {"stat", nb_stat},
    {"mkdir", nb_mkdir},
    {"rm", nb_rm},
    {"mv", nb_mv},
    {"cp", nb_cp},
    {"touch", nb_touch},
    {NULL, NULL}
};

void nterm_register_io(lua_State *L) {
    for (const nterm_reg_t *r = IO_FUNCS; r->name; ++r) {
        lua_pushcfunction(L, r->fn);
        lua_setfield(L, -2, r->name);
    }
}
