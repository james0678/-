#ifndef WATER_LEVEL_H
#define WATER_LEVEL_H

#include "types.h"

typedef struct {
    CalibrationPoint* points;
    int num_points;
    float min_valid_voltage;
    float max_valid_voltage;
} SensorCalibration;

// 센서별 보정 데이터를 설정 파일에서 로드
bool load_sensor_calibrations(void);

// 전압값을 수위로 변환 (보정 데이터 사용)
float convert_to_water_level(int sensor_id, float voltage);

// 여러 샘플의 평균을 구하고 이상치 제거
SensorData read_sensor_with_filtering(int sensor_id);

#endif 