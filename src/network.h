#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include "types.h"

typedef struct {
    char* host;
    int port;
    int timeout_seconds;
    int max_retries;
} NetworkConfig;

// 연결 상태를 관리하는 구조체
typedef struct {
    int socket;
    bool is_connected;
    time_t last_success;
    int failed_attempts;
} ConnectionState;

// 네트워크 초기화 및 연결 관리
bool network_init(const NetworkConfig* config);
bool network_ensure_connection(void);

// 데이터 전송 (자동 재연결 포함)
bool send_sensor_data(const SensorData* data);
bool send_ph_data(const PhData* data);

// 연결 종료
void network_cleanup(void);

#endif 