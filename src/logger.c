#include "logger.h"
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>

static FILE* log_fp = NULL;
static int current_log_level = DEFAULT_LOG_LEVEL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

bool logger_init(const char* log_file, int log_level) {
    log_fp = fopen(log_file, "a");
    if (!log_fp) {
        fprintf(stderr, "Failed to open log file: %s\n", log_file);
        return false;
    }
    current_log_level = log_level;
    return true;
}

void logger_cleanup(void) {
    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }
}

static void log_write(int level, const char* level_str, const char* format, va_list args) {
    if (level < current_log_level || !log_fp) return;

    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    pthread_mutex_lock(&log_mutex);
    
    fprintf(log_fp, "[%s] [%s] ", timestamp, level_str);
    vfprintf(log_fp, format, args);
    fprintf(log_fp, "\n");
    fflush(log_fp);
    
    pthread_mutex_unlock(&log_mutex);
}

void log_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_write(LOG_LEVEL_DEBUG, "DEBUG", format, args);
    va_end(args);
}

void log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_write(LOG_LEVEL_INFO, "INFO", format, args);
    va_end(args);
}

void log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_write(LOG_LEVEL_ERROR, "ERROR", format, args);
    va_end(args);
} 