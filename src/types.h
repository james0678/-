#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <time.h>

// 네트워크 설정 구조체
typedef struct {
    char* host;
    int port;
    int timeout_seconds;
    int max_retries;
} NetworkConfig;

// 보정 포인트 구조체
typedef struct {
    float voltage;
    float percentage;
} CalibrationPoint;

// 센서 보정 구조체
typedef struct {
    CalibrationPoint* points;
    int num_points;
    float min_valid_voltage;
    float max_valid_voltage;
} SensorCalibration;

// 이동 평균 필터 구조체
typedef struct {
    double queue[10];  // QUEUE_SIZE를 직접 사용
    int head;
    int count;
} MovingAverage;

// 센서 데이터 구조체
typedef struct {
    int sensor_id;
    float water_level;
    float voltage;
} SensorData;

// pH 센서 데이터 구조체
typedef struct {
    float ph_value;
    float voltage;
} PhData;

#endif 