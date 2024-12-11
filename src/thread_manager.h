#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

#include <pthread.h>
#include <stdbool.h>

typedef struct {
    pthread_t thread_id;
    bool is_running;
    void* (*thread_func)(void*);
    void* arg;
    int interval_ms;
} ThreadInfo;

// 스레드 생성 및 관리
bool start_monitoring_threads(void);
void stop_monitoring_threads(void);

// 스레드 상태 모니터링
bool check_thread_health(void);

// 스레드 추가 함수 선언 추가
bool add_monitoring_thread(void* (*thread_func)(void*), void* arg, int interval_ms);

#endif 