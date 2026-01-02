// neofetch.c - system info for TI calculators
#include "nterm.h"
#include <nucleus.h>
#include <libndls.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef HAVE_MALLINFO
#include <malloc.h>
#endif
// 90% of this probably doesn't work



int (*get_raw_adc)(int param_1) = (int (*)(int))0x100dd8b4;

// calibration curve
unsigned int (*get_battery_percent_from_adc)(unsigned int param_1) = (unsigned int (*)(unsigned int))0x105e4240;

static int adc_read_mv_patched(void) {
    return get_raw_adc(0);
}

static const char* detect_model(void) {
    if (is_classic) {
        if (is_cm) return "TI-Nspire CM/CM-C";
        return is_touchpad ? "TI-Nspire Touchpad" : "TI-Nspire Clickpad";
    } else {
        return "TI-Nspire CX-series";
    }
}

static const char* get_chip_info(void) {
    return is_classic ? "TI ASIC (NS200x series)" : "TI ASIC (ET-NS2010 family)";
}

static const char* get_cpu_info(void) {
    // classic runs a bit faster but uses more power
    return is_classic ? "ARM926EJ-S @ ~150 MHz" : "ARM926EJ-S @ ~132 MHz";
}

static void get_screen_info(char *screen_str, size_t screen_sz,
                            char *lcd_type_str, size_t lcd_type_sz,
                            int *out_w, int *out_h) {
    scr_type_t t = lcd_type();
    int w = 320, h = 240;

    switch (t) {
        case SCR_320x240_4:
            strncpy(lcd_type_str, "SCR_320x240_4 (4-bit gray)", lcd_type_sz);
            break;
        case SCR_320x240_8:
            strncpy(lcd_type_str, "SCR_320x240_8 (8-bit pal)", lcd_type_sz);
            break;
        case SCR_320x240_16:
            strncpy(lcd_type_str, "SCR_320x240_16 (RGB444)", lcd_type_sz);
            break;
        case SCR_320x240_565:
            strncpy(lcd_type_str, "SCR_320x240_565 (RGB565)", lcd_type_sz);
            break;
        case SCR_240x320_565:
            strncpy(lcd_type_str, "SCR_240x320_565 (RGB565, rotated)", lcd_type_sz);
            w = 240; h = 320;
            break;
        default:
            strncpy(lcd_type_str, "SCR_UNKNOWN", lcd_type_sz);
            break;
    }

    snprintf(screen_str, screen_sz, "%dx%d", w, h);
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}

static void format_memory_info(char *buffer, size_t size) {
#ifdef HAVE_MALLINFO
    struct mallinfo mi = mallinfo();
    unsigned usedK = (unsigned)(mi.uordblks / 1024u);
    unsigned totalK = (unsigned)(mi.arena / 1024u);
    if (totalK > 0) {
        unsigned percent = (usedK * 100u) / totalK;
        snprintf(buffer, size, "%uK / %uK (%u%%)", usedK, totalK, percent);
    } else {
        snprintf(buffer, size, "%uK used", usedK);
    }
#else
    // rough estimates - CX has more RAM than classic
    unsigned totalMB = (has_colors || is_cm) ? 64u : 32u;
    snprintf(buffer, size, "~%uMB available", totalMB);
#endif
}

// battery stuff is tricky - different OS versions put things in different places
extern int batt_percent(void) __attribute__((weak));
extern int batt_status(void) __attribute__((weak));
extern int power_is_connected(void) __attribute__((weak));
extern int battery_percent(void) __attribute__((weak));
extern int battery_status(void) __attribute__((weak));
extern int is_charging(void) __attribute__((weak));

extern unsigned os_version_index __attribute__((weak));

typedef int (*read_adc_t)(int ch);

// these addresses are for specific OS builds
static read_adc_t resolve_read_adc(void) {
    if (!os_version_index) return NULL;
    unsigned v = os_version_index & 0xFFFFFF00u;

    // some common CAS CX builds
    switch (v) {
        case 0x04020100u: return (read_adc_t)0x100D6958;
        case 0x04050100u: return (read_adc_t)0x100DD0F0;
        case 0x04050300u: return (read_adc_t)0x100DD10C;
        case 0x04050400u: return (read_adc_t)0x100DD630;
        case 0x04050500u: return (read_adc_t)0x100DD5F0;
        default: break;
    }
    
    // non-CAS builds use slightly different addresses
    switch (v) {
        case 0x04050100u: return (read_adc_t)0x100DD220;
        case 0x04050300u: return (read_adc_t)0x100DD228;
        case 0x04050400u: return (read_adc_t)0x100DD714;
        case 0x04050500u: return (read_adc_t)0x100DD708;
        default: break;
    }
    return NULL;
}

#ifndef READ_ADC_ADDR
#define READ_ADC_ADDR 0
#endif

static read_adc_t get_read_adc(void) {
#if READ_ADC_ADDR
    return (read_adc_t)READ_ADC_ADDR;
#else
    return resolve_read_adc();
#endif
}

static int adc_batt_channel = -1;
static int adc_mv_min = 3400;
static int adc_mv_max = 4200;

// try to find which ADC channel has the battery voltage
static int autodetect_batt_channel(read_adc_t ra) {
    int best_ch = -1, best_mv = -1;
    for (int ch = 0; ch < 16; ch++) {
        int mv = ra(ch);
        if (mv >= 2000 && mv <= 5000) {
            if (mv > best_mv) { 
                best_mv = mv; 
                best_ch = ch; 
            }
        }
    }
    if (best_ch < 0) {
        // just pick the highest reading if nothing looks battery-like
        for (int ch = 0; ch < 16; ch++) {
            int mv = ra(ch);
            if (mv > best_mv) { 
                best_mv = mv; 
                best_ch = ch; 
            }
        }
    }
    return best_ch;
}

// THIS FUNCTION IS NO LONGER NEEDED, IT WAS REPLACED BY adc_read_mv_patched
// static int adc_read_mv(read_adc_t ra, int ch) {
//     // average a few readings to reduce noise
//     const int N = 6;
//     long sum = 0;
//     for (int i = 0; i < N; i++) 
//         sum += ra(ch);
//     return (int)(sum / N);
// }

static int mv_to_percent(int mv) {
    if (mv <= adc_mv_min) return 0;
    if (mv >= adc_mv_max) return 100;
    return (mv - adc_mv_min) * 100 / (adc_mv_max - adc_mv_min);
}

static const char* status_to_str(int st) {
    switch (st) {
        case 0: return "discharging";
        case 1: return "charging";  
        case 2: return "full";
        default: return "unknown";
    }
}

typedef struct {
    int have_pct;
    int pct;
    int have_status;
    int status_code;
    int have_ac;
    int on_ac;
} batt_info_t;
static void read_battery(batt_info_t *bi) {
    memset(bi, 0, sizeof(*bi));

    // Try the weak symbols first, because they are the least likely to be stupid.
    if (batt_percent || battery_percent) {
        int p = batt_percent ? batt_percent() : battery_percent();
        if (p >= 0 && p <= 100) {  
            bi->have_pct = 1;  
            bi->pct = p;  
        }
    }
    
    // Check weak symbols for status and power... you know the drill.
    if (batt_status || battery_status || is_charging) {
        bi->have_status = 1;
        if (batt_status)  
            bi->status_code = batt_status();
        else if (battery_status)  
            bi->status_code = battery_status();
        else  
            bi->status_code = is_charging() ? 1 : 0;
    }
    
    if (power_is_connected) {
        bi->have_ac = 1;
        bi->on_ac = power_is_connected() ? 1 : 0;
    }


    if (!bi->have_pct && !is_classic) {
        if (os_version_index && (os_version_index >> 24) == 0x04) {
            int mv = adc_read_mv_patched();
            bi->pct = get_battery_percent_from_adc(mv);
            bi->have_pct = 1;
            
            if (!bi->have_status) {  
                bi->status_code = (bi->pct >= 99) ? 2 : 0;  
                bi->have_status = 1;  
            }
        }
    }
}

static void format_battery_info(char *buffer, size_t size,
                                char *percent_str, size_t percent_sz,
                                char *charging_str, size_t charging_sz,
                                char *power_str, size_t power_sz) {
    batt_info_t bi;
    read_battery(&bi);

    if (bi.have_pct) {
        const char *st = bi.have_status ? status_to_str(bi.status_code) : "unknown";
        if (bi.have_ac) 
            snprintf(buffer, size, "%d%% (%s, %s)", bi.pct, st, bi.on_ac ? "on AC" : "on battery");
        else 
            snprintf(buffer, size, "%d%% (%s)", bi.pct, st);
    } else {
        if (bi.have_status) {
            const char *st = status_to_str(bi.status_code);
            if (bi.have_ac) 
                snprintf(buffer, size, "N/A (%s, %s)", st, bi.on_ac ? "on AC" : "on battery");
            else 
                snprintf(buffer, size, "N/A (%s)", st);
        } else {
            snprintf(buffer, size, "N/A");
        }
    }

    snprintf(percent_str, percent_sz, bi.have_pct ? "%d" : "N/A", bi.pct);
    snprintf(charging_str, charging_sz, "%s", 
        bi.have_status ? ((bi.status_code == 1 || bi.status_code == 2) ? "Yes" : "No") : "Unknown");
    snprintf(power_str, power_sz, "%s", 
        bi.have_ac ? (bi.on_ac ? "AC" : "Battery") : "Unknown");
}

// some builds have os_version as a function instead of a variable
extern const char *os_version(void) __attribute__((weak));

static void get_os_version(char *buffer, size_t size) {
    unsigned int ndless_rev = nl_ndless_rev();

    if (os_version) {
        const char *ver = os_version();
        if (ver && ver[0]) {
            snprintf(buffer, size, "OS %s with Ndless r%u", ver, ndless_rev);
            return;
        }
    }

    if (os_version_index) {
        unsigned v = os_version_index;
        unsigned maj = (v >> 24) & 0xFF;
        unsigned min = (v >> 16) & 0xFF;  
        unsigned pat = (v >> 8) & 0xFF;
        unsigned bld = v & 0xFF;
        snprintf(buffer, size, "OS %u.%u.%u.%u with Ndless r%u", maj, min, pat, bld, ndless_rev);
    } else {
        snprintf(buffer, size, "OS Unknown with Ndless r%u", ndless_rev);
    }
}

static void get_hwtype(char *buffer, size_t size) {
    unsigned t = hwtype();
    snprintf(buffer, size, "%u", t);
}

static void get_uptime(char *buffer, size_t size) {
    static clock_t t0 = 0;
    if (!t0) t0 = clock();
    
    clock_t now = clock();
    unsigned long ms = 0;
    if (now > t0) {
        ms = (unsigned long)((now - t0) * 1000UL / CLOCKS_PER_SEC);
    }
    snprintf(buffer, size, "%lums (program)", ms);
}

static void check_ndless_dir(char *buffer, size_t size) {
    const char *root = get_documents_dir();
    if (!root || !*root) {
        snprintf(buffer, size, "unavailable");
        return;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "%s/ndless", root);

    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        snprintf(buffer, size, "present (%s)", path);
    } else {
        snprintf(buffer, size, "not found (%s)", path);
    }
}

static int nb_sysinfo(lua_State *L) {
    char os_info[96], mem[64], bat[64], screen_str[32], lcd_type_buf[64];
    char hwtype_str[8], uptime_str[48], ndless_dir_str[128];
    char bat_pct_str[16], bat_charging_str[16], bat_power_str[16];
    int w, h;

    const char *model = detect_model();
    const char *chip = get_chip_info();  
    const char *cpu = get_cpu_info();
    const char *colors = has_colors ? "Yes" : "No";
    const char *touch = is_touchpad ? "Yes" : "No";

    get_os_version(os_info, sizeof(os_info));
    format_memory_info(mem, sizeof(mem));
    format_battery_info(bat, sizeof(bat), bat_pct_str, sizeof(bat_pct_str),
                        bat_charging_str, sizeof(bat_charging_str),
                        bat_power_str, sizeof(bat_power_str));
    get_screen_info(screen_str, sizeof(screen_str), lcd_type_buf, sizeof(lcd_type_buf), &w, &h);
    get_hwtype(hwtype_str, sizeof(hwtype_str));
    get_uptime(uptime_str, sizeof(uptime_str));
    check_ndless_dir(ndless_dir_str, sizeof(ndless_dir_str));

    lua_newtable(L);
    
    lua_pushstring(L, "os"); lua_pushstring(L, os_info); lua_settable(L, -3);
    lua_pushstring(L, "model"); lua_pushstring(L, model); lua_settable(L, -3);
    lua_pushstring(L, "chip"); lua_pushstring(L, chip); lua_settable(L, -3);
    lua_pushstring(L, "cpu"); lua_pushstring(L, cpu); lua_settable(L, -3);
    lua_pushstring(L, "memory"); lua_pushstring(L, mem); lua_settable(L, -3);
    lua_pushstring(L, "screen"); lua_pushstring(L, screen_str); lua_settable(L, -3);
    lua_pushstring(L, "battery"); lua_pushstring(L, bat); lua_settable(L, -3);
    lua_pushstring(L, "colors"); lua_pushstring(L, colors); lua_settable(L, -3);

    lua_pushstring(L, "lcd_type"); lua_pushstring(L, lcd_type_buf); lua_settable(L, -3);
    lua_pushstring(L, "hwtype"); lua_pushstring(L, hwtype_str); lua_settable(L, -3);
    lua_pushstring(L, "touchpad"); lua_pushstring(L, touch); lua_settable(L, -3);
    lua_pushstring(L, "uptime_ms"); lua_pushstring(L, uptime_str); lua_settable(L, -3);
    lua_pushstring(L, "ndless_dir"); lua_pushstring(L, ndless_dir_str); lua_settable(L, -3);

    lua_pushstring(L, "battery_percent"); lua_pushstring(L, bat_pct_str); lua_settable(L, -3);
    lua_pushstring(L, "charging"); lua_pushstring(L, bat_charging_str); lua_settable(L, -3);
    lua_pushstring(L, "power"); lua_pushstring(L, bat_power_str); lua_settable(L, -3);

    return 1;
}

static const nterm_reg_t SYSINFO_FUNCS[] = {
    {"sysinfo", nb_sysinfo},
    {NULL, NULL}
};

void nterm_register_fastfetch(lua_State *L) {
    for (const nterm_reg_t *r = SYSINFO_FUNCS; r->name; ++r) {
        lua_pushcfunction(L, r->fn);
        lua_setfield(L, -2, r->name);
    }
}
