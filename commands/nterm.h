#pragma once

#include <os.h>
#include <lauxlib.h>
#include <nucleus.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define DFLT_HOME "/documents"

static inline int nterm_push_error(lua_State *L, const char *err) {
	lua_pushnil(L); // FIXED: Use nil to unambiguously signal failure
	lua_pushstring(L, err);
	return 2;
}
typedef struct {
	const char *name;
	lua_CFunction fn;
} nterm_reg_t;
