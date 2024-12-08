//컴파일 방법: gcc -o clientWaterLevel clientWaterLevel.c -lbcm2835 -ljson-c
// 실행 방법: sudo ./clientWaterLevel 192.168.14.17

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <json-c/json.h>
#include <time.h>
#include <bcm2835.h>
#include <signal.h>

#define PORT 8080
#define MAX_IP_LENGTH 16
#define SPI_CHANNEL 0
#define NUM_SENSORS 4

static int running = 1;

// ADC 값 읽기 함수 (waterLevel.c에서 가져옴)
uint16_t read_adc(int channel) {
    if (channel < 0 || channel > 7) {
        fprintf(stderr, "Channel must be between 0 and 7\n");
        return 0;
    }

    uint8_t buffer[3] = {0};
    buffer[0] = 0x01;
    buffer[1] = (0x08 + channel) << 4;
    buffer[2] = 0x00;

    bcm2835_spi_transfern((char*)buffer, 3);
    
    return ((buffer[1] & 0x03) << 8) + buffer[2];
}

// 수위 퍼센트 계산 함수 (waterLevel.c에서 가져옴)
float calculate_water_level_percentage(int sensor_id, float voltage) {
    float percentage = 0.0;
    float base_voltage;
    float reference_voltage;
    
    if (sensor_id == 4) {
        base_voltage = 1.5;
        reference_voltage = 3.0;
    }
    else if (sensor_id == 3) {
        base_voltage = 2.0;
        reference_voltage = 3.5;
    }
    else {
        base_voltage = 2.95;
        reference_voltage = 3.35;
    }

    if (voltage <= base_voltage) return 0.0;
    if (voltage >= reference_voltage) return 100.0;

    percentage = ((voltage - base_voltage) / (reference_voltage - base_voltage)) * 100.0;
    return percentage;
}

void send_sensor_data(const char* server_ip, int sensor_id, float water_level, float voltage) {
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
    
    // 현재 시간 생성
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    
    // JSON 데이터 생성
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

    if (!bcm2835_init()) {
        printf("bcm2835 초기화 실패\n");
        return -1;
    }

    if (!bcm2835_spi_begin()) {
        printf("SPI 초기화 실패\n");
        bcm2835_close();
        return -1;
    }

    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

    signal(SIGINT, sigintHandler);

    printf("Starting water level monitoring...\n");

    while (running) {
        for (int sensor_id = 1; sensor_id <= NUM_SENSORS; sensor_id++) {
            uint16_t adc_total = 0;
            for (int i = 0; i < 3; i++) {
                adc_total += read_adc(sensor_id);
                usleep(10000);  // 10ms 대기
            }
            uint16_t adc_value = adc_total / 3;
            
            float voltage = (adc_value / 1023.0) * 5.0;
            float water_level = calculate_water_level_percentage(sensor_id, voltage);
            
            send_sensor_data(server_ip, sensor_id, water_level, voltage);
            printf("Sent data for sensor %d: Water Level: %.1f%%, Voltage: %.2fV\n", 
                   sensor_id, water_level, voltage);
            
            sleep(1);  // 각 센서 간 1초 대기
        }
    }

    printf("\nShutting down...\n");
    bcm2835_spi_end();
    bcm2835_close();
    return 0;
}