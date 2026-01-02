// This is completely unfinished
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "py/runtime.h"
#include "py/nlr.h"
#include "py/obj.h"
#include "py/mpconfig.h"

mp_uint_t mp_verbose_flag = 0;

__attribute__((weak)) void mp_hal_delay_ms(mp_uint_t ms) {
    for (volatile mp_uint_t i = 0; i < ms * 1000; i++) {
        // busy wait
    }
}

__attribute__((weak)) mp_uint_t mp_hal_ticks_ms(void) {

    static mp_uint_t ticks = 0;
    return ++ticks;
}

__attribute__((weak)) void mp_hal_stdout_tx_str(const char *str) {
    printf("%s", str);
}

__attribute__((weak)) void mp_hal_stdout_tx_strn(const char *str, size_t len) {
    printf("%.*s", (int)len, str);
}

__attribute__((weak)) int mp_hal_stdin_rx_chr(void) {
    return getchar();
}


__attribute__((weak)) void mp_keyboard_interrupt(void) {
}

__attribute__((weak)) bool mp_sched_vm_abort(void) {
    return false;
}
__attribute__((weak)) void mp_sched_schedule(mp_obj_t function, mp_obj_t arg) {
    mp_call_function_1(function, arg);
}
