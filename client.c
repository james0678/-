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
#include <sys/time.h>  // struct timeval을 사용하기 위해 추가
#include <netinet/tcp.h>  // TCP_KEEPIDLE 등을 위한 헤더
#include <stdatomic.h>  // C의 atomic 지원을 위한 헤더
#include <errno.h>    // errno 사용을 위해 추가
#include <stdarg.h>   // va_list 사용을 위해 추가

// LogLevel 열거형 정의 추가
typedef enum {
    DEBUG,
    INFO,
    WARNING,
    ERROR
} LogLevel;

// 함수 프로토타입 선언
void get_current_timestamp(char* timestamp);
int send_data_to_server(const char* server_ip, int port, const char* data);
float read_water_level(int channel);
float calculate_water_level(float voltage);
int send_data_with_retry(const char* server_ip, int port, const char* data, int max_retries);
void reinit_spi(void);
void send_water_level_data(int sensor_id, float water_level, float voltage, const char* server_ip);
void send_ph_data(const char* server_ip, float ph_value);

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

// 전역 변수들 - 한 번만 선언
static atomic_bool running = ATOMIC_VAR_INIT(true);
static int sock = -1;  // 소켓 초기화
static atomic_bool is_connected = ATOMIC_VAR_INIT(false);  // 연결 상태
static pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;  // 소켓 뮤텍스

// 구조체 정의
typedef struct {
    float values[QUEUE_SIZE];
    int head;
    int count;
} MovingAverage;

typedef struct {
    float voltage;
    float percentage;
} CalibrationPoint;

// 필터 전역 변수
static MovingAverage phFilter;
static MovingAverage waterLevelFilters[NUM_SENSORS];

// 로깅 시스템 추가
void log_message(LogLevel level, const char* format, ...) {
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    va_list args;
    va_start(args, format);
    printf("[%s][%s] ", timestamp, 
           level == DEBUG ? "DEBUG" : 
           level == INFO ? "INFO" : 
           level == WARNING ? "WARNING" : "ERROR");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

// 기존의 수위 센서 보정 로직 유지 및 개선
float calculate_water_level_percentage(int sensor_id, float voltage) {
    // 기존 보정 포인트 유지
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

    // 센서 선택 로직 개선
    switch(sensor_id) {
        case 1: points = sensor1; break;
        case 2: points = sensor2; break;
        case 3: points = sensor3; break;
        case 4: points = sensor4; break;
        default: 
            log_message(ERROR, "Invalid sensor ID: %d", sensor_id);
            return 0.0;
    }

    // 범위 체크
    if (voltage <= points[0].voltage) return 0.0;
    if (voltage >= points[num_points-1].voltage) return 100.0;

    // 구간 찾기 및 선형 보간 (기존 로직 유지)
    for (int i = 0; i < num_points - 1; i++) {
        if (voltage >= points[i].voltage && voltage <= points[i+1].voltage) {
            float voltage_range = points[i+1].voltage - points[i].voltage;
            float percentage_range = points[i+1].percentage - points[i].percentage;
            float voltage_offset = voltage - points[i].voltage;
            
            return points[i].percentage + 
                   (voltage_offset / voltage_range) * percentage_range;
        }
    }

    return 0.0;
}

// ADC 읽기 함수 개선
uint16_t read_adc(int channel) {
    if (channel < 0 || channel > 7) {
        log_message(ERROR, "Invalid ADC channel: %d", channel);
        return 0;
    }

    uint8_t buffer[3] = {0};
    buffer[0] = 0x01;
    buffer[1] = (0x08 + channel) << 4;
    buffer[2] = 0x00;

    // SPI 통신 오류 처리 수정
    bcm2835_spi_transfern((char*)buffer, 3);
    uint16_t value = ((buffer[1] & 0x03) << 8) + buffer[2];
    
    if (value == 0 || value > 1023) {
        log_message(WARNING, "Suspicious ADC value on channel %d: %d", channel, value);
    }
    
    return value;
}

// 수위 센서 읽기 함수 개선
float read_water_level_with_average(int sensor_id) {
    const int NUM_SAMPLES = 5;
    float voltage_sum = 0.0;
    int valid_samples = 0;
    
    for (int i = 0; i < NUM_SAMPLES; i++) {
        uint16_t adc_value = read_adc(sensor_id);
        if (adc_value > 0 && adc_value < 1024) {
            float voltage = (adc_value / 1023.0) * 5.0;
            voltage_sum += voltage;
            valid_samples++;
        }
        usleep(50000);  // 50ms 대기
    }
    
    if (valid_samples == 0) {
        log_message(ERROR, "No valid readings for sensor %d", sensor_id);
        return 0.0;
    }
    
    float avg_voltage = voltage_sum / valid_samples;
    
    // 이동 평균 필터 적용
    MovingAverage* filter = &waterLevelFilters[sensor_id - 1];
    filter->values[filter->head] = avg_voltage;
    filter->head = (filter->head + 1) % QUEUE_SIZE;
    if (filter->count < QUEUE_SIZE) filter->count++;
    
    float filtered_sum = 0.0;
    for (int i = 0; i < filter->count; i++) {
        filtered_sum += filter->values[i];
    }
    float filtered_voltage = filtered_sum / filter->count;
    
    return calculate_water_level_percentage(sensor_id, filtered_voltage);
}

// pH 센서 읽기 함수 개선
float read_ph(void) {
    const int NUM_SAMPLES = 10;
    uint32_t adc_total = 0;
    int valid_samples = 0;
    
    for (int i = 0; i < NUM_SAMPLES; i++) {
        uint16_t adc_value = read_adc(0);
        if (adc_value > 0 && adc_value < 1024) {
            adc_total += adc_value;
            valid_samples++;
        }
        usleep(20000);  // 20ms 대기
    }
    
    if (valid_samples == 0) {
        log_message(ERROR, "No valid pH readings");
        return 0.0;
    }
    
    double voltage = ((adc_total / valid_samples) / 1023.0) * 5.0;
    
    // pH 값 계산 (보정된 값 사용)
    const double V1 = 2.52;
    const double V2 = 3.0;
    const double PH1 = 6.0;
    const double PH2 = 7.0;
    
    double slope = (PH2 - PH1) / (V2 - V1);
    double ph_value = PH1 + slope * (voltage - V1);
    
    // 범위 제한
    if (ph_value > 14.0) ph_value = 14.0;
    if (ph_value < 0.0) ph_value = 0.0;
    
    return ph_value;
}

// 네트워크 연결 관리 개선
bool connect_to_server(const char* server_ip) {
    static time_t last_attempt = 0;
    time_t current_time = time(NULL);
    
    if (current_time - last_attempt < 10) {
        log_message(DEBUG, "Too soon to retry connection. Waiting...");
        return false;
    }
    
    last_attempt = current_time;
    pthread_mutex_lock(&socket_mutex);
    
    if (sock >= 0) {
        close(sock);
        sock = -1;
    }

    log_message(INFO, "Attempting to connect to %s:%d", server_ip, PORT);
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_message(ERROR, "Socket creation failed: %s", strerror(errno));
        pthread_mutex_unlock(&socket_mutex);
        return false;
    }

    // 소켓 옵션 설정 추가
    int flag = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
        log_message(WARNING, "Failed to set SO_REUSEADDR: %s", strerror(errno));
    }

    // 연결 타임아웃 설정
    struct timeval timeout;
    timeout.tv_sec = 5;  // 5초 타임아웃
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_message(WARNING, "Failed to set receive timeout: %s", strerror(errno));
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_message(WARNING, "Failed to set send timeout: %s", strerror(errno));
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT)
    };

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        log_message(ERROR, "Invalid address: %s", server_ip);
        close(sock);
        sock = -1;
        pthread_mutex_unlock(&socket_mutex);
        return false;
    }

    log_message(DEBUG, "Attempting connection to server...");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        log_message(ERROR, "Connection failed: %s (errno: %d)", strerror(errno), errno);
        close(sock);
        sock = -1;
        pthread_mutex_unlock(&socket_mutex);
        return false;
    }

    is_connected = true;
    log_message(INFO, "Successfully connected to server %s:%d", server_ip, PORT);
    pthread_mutex_unlock(&socket_mutex);
    return true;
}

// 데이터 전송 함수 개선
int send_data_to_server(const char* server_ip, int port, const char* data) {
    pthread_mutex_lock(&socket_mutex);
    
    if (!is_connected) {
        pthread_mutex_unlock(&socket_mutex);
        if (!connect_to_server(server_ip)) {
            return -1;
        }
        pthread_mutex_lock(&socket_mutex);
    }

    int total = 0;
    int len = strlen(data);
    int bytesleft = len;
    int n;

    while (total < len) {
        n = send(sock, data + total, bytesleft, MSG_NOSIGNAL);
        if (n == -1) {
            log_message(ERROR, "Send failed: %s", strerror(errno));
            is_connected = false;
            close(sock);
            sock = -1;
            pthread_mutex_unlock(&socket_mutex);
            return -1;
        }
        total += n;
        bytesleft -= n;
    }

    pthread_mutex_unlock(&socket_mutex);
    return 0;
}

// 네트워크 초기화 함수
void init_network() {
    log_message(INFO, "Initializing network...");
    system("sudo ifconfig wlan0 down");
    sleep(2);
    system("sudo ifconfig wlan0 up");
    sleep(3);
    
    // DHCP 갱신
    system("sudo dhclient wlan0");
    sleep(2);
    
    // 네트워크 상�� 확인
    system("ifconfig wlan0 | grep 'inet '");
    log_message(INFO, "Network initialization completed");
}

// 자원 정리 함수
void cleanup_resources(void) {
    log_message(INFO, "Cleaning up resources...");
    
    // 소켓 정리
    pthread_mutex_lock(&socket_mutex);
    if (sock >= 0) {
        close(sock);
        sock = -1;
    }
    pthread_mutex_unlock(&socket_mutex);
    
    // SPI 정리
    bcm2835_spi_end();
    bcm2835_close();
    
    log_message(INFO, "Cleanup completed");
}

// SIGINT 핸들러
void sigintHandler(int sig_num) {
    log_message(INFO, "Received shutdown signal");
    running = false;
    shutdown_flag = 1;
    
    pthread_mutex_lock(&socket_mutex);
    if (sock >= 0) {
        close(sock);
        sock = -1;
    }
    pthread_mutex_unlock(&socket_mutex);
    
    signal(SIGINT, SIG_DFL);
    cleanup_resources();
    exit(0);
}

// pH 데이터 전송 함수
void send_ph_data(const char* server_ip, float ph_value) {
    char timestamp[32];
    get_current_timestamp(timestamp);
    
    char json_data[256];
    snprintf(json_data, sizeof(json_data),
             "{ \"table\": \"tb_ph\", \"timestamp\": \"%s\", \"pH_value\": %.2f }",
             timestamp, ph_value);
    
    log_message(INFO, "Sending pH data - Value: %.2f", ph_value);
    log_message(DEBUG, "JSON data: %s", json_data);
    
    if (send_data_to_server(server_ip, PORT, json_data) < 0) {
        log_message(ERROR, "Failed to send pH data");
    }
}

// 수위 데이터 전송 함수
void send_water_level_data(int sensor_id, float water_level, float voltage, const char* server_ip) {
    char timestamp[32];
    get_current_timestamp(timestamp);
    
    char json_data[256];
    snprintf(json_data, sizeof(json_data),
             "{ \"table\": \"tb_water_level\", \"timestamp\": \"%s\", \"sensor_id\": \"%d\", \"water_level\": %.1f, \"voltage\": %.16f }",
             timestamp, sensor_id, water_level, voltage);
    
    log_message(INFO, "Sending water level data - Sensor %d, Level: %.1f%%, Voltage: %.2fV",
                sensor_id, water_level, voltage);
    log_message(DEBUG, "JSON data: %s", json_data);
    
    if (send_data_to_server(server_ip, PORT, json_data) < 0) {
        log_message(ERROR, "Failed to send water level data for sensor %d", sensor_id);
    }
}

// 수위 모니터링 스레드
void* water_level_thread(void* arg) {
    char* server_ip = (char*)arg;
    
    while (running) {
        for (int sensor_id = 1; sensor_id <= NUM_SENSORS; sensor_id++) {
            float water_level = read_water_level_with_average(sensor_id);
            float voltage = read_adc(sensor_id) * 5.0 / 1023.0;
            
            if (is_connected) {
                send_water_level_data(sensor_id, water_level, voltage, server_ip);
            }
            
            usleep(500000);  // 센서 간 0.5초 대기
        }
        
        sleep(2);  // 2초 간격으로 측정
    }
    return NULL;
}

// pH 모니터링 스레드
void* ph_thread(void* arg) {
    char* server_ip = (char*)arg;
    
    while (running) {
        float ph = read_ph();
        
        // 이동 평균 필터 적용
        phFilter.values[phFilter.head] = ph;
        phFilter.head = (phFilter.head + 1) % QUEUE_SIZE;
        if (phFilter.count < QUEUE_SIZE) phFilter.count++;
        
        float filtered_sum = 0.0;
        for (int i = 0; i < phFilter.count; i++) {
            filtered_sum += phFilter.values[i];
        }
        float filtered_ph = filtered_sum / phFilter.count;
        
        if (is_connected) {
            send_ph_data(server_ip, filtered_ph);
        }
        
        sleep(2);  // 2초 간격으로 측정
    }
    return NULL;
}

// 현재 시간 얻기
void get_current_timestamp(char* timestamp) {
    time_t now = time(NULL);
    strftime(timestamp, 32, "%Y-%m-%dT%H:%M:%S", localtime(&now));
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        log_message(ERROR, "Usage: %s <server_ip>", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    
    // 네트워크 초기화
    init_network();
    
    log_message(INFO, "Starting water level monitoring for server at %s:%d", server_ip, PORT);

    // 초기 서버 연결 시도
    int retry_count = 0;
    while (!connect_to_server(server_ip) && retry_count < 3) {
        log_message(INFO, "Initial connection attempt %d failed, retrying...", retry_count + 1);
        sleep(2);
        retry_count++;
    }

    if (!is_connected) {
        log_message(ERROR, "Failed to establish initial connection to server after %d attempts", retry_count);
        return 1;
    }

    // BCM2835 초기화
    if (!bcm2835_init()) {
        log_message(ERROR, "Failed to initialize BCM2835");
        return 1;
    }

    // SPI 초기화
    if (!bcm2835_spi_begin()) {
        log_message(ERROR, "Failed to initialize SPI");
        bcm2835_close();
        return 1;
    }

    // SPI 설정
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

    // 시그널 핸들러 설정
    signal(SIGINT, sigintHandler);

    // pH 이동 평균 필터 초기화
    memset(&phFilter, 0, sizeof(MovingAverage));
    for (int i = 0; i < NUM_SENSORS; i++) {
        memset(&waterLevelFilters[i], 0, sizeof(MovingAverage));
    }

    // 스레드 생성
    pthread_t ph_tid, water_level_tid;
    if (pthread_create(&ph_tid, NULL, ph_thread, (void*)server_ip) != 0) {
        log_message(ERROR, "Failed to create pH thread");
        cleanup_resources();
        return 1;
    }

    if (pthread_create(&water_level_tid, NULL, water_level_thread, (void*)server_ip) != 0) {
        log_message(ERROR, "Failed to create water level thread");
        running = false;
        pthread_join(ph_tid, NULL);
        cleanup_resources();
        return 1;
    }

    // 메인 루프
    while (running) {
        sleep(1);
    }

    // 정리
    pthread_join(ph_tid, NULL);
    pthread_join(water_level_tid, NULL);
    cleanup_resources();

    log_message(INFO, "Program terminated successfully");
    return 0;
}