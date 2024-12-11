#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include "config.h"

bool logger_init(const char* log_file, int log_level);
void logger_cleanup(void);

void log_debug(const char* format, ...);
void log_info(const char* format, ...);
void log_error(const char* format, ...);

#endif 