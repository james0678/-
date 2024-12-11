#include "water_level.h"
#include "adc.h"
#include "logger.h"
#include "config.h"
#include <stdlib.h>
#include <math.h>

// 이상치 제거를 위한 표준편차 임계값
#define STD_DEV_THRESHOLD 2.0

static float calculate_mean(float* values, int count) {
    float sum = 0;
    for (int i = 0; i < count; i++) {
        sum += values[i];
    }
    return sum / count;
}

static float calculate_std_dev(float* values, int count, float mean) {
    float sum_sq_diff = 0;
    for (int i = 0; i < count; i++) {
        float diff = values[i] - mean;
        sum_sq_diff += diff * diff;
    }
    return sqrt(sum_sq_diff / count);
}

SensorData read_sensor_with_filtering(int sensor_id) {
    SensorData result = {0};
    result.sensor_id = sensor_id;

    float voltages[WATER_LEVEL_SAMPLES];
    int valid_count = 0;

    // 여러 샘플 수집
    for (int i = 0; i < WATER_LEVEL_SAMPLES; i++) {
        uint16_t adc_value = adc_read(sensor_id);
        float voltage = adc_to_voltage(adc_value);
        
        if (voltage >= 0 && voltage <= VOLTAGE_REF) {
            voltages[valid_count++] = voltage;
        }
    }

    if (valid_count < 3) {
        log_error("Sensor %d: Not enough valid samples (%d)", sensor_id, valid_count);
        return result;
    }

    // 이상치 제거
    float mean = calculate_mean(voltages, valid_count);
    float std_dev = calculate_std_dev(voltages, valid_count, mean);
    
    float filtered_sum = 0;
    int filtered_count = 0;

    for (int i = 0; i < valid_count; i++) {
        if (fabs(voltages[i] - mean) <= STD_DEV_THRESHOLD * std_dev) {
            filtered_sum += voltages[i];
            filtered_count++;
        }
    }

    if (filtered_count == 0) {
        log_error("Sensor %d: No samples left after filtering", sensor_id);
        return result;
    }

    result.voltage = filtered_sum / filtered_count;
    result.water_level = convert_to_water_level(sensor_id, result.voltage);

    log_debug("Sensor %d: Voltage=%.3f, Level=%.1f%%", 
              sensor_id, result.voltage, result.water_level);

    return result;
} 