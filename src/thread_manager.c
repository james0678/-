#include "thread_manager.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_THREADS 10

static ThreadInfo threads[MAX_THREADS];
static int thread_count = 0;
static bool should_stop = false;

static void* thread_wrapper(void* arg) {
    ThreadInfo* info = (ThreadInfo*)arg;
    
    while (!should_stop) {
        if (!info->is_running) {
            break;
        }
        
        info->thread_func(info->arg);
        
        if (info->interval_ms > 0) {
            usleep(info->interval_ms * 1000);
        }
    }
    
    return NULL;
}

bool start_monitoring_threads(void) {
    should_stop = false;
    
    for (int i = 0; i < thread_count; i++) {
        threads[i].is_running = true;
        
        if (pthread_create(&threads[i].thread_id, NULL, thread_wrapper, &threads[i]) != 0) {
            log_error("Failed to create thread %d", i);
            return false;
        }
        
        log_info("Started monitoring thread %d", i);
    }
    
    return true;
}

void stop_monitoring_threads(void) {
    should_stop = true;
    
    for (int i = 0; i < thread_count; i++) {
        threads[i].is_running = false;
        pthread_join(threads[i].thread_id, NULL);
        log_info("Stopped monitoring thread %d", i);
    }
}

bool check_thread_health(void) {
    for (int i = 0; i < thread_count; i++) {
        if (!threads[i].is_running) {
            log_error("Thread %d is not running", i);
            return false;
        }
    }
    return true;
}

bool add_monitoring_thread(void* (*thread_func)(void*), void* arg, int interval_ms) {
    if (thread_count >= MAX_THREADS) {
        log_error("Maximum number of threads reached");
        return false;
    }
    
    ThreadInfo* info = &threads[thread_count];
    info->thread_func = thread_func;
    info->arg = arg;
    info->interval_ms = interval_ms;
    info->is_running = false;
    
    thread_count++;
    return true;
} 