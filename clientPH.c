// # 필요한 라이브러리 설치
// sudo apt-get install libjson-c-dev wiringpi

// # 컴파일
// gcc -o clientPH clientPH.c -lwiringPi -ljson-c

// # 실행 (서버 IP 주소 필요)
// ./clientPH 192.168.14.17

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <json-c/json.h>
#include <time.h>
#include <wiringPi.h>
#include <ads1115.h>
#include <signal.h>

#define PORT 8080
#define MAX_IP_LENGTH 16
#define ADS1115_ADDRESS 0x48
#define ADS1115_CHANNEL 0

static int running = 1;

void send_sensor_data(const char* server_ip, float pH_value, float voltage) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
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
        printf("Connection Failed\n");
        close(sock);
        return;
    }
    
    // 현재 시간 생성 (형식: YYYY-MM-DDTHH:MM:SS)
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    
    // JSON 데이터 생성 (단순화된 형식)
    struct json_object *json_obj = json_object_new_object();
    json_object_object_add(json_obj, "timestamp", json_object_new_string(timestamp));
    json_object_object_add(json_obj, "pH_value", json_object_new_double(pH_value));
    json_object_object_add(json_obj, "voltage", json_object_new_double(voltage));
    
    const char *json_str = json_object_to_json_string(json_obj);
    send(sock, json_str, strlen(json_str), 0);
    
    printf("Sent data: pH: %.2f, Voltage: %.2fV\n", pH_value, voltage);
    
    json_object_put(json_obj);
    close(sock);
}

void sigintHandler(int sig_num) {
    running = 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    printf("Starting pH monitoring for server at %s:%d\n", server_ip, PORT);

    if (wiringPiSetup() == -1) {
        printf("WiringPi 초기화 실패\n");
        return 1;
    }

    ads1115Setup(ADS1115_ADDRESS, ADS1115_CHANNEL);
    
    // Ctrl+C 핸들러 설정
    signal(SIGINT, sigintHandler);

    while (running) {
        int adc_value = analogRead(ADS1115_CHANNEL);
        float voltage = adc_value * (4.096 / 32767.0);
        float ph_value = 7.0 + ((2.5 - voltage) / 0.18);
        
        send_sensor_data(server_ip, ph_value, voltage);
        sleep(1);
    }
    
    printf("\nShutting down...\n");
    return 0;
}