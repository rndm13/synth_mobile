#include "log.h"
#include "stdio.h"
#include <stdarg.h>

FILE* g_log_f;
void log_init(void) {
    g_log_f = stderr;
    setvbuf(g_log_f, NULL, _IOFBF, 4096);
    log_info("Initialized logging");
}

void log_deinit(void) {
    log_flush();
}

void log_flush(void) {
    fflush(g_log_f);
}

void log_debug(const char* msg, ...) {
    // va_list va;
    //
    // va_start(va, msg);
    //
    // fprintf(g_log_f, "[DEBUG] ");
    // vfprintf(g_log_f, msg, va);
    // fprintf(g_log_f, "\n");
    //
    // va_end(va);
}

void log_error(const char* msg, ...) {
    va_list va;

    va_start(va, msg);

    fprintf(g_log_f, "[ERROR] ");
    vfprintf(g_log_f, msg, va);
    fprintf(g_log_f, "\n");

    va_end(va);
}

void log_info(const char* msg, ...) {
    va_list va;

    va_start(va, msg);

    fprintf(g_log_f, "[INFO ] ");
    vfprintf(g_log_f, msg, va);
    fprintf(g_log_f, "\n");

    va_end(va);
}

void log_warn(const char* msg, ...) {
    va_list va;

    va_start(va, msg);

    fprintf(g_log_f, "[WARN ] ");
    vfprintf(g_log_f, msg, va);
    fprintf(g_log_f, "\n");

    va_end(va);
}
