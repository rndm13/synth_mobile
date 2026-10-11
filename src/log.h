#pragma once

void log_init(void);
void log_deinit(void);
void log_flush(void);
void log_debug(const char* msg, ...);
void log_error(const char* msg, ...);
void log_info(const char* msg, ...);
void log_warn(const char* msg, ...);
