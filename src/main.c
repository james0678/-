#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include "types.h"
#include "config.h"
#include "logger.h"
#include "adc.h"
#include "network.h"
#include "water_level.h"
#include "ph_sensor.h"
#include "thread_manager.h"

static volatile bool running = true;

// 시그널 핸들러
static void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        log_info("Received signal %d, shutting down...", signum);
        running = false;
    }
}

// 수위 모니터링 스레드 함수
static void* water_level_thread(void* arg) {
    while (running) {
        for (int i = 0; i < NUM_SENSORS; i++) {
            SensorData data = read_sensor_with_filtering(i);
            if (data.water_level >= 0) {  // 유효한 데이터인 경우
                if (!send_sensor_data(&data)) {
                    log_error("Failed to send water level data for sensor %d", i);
                }
            }
        }
        usleep(MEASUREMENT_INTERVAL * 1000000);
    }
    return NULL;
}

// pH 모니터링 스레드 함수
static void* ph_thread(void* arg) {
    while (running) {
        PhData data = read_ph_with_filtering();
        if (data.ph_value > 0) {  // 유효한 데이터인 경우
            if (!send_ph_data(&data)) {
                log_error("Failed to send pH data");
            }
        }
        usleep(MEASUREMENT_INTERVAL * 1000000);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
        return 1;
    }

    // 설정 파일 로드
    if (!load_config(argv[1])) {
        fprintf(stderr, "Failed to load config file\n");
        return 1;
    }

    // 로거 초기화
    if (!logger_init(LOG_FILE_PATH, DEFAULT_LOG_LEVEL)) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    // ADC 초기화
    if (!adc_init()) {
        log_error("Failed to initialize ADC");
        return 1;
    }

    // pH 센서 초기화
    if (!ph_sensor_init()) {
        log_error("Failed to initialize pH sensor");
        adc_cleanup();
        return 1;
    }

    // 네트워크 초기화
    NetworkConfig net_config = {
        .host = "localhost",  // 설정 파일에서 로드한 값으로 대체
        .port = PORT,
        .timeout_seconds = 5,
        .max_retries = 3
    };
    
    if (!network_init(&net_config)) {
        log_error("Failed to initialize network");
        ph_sensor_cleanup();
        adc_cleanup();
        return 1;
    }

    // 시그널 핸들러 설정
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 모니터링 스레드 추가
    add_monitoring_thread(water_level_thread, NULL, MEASUREMENT_INTERVAL * 1000);
    add_monitoring_thread(ph_thread, NULL, MEASUREMENT_INTERVAL * 1000);

    // 모니터링 시작
    if (!start_monitoring_threads()) {
        log_error("Failed to start monitoring threads");
        network_cleanup();
        ph_sensor_cleanup();
        adc_cleanup();
        return 1;
    }

    log_info("Water level and pH monitoring started");

    // 메인 루프
    while (running) {
        if (!check_thread_health()) {
            log_error("Thread health check failed");
            break;
        }
        sleep(1);
    }

    // 정리
    stop_monitoring_threads();
    network_cleanup();
    ph_sensor_cleanup();
    adc_cleanup();
    logger_cleanup();

    return 0;
} 