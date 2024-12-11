#ifndef CONFIG_H
#define CONFIG_H

// 네트워크 설정
#define PORT 8080
#define MAX_IP_LENGTH 16
#define MAX_RETRIES 3
#define RECONNECT_DELAY 1  // seconds

// 센서 설정
#define NUM_SENSORS 4
#define SPI_CHANNEL 0
#define ADC_MAX_VALUE 1023
#define VOLTAGE_REF 5.0

// 샘플링 설정
#define WATER_LEVEL_SAMPLES 10
#define PH_SAMPLES 50
#define SAMPLE_DELAY_US 10000  // 10ms
#define MEASUREMENT_INTERVAL 3  // seconds

// pH 센서 보정값
#define PH_VOLTAGE_1 2.52  // pH 6.0일 때의 전압
#define PH_VOLTAGE_2 3.0   // pH 7.0일 때의 전압
#define PH_VALUE_1 6.0
#define PH_VALUE_2 7.0

// 이동 평균 필터 설정
#define QUEUE_SIZE 10

// 추가될 설정
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_ERROR 2

#define DEFAULT_LOG_LEVEL LOG_LEVEL_INFO
#define LOG_FILE_PATH "/var/log/water_monitor.log"

// JSON 설정 파일에서 로드할 수 있도록 변경
typedef struct {
    NetworkConfig network;
    SensorCalibration calibrations[NUM_SENSORS];
    int log_level;
    char log_file[256];
} AppConfig;

bool load_config(const char* config_file);

#endif 