// 컴파일: gcc -o clientSensorMonitor clientSensorMonitor.c -lwiringPi -ljson-c
// 실행: sudo ./clientSensorMonitor 192.168.14.17

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <json-c/json.h>
#include <time.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <signal.h>

#define SPI_CHANNEL 0
#define SPI_SPEED 1350000
#define PORT 8080
#define NUM_WATER_SENSORS 4

static int running = 1;

// ADC 읽기 함수
uint16_t readADC(int channel) {
    uint8_t buffer[3];
    buffer[0] = 1;
    buffer[1] = (8 + channel) << 4;
    buffer[2] = 0;

    wiringPiSPIDataRW(SPI_CHANNEL, buffer, 3);
    return ((buffer[1] & 3) << 8) + buffer[2];
}

// pH 센서용 평균 ADC 값 읽기
uint16_t readADCAvg(int channel, int samples) {
    uint32_t total = 0;
    for (int i = 0; i < samples; i++) {
        total += readADC(channel);
        usleep(2000);
    }
    return total / samples;
}

float calculate_water_level_percentage(int sensor_id, float voltage) {
    float percentage = 0.0;
    float base_voltage;      // 최소 전압 (0%)
    float reference_voltage; // 최대 전압 (100%)
    float tolerance = 0.5;   // 허용 오차 범위 증가
    
    // 각 센서별 기준 전압 설정
    switch(sensor_id) {
        case 1:
            reference_voltage = 2.83;
            base_voltage = 2.0;        // 기준 전압 상향 조정
            break;
        case 2:
            reference_voltage = 2.73;  // 센서 2의 실제 측정 전압으로 수정
            base_voltage = 2.0;
            break;
        case 3:
            reference_voltage = 2.74;
            base_voltage = 2.0;
            break;
        case 4:
            reference_voltage = 2.74;
            base_voltage = 2.0;
            break;
        default:
            reference_voltage = 2.8;
            base_voltage = 2.0;
    }

    // reference_voltage 근처의 값은 100%로 처리 (더 관대한 범위)
    if (voltage >= (reference_voltage - tolerance)) {
        return 100.0;
    }

    // 범위 체크
    if (voltage <= base_voltage) {
        return 0.0;
    }

    // 비율 계산 (비 가파른 곡선)
    float normalized = (voltage - base_voltage) / (reference_voltage - base_voltage);
    percentage = normalized * normalized * normalized * 100.0;  // 세제곱으로 변경하여 고수위에서 더 높은 값이 나오도록
    
    // 수위가 80% 이상일 때는 더 높은 값을 반환
    if (percentage > 80.0) {
        percentage = percentage * 1.2;  // 20% 증가
    }
    
    // 범위 제한
    if (percentage > 100.0) percentage = 100.0;
    if (percentage < 0.0) percentage = 0.0;
    
    return percentage;
}

// pH 데이터 전송
void send_ph_data(const char* server_ip, float pH_value, float voltage) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("pH Socket creation error\n");
        return;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        printf("Invalid address: %s\n", server_ip);
        close(sock);
        return;
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("pH Connection Failed\n");
        close(sock);
        return;
    }
    
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    
    struct json_object *json_obj = json_object_new_object();
    json_object_object_add(json_obj, "table", json_object_new_string("tb_ph"));
    json_object_object_add(json_obj, "timestamp", json_object_new_string(timestamp));
    json_object_object_add(json_obj, "pH_value", json_object_new_double(pH_value));
    json_object_object_add(json_obj, "voltage", json_object_new_double(voltage));
    json_object_object_add(json_obj, "sensor_id", json_object_new_string("SENSOR_01"));
    json_object_object_add(json_obj, "location", json_object_new_string("LAB_01"));
    
    const char *json_str = json_object_to_json_string(json_obj);
    send(sock, json_str, strlen(json_str), 0);
    
    printf("Sent pH data: pH: %.2f, Voltage: %.2fV\n", pH_value, voltage);
    
    json_object_put(json_obj);
    close(sock);
}

// 수위 데이터 전송
void send_water_level_data(const char* server_ip, int sensor_id, float water_level, float voltage) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Water level Socket creation error\n");
        return;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        printf("Invalid address: %s\n", server_ip);
        close(sock);
        return;
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Water level Connection Failed\n");
        close(sock);
        return;
    }
    
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    
    struct json_object *json_obj = json_object_new_object();
    json_object_object_add(json_obj, "table", json_object_new_string("tb_water_level"));
    json_object_object_add(json_obj, "timestamp", json_object_new_string(timestamp));
    char sensor_id_str[8];
    snprintf(sensor_id_str, sizeof(sensor_id_str), "%d", sensor_id);
    json_object_object_add(json_obj, "sensor_id", json_object_new_string(sensor_id_str));
    json_object_object_add(json_obj, "water_level", json_object_new_double(water_level));
    json_object_object_add(json_obj, "voltage", json_object_new_double(voltage));
    
    const char *json_str = json_object_to_json_string(json_obj);
    send(sock, json_str, strlen(json_str), 0);
    
    printf("Sent water level data for sensor %d: Level: %.1f%%, Voltage: %.2fV\n", 
           sensor_id, water_level, voltage);
    
    json_object_put(json_obj);
    close(sock);
}

void sigintHandler(int sig_num) {
    running = 0;
}

void init_network() {
    system("sudo ifconfig wlan0 down");
    sleep(1);
    system("sudo ifconfig wlan0 up");
    sleep(2);
    printf("Network interface reset completed.\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

    init_network();

    char *server_ip = argv[1];
    printf("Starting sensor monitoring for server at %s:%d\n", server_ip, PORT);

    if (wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) == -1) {
        printf("SPI 초기화 실패\n");
        return -1;
    }

    signal(SIGINT, sigintHandler);

    while (running) {
        // pH 센서 읽기
        uint16_t ph_adc = readADCAvg(0, 50);
        float ph_voltage = (ph_adc / 1023.0) * 5.0;
        float ph_value = 7.0 + ((4.8 - ph_voltage) / 0.18);
        send_ph_data(server_ip, ph_value, ph_voltage);
        
        // 수위 센서 읽기
        for (int sensor_id = 1; sensor_id <= NUM_WATER_SENSORS; sensor_id++) {
            uint16_t water_adc_total = 0;
            for (int i = 0; i < 3; i++) {
                water_adc_total += readADC(sensor_id);
                usleep(10000);
            }
            uint16_t water_adc = water_adc_total / 3;
            
            float water_voltage = (water_adc / 1023.0) * 5.0;
            float water_level = calculate_water_level_percentage(sensor_id, water_voltage);
            
            send_water_level_data(server_ip, sensor_id, water_level, water_voltage);
            sleep(1);
        }
        
        sleep(1);
    }
    
    printf("\nShutting down...\n");
    return 0;
}