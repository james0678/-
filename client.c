// clientWaterLevel.c
// 컴파일 방법: gcc -o clientWaterLevel clientWaterLevel.c -lbcm2835 -ljson-c -lpthread -lm
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
#include <pthread.h>  // 스레드 관련 헤더 추가
#include <math.h>  // fabs 함수를 위해 추가
#include <stdbool.h>  // bool 타입을 위한 헤더

// 함수 프로토타입 선언
void get_current_timestamp(char* timestamp);
int send_data_to_server(const char* server_ip, int port, const char* data);
float read_water_level(int channel);
float calculate_water_level(float voltage);
int send_data_with_retry(const char* server_ip, int port, const char* data, int max_retries);
void reinit_spi(void);

#define PORT 8080
#define MAX_IP_LENGTH 16
#define SPI_CHANNEL 0
#define NUM_SENSORS 4
#define QUEUE_SIZE 10

// 전역 변수
volatile int shutdown_flag = 0;

// 스레드 인자를 위한 구조체
typedef struct {
    char* server_ip;
} ThreadArgs;

static int running = 1;

// ADC 값 읽기 함수
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
    
    uint16_t value = ((buffer[1] & 0x03) << 8) + buffer[2];
    
    // 값 검증 추가
    // if (value == 0 || value > 1023) {
    //     printf("Warning: Invalid ADC value on channel %d: %d\n", channel, value);
    //     return 0;
    // }
    
    return value;
}

// 구간별 선형 보간을 위한 구조체
typedef struct {
    float voltage;
    float percentage;
} CalibrationPoint;

float calculate_water_level_percentage(int sensor_id, float voltage) {
    // 각 센서별 보정 포인트 (전압, 수위%) 정의
    CalibrationPoint sensor1[] = {
        {0.0, 0}, {1.7, 25}, {2.65, 50}, {2.87, 75}, {3.03, 100}
    };
    CalibrationPoint sensor2[] = {
        {0.0, 0}, {1.8, 25}, {2.0, 50}, {2.3, 75}, {3.10, 100}
    };
    CalibrationPoint sensor3[] = {
        {0.0, 0}, {2.2, 25}, {3.0, 50}, {3.25, 75}, {3.48, 100}
    };
    CalibrationPoint sensor4[] = {
        {0.0, 0}, {2.12, 25}, {2.90, 50}, {3.2, 75}, {3.50, 100}
    };

    CalibrationPoint* points;
    int num_points = 5;

    // 센서 선택
    switch(sensor_id) {
        case 1: points = sensor1; break;
        case 2: points = sensor2; break;
        case 3: points = sensor3; break;
        case 4: points = sensor4; break;
        default: return 0.0;
    }

    // 범위 체크
    if (voltage <= points[0].voltage) return 0.0;
    if (voltage >= points[num_points-1].voltage) return 100.0;

    // 구간 찾기 및 선형 보간
    for (int i = 0; i < num_points - 1; i++) {
        if (voltage >= points[i].voltage && voltage <= points[i+1].voltage) {
            float voltage_range = points[i+1].voltage - points[i].voltage;
            float percentage_range = points[i+1].percentage - points[i].percentage;
            float voltage_offset = voltage - points[i].voltage;
            
            return points[i].percentage + 
                   (voltage_offset / voltage_range) * percentage_range;
        }
    }

    return 0.0;  // 기본값
}

// 여러 번 측정하여 평균을 구하는 함수
float read_water_level_with_average(int sensor_id) {
    const int NUM_SAMPLES = 10;
    float voltage_sum = 0.0;
    int valid_samples = 0;
    
    for (int i = 0; i < NUM_SAMPLES; i++) {
        uint16_t adc_value = read_adc(sensor_id);
        if (adc_value > 0) {  // 유효한 값 사용
            float voltage = (adc_value / 1023.0) * 5.0;
            voltage_sum += voltage;
            valid_samples++;
        }
        usleep(10000);
    }
    
    if (valid_samples == 0) {
        printf("Error: No valid readings for sensor %d\n", sensor_id);
        return 0.0;
    }
    
    float avg_voltage = voltage_sum / valid_samples;
    return calculate_water_level_percentage(sensor_id, avg_voltage);
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
    
    // 서버로 보내는 JSON 데이터 출력
    printf("\nSending JSON data to server:\n%s\n", json_str);
    
    send(sock, json_str, strlen(json_str), 0);
    
    json_object_put(json_obj);
    close(sock);
}

// SIGINT 핸들러 수정
void sigintHandler(int sig_num) {
    printf("\nReceived SIGINT. Shutting down...\n");
    running = 0;
    shutdown_flag = 1;
    
    // 강제 종료를 위한 시그널 재설정
    signal(SIGINT, SIG_DFL);
    
    // 모든 정리 작업 수행
    bcm2835_spi_end();
    bcm2835_close();
    
    // 프로세스 종료
    exit(0);
}

// 네트워크 기화 함수
void init_network() {
    system("sudo ifconfig wlan0 down");
    sleep(1);
    system("sudo ifconfig wlan0 up");
    sleep(2);
    printf("Network interface reset completed.\n");
}

// 이동 평균 필터 구조체 추가
typedef struct {
    double queue[QUEUE_SIZE];
    int head;
    int count;
} MovingAverage;

// 전역 변수로 pH 필터 추가
static MovingAverage phFilter;

// 이동 평균 필터 함수들 추가
void initMovingAverage(MovingAverage* filter) {
    filter->head = 0;
    filter->count = 0;
    for (int i = 0; i < QUEUE_SIZE; i++) {
        filter->queue[i] = 0.0;
    }
}

double addToMovingAverage(MovingAverage* filter, double value) {
    filter->queue[filter->head] = value;
    filter->head = (filter->head + 1) % QUEUE_SIZE;
    if (filter->count < QUEUE_SIZE) filter->count++;

    double sum = 0.0;
    for (int i = 0; i < filter->count; i++) {
        sum += filter->queue[i];
    }
    return sum / filter->count;
}

// pH 값 읽기 함수 수정
float read_ph(void) {
    const int NUM_SAMPLES = 50;
    uint32_t adc_total = 0;
    int valid_samples = 0;
    
    for (int i = 0; i < NUM_SAMPLES; i++) {
        uint16_t adc_value = read_adc(0);  // pH 센서는 채널 0 사용
        if (adc_value > 0) {
            adc_total += adc_value;
            valid_samples++;
        }
        usleep(2000);
    }
    
    if (valid_samples == 0) {
        printf("Error: No valid pH readings\n");
        return 0.0;
    }
    
    double voltage = ((adc_total / valid_samples) / 1023.0) * 5.0;
    
    // pH 값 계산 (보정된 값 사용)
    const double V1 = 2.52;  // pH 6.0일 때의 전압 (수정됨)
    const double V2 = 3.0;   // pH 7.0일 때의 전압 (임의 설정)
    const double PH1 = 6.0;  // 첫 번째 기준점
    const double PH2 = 7.0;  // 두 번째 기준점
    
    double slope = (PH2 - PH1) / (V2 - V1);
    double ph_value = PH1 + slope * (voltage - V1);
    
    // pH 값 범위 제한
    if (ph_value > 14.0) ph_value = 14.0;
    if (ph_value < 0.0) ph_value = 0.0;
    
    return ph_value;
}

// pH 데이터 전송 함수 추가 (main 함수 위에 추가)
void send_ph_data(const char* server_ip, float ph_value) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error for pH data\n");
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
        printf("Connection Failed for pH data\n");
        close(sock);
        return;
    }
    
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    
    struct json_object *json_obj = json_object_new_object();
    json_object_object_add(json_obj, "table", json_object_new_string("tb_ph"));
    json_object_object_add(json_obj, "timestamp", json_object_new_string(timestamp));
    json_object_object_add(json_obj, "pH_value", json_object_new_double(ph_value));
    
    const char *json_str = json_object_to_json_string(json_obj);
    printf("\nSending pH JSON data to server:\n%s\n", json_str);
    
    send(sock, json_str, strlen(json_str), 0);
    
    json_object_put(json_obj);
    close(sock);
}

// pH 측정 스레드 함수
void* ph_thread(void* arg) {
    char* server_ip = (char*)arg;
    
    while (running) {
        // ADC 값 읽기
        uint32_t adc_total = 0;
        for (int i = 0; i < 50; i++) {
            adc_total += read_adc(0);
            usleep(2000);
        }
        uint16_t adc_value = adc_total / 50;
        
        // 전압으로 변환
        double voltage = (adc_value / 1023.0) * 5.0;
        float ph = read_ph();
        
        // voltage와 pH 값 출력
        printf("pH Sensor - Voltage: %.2fV, pH Value: %.2f\n", voltage, ph);
        
        send_ph_data(server_ip, ph);
        sleep(0.5);
    }
    return NULL;
}

// 수위 데이터를 서버로 전송하는 함수 수정
void send_water_level_data(int sensor_id, float water_level, float voltage, const char* server_ip) {
    char timestamp[32];
    get_current_timestamp(timestamp);
    
    // JSON 데이터 생성
    char json_data[256];
    snprintf(json_data, sizeof(json_data),
             "{ \"table\": \"tb_water_level\", \"timestamp\": \"%s\", \"sensor_id\": \"%d\", \"water_level\": %.1f, \"voltage\": %.16f }",
             timestamp, sensor_id, water_level, voltage);
    
    printf("\nSending JSON data to server:\n%s\n", json_data);
    
    // 서버로 데이터 전송
    if (send_data_to_server(server_ip, PORT, json_data) < 0) {
        printf("Connection Failed\n");
        return;
    }
    
    printf("Sent data for sensor %d: Water Level: %.1f%%, Voltage: %.2fV\n", 
           sensor_id, water_level, voltage);
}

// 수위 모니터링 스레드 함수 수정
void *water_level_monitoring_thread(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    
    while (!shutdown_flag) {
        for (int i = 0; i < NUM_SENSORS; i++) {
            float voltage = read_water_level(i);
            float water_level = calculate_water_level(voltage);
            
            // 모든 수위 데이터를 서버로 전송 (유효성 검사 제거)
            send_water_level_data(i + 1, water_level, voltage, args->server_ip);
            
            usleep(250000); // 0.25 대기
        }
    }
    return NULL;
}

// 전압을 수위 퍼센트로 변환하는 함수
float calculate_water_level(float voltage) {
    // 전압 범위: 0V ~ 5V
    // 수위 범위: 0% ~ 100%
    float water_level = (voltage / 5.0) * 100.0;
    
    // 범위 제한
    if (water_level > 100.0) water_level = 100.0;
    if (water_level < 0.0) water_level = 0.0;
    
    return water_level;
}

// 현재 시간 얻기
void get_current_timestamp(char* timestamp) {
    time_t now = time(NULL);
    strftime(timestamp, 32, "%Y-%m-%dT%H:%M:%S", localtime(&now));
}

// 전역 변수로 소켓 선언
static int sock = 0;
static struct sockaddr_in serv_addr;

// 서버 연결 설정 함수
void setup_connection(const char* server_ip) {
    while (running) {
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            printf("Socket creation error\n");
            sleep(1);
            continue;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);

        if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
            printf("Invalid address or Address not supported\n");
            close(sock);
            sleep(1);
            continue;
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("Connection failed\n");
            close(sock);
            sleep(1);
            continue;
        }

        printf("Connected to server\n");
        break;
    }
}

// 데이터 전송 함수
int send_data_to_server(const char* server_ip, int port, const char* data) {
    if (send(sock, data, strlen(data), 0) < 0) {
        printf("Send failed, attempting to reconnect...\n");
        close(sock);
        setup_connection(server_ip);
        return -1;
    }
    printf("Data sent successfully\n");
    return 0;
}

// 재시도 함수
int send_data_with_retry(const char* server_ip, int port, const char* data, int max_retries) {
    int retry_count = 0;
    while (retry_count < max_retries) {
        if (send_data_to_server(server_ip, port, data) == 0) {
            return 0;  // 성공
        }
        printf("Connection failed, retrying (%d/%d)...\n", retry_count + 1, max_retries);
        sleep(1);
        retry_count++;
    }
    return -1;  // 최대 재시도 횟수 초과
}

// reinit_spi 함수 정의 (main 함수 위에 추가)
void reinit_spi(void) {
    bcm2835_spi_end();
    sleep(1);
    if (!bcm2835_spi_begin()) {
        printf("SPI 재초기화 실패\n");
        return;
    }
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
    printf("SPI 재초기화 완료\n");
}

// 수위 센서 읽기 함수 추가
float read_water_level(int channel) {
    uint16_t adc_value = read_adc(channel);
    return (adc_value / 1023.0) * 5.0;  // 전압값으로 변환 (0-5V)
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

    // 네트워크 초기화
    init_network();

    char *server_ip = argv[1];
    printf("Starting water level monitoring for server at %s:%d\n", server_ip, PORT);

    if (!bcm2835_init()) {
        printf("bcm2835 초기화 실패\n");
        return -1;
    }

    if (!bcm2835_spi_begin()) {
        printf("SPI 초��화 실패\n");
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

    // pH 이동 평균 필터 초화
    initMovingAverage(&phFilter);

    // pH 측정 스레드 생성
    pthread_t ph_tid;
    if (pthread_create(&ph_tid, NULL, ph_thread, (void*)server_ip) != 0) {
        printf("Failed to create pH thread\n");
        return -1;
    }

    int error_count = 0;
    const int MAX_ERRORS = 5;

    // 초기 서버 연결
    setup_connection(server_ip);

    while (running) {
        // 에러 카운트가 임계값 초과시 초기화
        if (error_count >= MAX_ERRORS) {
            printf("Too many errors, reinitializing...\n");
            reinit_spi();
            init_network();
            error_count = 0;
            sleep(5);
            continue;
        }

        // 모든 센서의 데이터를 한 번에 수집
        float water_levels[NUM_SENSORS];
        float voltages[NUM_SENSORS];
        bool valid_readings = true;
        
        for (int sensor_id = 1; sensor_id <= NUM_SENSORS; sensor_id++) {
            water_levels[sensor_id-1] = read_water_level_with_average(sensor_id);
            
            uint32_t adc_total = 0;
            int valid_samples = 0;
            
            for (int i = 0; i < 10; i++) {
                uint16_t adc_value = read_adc(sensor_id);
                if (adc_value > 0 && adc_value < 1024) {
                    adc_total += adc_value;
                    valid_samples++;
                }
                usleep(10000);
            }
            
            if (valid_samples == 0) {
                printf("Error: No valid readings for sensor %d\n", sensor_id);
                valid_readings = false;
                error_count++;
                break;
            }
            
            voltages[sensor_id-1] = (adc_total / (float)valid_samples / 1023.0) * 5.0;
        }
        
        if (!valid_readings) {
            sleep(1);
            continue;
        }
        
        // pH 센서 측정
        float ph = read_ph();
        if (ph == 0.0) {
            error_count++;
            sleep(1);
            continue;
        }
        
        // JSON 데이터 생성 및 전송
        struct json_object *json_array = json_object_new_array();
        
        for (int sensor_id = 1; sensor_id <= NUM_SENSORS; sensor_id++) {
            struct json_object *sensor_data = json_object_new_object();
            json_object_object_add(sensor_data, "sensor_id", json_object_new_int(sensor_id));
            json_object_object_add(sensor_data, "water_level", json_object_new_double(water_levels[sensor_id-1]));
            json_object_object_add(sensor_data, "voltage", json_object_new_double(voltages[sensor_id-1]));
            json_object_array_add(json_array, sensor_data);
            
            printf("Sensor %d: Water Level: %.1f%%, Voltage: %.2fV\n", 
                   sensor_id, water_levels[sensor_id-1], voltages[sensor_id-1]);
        }
        
        struct json_object *ph_data = json_object_new_object();
        json_object_object_add(ph_data, "type", json_object_new_string("ph"));
        json_object_object_add(ph_data, "value", json_object_new_double(ph));
        json_object_array_add(json_array, ph_data);
        
        const char *json_str = json_object_to_json_string(json_array);
        if (send_data_with_retry(server_ip, PORT, json_str, 3) == 0) {
            error_count = 0;  // 성공적인 전송 시 에러 카운트 리셋
        } else {
            error_count++;
        }
        
        json_object_put(json_array);  // JSON 객체 메모리 해제
        
        sleep(3);  // 측정 주기
    }

    // pH 스레드 종료 대기
    pthread_join(ph_tid, NULL);

    printf("\nShutting down...\n");
    bcm2835_spi_end();
    bcm2835_close();
    return 0;
}