#include "ph_sensor.h"
#include "adc.h"
#include "logger.h"
#include "config.h"
#include <math.h>

static MovingAverage ph_filter;

bool ph_sensor_init(void) {
    ph_filter.head = 0;
    ph_filter.count = 0;
    return true;
}

static float voltage_to_ph(float voltage) {
    // pH 값 계산 (보정된 값 사용)
    float slope = (PH_VALUE_2 - PH_VALUE_1) / (PH_VOLTAGE_2 - PH_VOLTAGE_1);
    float ph_value = PH_VALUE_1 + slope * (voltage - PH_VOLTAGE_1);
    
    // pH 값 범위 제한
    if (ph_value > 14.0f) ph_value = 14.0f;
    if (ph_value < 0.0f) ph_value = 0.0f;
    
    return ph_value;
}

PhData read_ph_with_filtering(void) {
    PhData result = {0};
    uint32_t adc_sum = 0;
    int valid_samples = 0;
    
    // 여러 샘플 수집
    for (int i = 0; i < PH_SAMPLES; i++) {
        uint16_t adc_value = adc_read(0);  // pH 센서는 채널 0 사용
        if (adc_value > 0 && adc_value < ADC_MAX_VALUE) {
            adc_sum += adc_value;
            valid_samples++;
        }
        usleep(2000);
    }
    
    if (valid_samples == 0) {
        log_error("No valid pH readings");
        return result;
    }
    
    result.voltage = adc_to_voltage(adc_sum / valid_samples);
    float raw_ph = voltage_to_ph(result.voltage);
    
    // 이동 평균 필터 적용
    if (ph_filter.count < QUEUE_SIZE) {
        ph_filter.queue[ph_filter.head] = raw_ph;
        ph_filter.head = (ph_filter.head + 1) % QUEUE_SIZE;
        ph_filter.count++;
        
        float sum = 0;
        for (int i = 0; i < ph_filter.count; i++) {
            sum += ph_filter.queue[i];
        }
        result.ph_value = sum / ph_filter.count;
    } else {
        float sum = 0;
        ph_filter.queue[ph_filter.head] = raw_ph;
        ph_filter.head = (ph_filter.head + 1) % QUEUE_SIZE;
        
        for (int i = 0; i < QUEUE_SIZE; i++) {
            sum += ph_filter.queue[i];
        }
        result.ph_value = sum / QUEUE_SIZE;
    }
    
    log_debug("pH Reading - Voltage: %.3fV, pH: %.2f", result.voltage, result.ph_value);
    return result;
}

void ph_sensor_cleanup(void) {
    // 현재는 특별한 정리 작업이 필요 없음
} 