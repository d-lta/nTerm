#include "nterm.h"
#include <lauxlib.h>
#include <lualib.h>
#include <unistd.h>

void nterm_register_io(lua_State *L);
void nterm_register_fastfetch(lua_State *L);
void nterm_register_lua_tools(lua_State *L);
void nterm_register_process(lua_State *L);
void nterm_register_nano(lua_State *L);
void nterm_register_grep(lua_State *L);

void nterm_register_mem_dump(lua_State *L);
void nterm_register_zip(lua_State *L);
// void nterm_register_luna(lua_State *L); -  this is broken so i'm leaving it commented out for now
// void nterm_register_ndcall(lua_State *L);
int main(void)
{
    // set working directory first
    chdir(DFLT_HOME);

    lua_State *L = nl_lua_getstate();
    if (!L) {
        return 0; // failed to get lua state
    }

    // setup nterm module table
    lua_newtable(L);

    nterm_register_io(L);
    nterm_register_fastfetch(L);
    nterm_register_lua_tools(L);
    nterm_register_process(L);
    nterm_register_nano(L);
    nterm_register_grep(L);
    //nterm_register_ndcall(L);  same as above
    nterm_register_mem_dump(L);
    nterm_register_zip(L);
    // nterm_register_luna(L); - nope


    lua_setglobal(L, "nterm");

    return 0;
}

