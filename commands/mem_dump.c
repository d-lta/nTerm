#include "nterm.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* TODO - add permission flag because this might reset the calculator */
/* memory dumping because i need all that sweet bin files */
static int nb_mem_dump(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);
    unsigned int start_addr;
    unsigned int size;
    int num_args = lua_gettop(L);
    
    if (num_args == 2 && lua_type(L, 2) == LUA_TSTRING) {
        // lets dump sdram
        const char *mem_region = luaL_checkstring(L, 2);
        if (strcmp(mem_region, "sdram") == 0) {
            start_addr = 0x10000000;
            size = 0x2000000;
        } else {
            return nterm_push_error(L, "Invalid memory region specified. Use 'sdram' or provide a numeric address and size.");
        }
    } else if (num_args == 3 && lua_type(L, 2) == LUA_TNUMBER && lua_type(L, 3) == LUA_TNUMBER) {

        start_addr = luaL_checknumber(L, 2);
        size = luaL_checknumber(L, 3);
    } else {
        return nterm_push_error(L, "Invalid arguments. Expected `memdump filename 'sdram'` or `memdump filename start_addr size`.");
    }
    
    // woah are you sure
    if (start_addr < 0x10000000) {
        printf("Warning: Accessing system memory region 0x%08x\n", start_addr);
    }
    
    FILE *f = fopen(filename, "wb");
    if (!f) {
        return nterm_push_error(L, strerror(errno));
    }
    
    void *ptr = (void*)start_addr;
    size_t w = fwrite(ptr, size, 1, f);
    int e = ferror(f);
    fclose(f);
    
    if (e || w != 1) {
        return nterm_push_error(L, "Failed to write memory dump to file.");
    }
    
    lua_pushboolean(L, 1);
    return 1;
}
/* hexdumping because i need to hexdump the calculator*/
static int nb_hexdump(lua_State *L) {
    unsigned int start_addr;
    unsigned int size = 256;
    const char *format = "C";
    int num_args = lua_gettop(L);
    

    if (lua_type(L, 1) == LUA_TSTRING) {
        const char *mem_region = luaL_checkstring(L, 1);
        if (strcmp(mem_region, "sdram") == 0) {
            start_addr = 0x10000000;
        } else {
            return nterm_push_error(L, "Invalid memory region. Use 'sdram' or provide numeric address.");
        }
    } else if (lua_type(L, 1) == LUA_TNUMBER) {
        start_addr = luaL_checknumber(L, 1);
    } else {
        return nterm_push_error(L, "First argument must be address (number) or 'sdram' (string).");
    }

    if (num_args >= 2 && lua_type(L, 2) == LUA_TNUMBER) {
        size = luaL_checknumber(L, 2);
        if (size > 4096) {
            size = 4096;
        }
    }
    

    if (num_args >= 3 && lua_type(L, 3) == LUA_TSTRING) {
        format = luaL_checkstring(L, 3);
    }
    
    // again, are you sure
    if (start_addr < 0x10000000) {
        printf("Warning: Accessing system memory region 0x%08x\n", start_addr);
    }
    
    unsigned char *ptr = (unsigned char*)start_addr;
    
    printf("Hexdump of 0x%08x (%u bytes):\n", start_addr, size);
    
    if (strcmp(format, "C") == 0) {
        for (unsigned int i = 0; i < size; i += 16) {
            printf("%08x  ", start_addr + i);
            
            for (int j = 0; j < 16; j++) {
                if (i + j < size) {
                    printf("%02x ", ptr[i + j]);
                } else {
                    printf("   ");
                }
                if (j == 7) printf(" ");
            }
            
            printf(" |");
            
            // ascii
            for (int j = 0; j < 16 && (i + j) < size; j++) {
                unsigned char c = ptr[i + j];
                printf("%c", isprint(c) ? c : '.');
            }
            
            printf("|\n");
        }
    } else if (strcmp(format, "x") == 0) {
        for (unsigned int i = 0; i < size; i += 16) {
            printf("%08x: ", start_addr + i);
            for (int j = 0; j < 16 && (i + j) < size; j++) {
                printf("%02x ", ptr[i + j]);
            }
            printf("\n");
        }
    } else {
        return nterm_push_error(L, "Invalid format. Use 'C' for canonical or 'x' for hex-only.");
    }
    
    lua_pushboolean(L, 1);
    return 1;
}

/* can i have a peek at that address please */
static int nb_mem_peek(lua_State *L) {
    unsigned int addr = luaL_checknumber(L, 1);
    const char *type = "b";
    
    if (lua_gettop(L) >= 2) {
        type = luaL_checkstring(L, 2);
    }
    
    // okay this is getting repetitive
    if (addr & 0x3 && strcmp(type, "b") != 0) {
        printf("Warning: Unaligned access at 0x%08x\n", addr);
    }
    
    if (strcmp(type, "b") == 0) {
        unsigned char val = *(unsigned char*)addr;
        lua_pushnumber(L, val);
        printf("0x%08x: 0x%02x (%u)\n", addr, val, val);
    } else if (strcmp(type, "w") == 0) {
        unsigned short val = *(unsigned short*)addr;
        lua_pushnumber(L, val);
        printf("0x%08x: 0x%04x (%u)\n", addr, val, val);
    } else if (strcmp(type, "d") == 0) {
        unsigned int val = *(unsigned int*)addr;
        lua_pushnumber(L, val);
        printf("0x%08x: 0x%08x (%u)\n", addr, val, val);
    } else {
        return nterm_push_error(L, "Invalid type. Use 'b' (byte), 'w' (word), or 'd' (dword).");
    }
    
    return 1;
}


static int nb_memmap(lua_State *L) {
    printf("TI-Nspire Memory Map:\n");
    printf("0x00000000-0x0FFFFFFF: Boot/System ROM\n");
    printf("0x10000000-0x11FFFFFF: SDRAM (32MB total)\n");  
    printf("0x90000000-0x907FFFFF: Boot1/Boot2 regions\n");
    printf("0x900A0000-0x900AFFFF: GPIO/Keypad\n");
    printf("0x900B0000-0x900BFFFF: Power Management\n");
    printf("0x900F0000-0x900FFFFF: LCD Controller\n");
    printf("0xA4000000-0xA7FFFFFF: OS/System area\n");
    printf("\nSafe starting points:\n");
    printf("  peek 0x10000000    # SDRAM start\n");
    printf("  peek 0x900A0000    # GPIO registers\n");
    printf("  peek 0x900F0020    # LCD contrast\n");
    
    lua_pushboolean(L, 1);
    return 1;
}


const nterm_reg_t MEMORY_FUNCS[] = {
    {"memdump", nb_mem_dump},
    {"hexdump", nb_hexdump},
    {"peek", nb_mem_peek},
    {"memmap", nb_memmap},
    {NULL, NULL}
};

void nterm_register_mem_dump(lua_State *L) {
    for (const nterm_reg_t *r = MEMORY_FUNCS; r->name; ++r) {
        lua_pushcfunction(L, r->fn);
        lua_setfield(L, -2, r->name);
    }
}
