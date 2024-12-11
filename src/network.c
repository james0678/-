#include "network.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <json-c/json.h>
#include <time.h>

static ConnectionState connection = {0};
static NetworkConfig network_config;

static bool set_socket_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        log_error("Failed to get socket flags: %s", strerror(errno));
        return false;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        log_error("Failed to set socket non-blocking: %s", strerror(errno));
        return false;
    }
    return true;
}

bool network_init(const NetworkConfig* config) {
    memcpy(&network_config, config, sizeof(NetworkConfig));
    connection.is_connected = false;
    connection.failed_attempts = 0;
    return true;
}

bool network_ensure_connection(void) {
    // 이미 연결되어 있다면 true 반환
    if (connection.is_connected) {
        return true;
    }

    // 재연결 시도 횟수 초과 확인
    if (connection.failed_attempts >= network_config.max_retries) {
        log_error("Maximum reconnection attempts reached");
        return false;
    }

    // 기존 소켓이 있다면 닫기
    if (connection.socket > 0) {
        close(connection.socket);
    }

    // 새 소켓 생성
    connection.socket = socket(AF_INET, SOCK_STREAM, 0);
    if (connection.socket < 0) {
        log_error("Socket creation failed: %s", strerror(errno));
        connection.failed_attempts++;
        return false;
    }

    // 논블로킹 모드 설정
    if (!set_socket_nonblocking(connection.socket)) {
        close(connection.socket);
        connection.failed_attempts++;
        return false;
    }

    // 서버 주소 설정
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(network_config.port);
    
    if (inet_pton(AF_INET, network_config.host, &server_addr.sin_addr) <= 0) {
        log_error("Invalid address: %s", network_config.host);
        close(connection.socket);
        connection.failed_attempts++;
        return false;
    }

    // 연결 시도
    if (connect(connection.socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        if (errno != EINPROGRESS) {
            log_error("Connection failed: %s", strerror(errno));
            close(connection.socket);
            connection.failed_attempts++;
            return false;
        }

        // select로 연결 완료 대기
        fd_set write_fds;
        struct timeval timeout;
        
        FD_ZERO(&write_fds);
        FD_SET(connection.socket, &write_fds);
        
        timeout.tv_sec = network_config.timeout_seconds;
        timeout.tv_usec = 0;

        int result = select(connection.socket + 1, NULL, &write_fds, NULL, &timeout);
        if (result <= 0) {
            log_error("Connection timeout");
            close(connection.socket);
            connection.failed_attempts++;
            return false;
        }
    }

    // 연결 성공
    connection.is_connected = true;
    connection.last_success = time(NULL);
    connection.failed_attempts = 0;
    log_info("Connected to server %s:%d", network_config.host, network_config.port);
    
    return true;
}

static bool send_json_data(const char* json_str) {
    if (!network_ensure_connection()) {
        return false;
    }

    size_t total = 0;
    size_t len = strlen(json_str);
    
    while (total < len) {
        ssize_t sent = send(connection.socket, json_str + total, len - total, 0);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
                continue;
            }
            log_error("Send failed: %s", strerror(errno));
            connection.is_connected = false;
            return false;
        }
        total += sent;
    }
    
    return true;
}

bool send_sensor_data(const SensorData* data) {
    struct json_object* json = json_object_new_object();
    char timestamp[32];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    
    json_object_object_add(json, "table", json_object_new_string("tb_water_level"));
    json_object_object_add(json, "timestamp", json_object_new_string(timestamp));
    json_object_object_add(json, "sensor_id", json_object_new_int(data->sensor_id));
    json_object_object_add(json, "water_level", json_object_new_double(data->water_level));
    json_object_object_add(json, "voltage", json_object_new_double(data->voltage));
    
    const char* json_str = json_object_to_json_string(json);
    bool result = send_json_data(json_str);
    
    json_object_put(json);
    return result;
}

bool send_ph_data(const PhData* data) {
    struct json_object* json = json_object_new_object();
    char timestamp[32];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    
    json_object_object_add(json, "table", json_object_new_string("tb_ph"));
    json_object_object_add(json, "timestamp", json_object_new_string(timestamp));
    json_object_object_add(json, "ph_value", json_object_new_double(data->ph_value));
    json_object_object_add(json, "voltage", json_object_new_double(data->voltage));
    
    const char* json_str = json_object_to_json_string(json);
    bool result = send_json_data(json_str);
    
    json_object_put(json);
    return result;
}

void network_cleanup(void) {
    if (connection.socket > 0) {
        close(connection.socket);
        connection.socket = 0;
    }
    connection.is_connected = false;
} 