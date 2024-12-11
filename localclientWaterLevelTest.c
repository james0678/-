#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bcm2835.h>
#include <math.h>

// 수위 퍼센트 계산 함수
float calculate_water_level_percentage(int sensor_id, float voltage) {
    float percentage = 0.0;
    float base_voltage;      // 최소 전압 (0%)
    float reference_voltage; // 최대 전압 (100%)
    float tolerance = 0.5;   
    
    // 각 센서별 기준 전압 설정
    switch(sensor_id) {
        case 1:
            reference_voltage = 2.83;
            base_voltage = 0.5;
            break;
        case 2:
            reference_voltage = 2.73;
            base_voltage = 0.5;
            break;
        case 3:
            reference_voltage = 2.74;
            base_voltage = 0.5;
            break;
        case 4:
            reference_voltage = 2.74;
            base_voltage = 0.5;
            break;
        default:
            reference_voltage = 2.8;
            base_voltage = 0.5;
    }

    if (voltage >= (reference_voltage - tolerance)) {
        return 100.0;
    }

    if (voltage <= base_voltage) {
        return 0.0;
    }

    float normalized = (voltage - base_voltage) / (reference_voltage - base_voltage);
    percentage = sqrt(normalized) * 100.0;
    
    if (percentage > 100.0) percentage = 100.0;
    if (percentage < 0.0) percentage = 0.0;
    
    return percentage;
}

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
    
    return ((buffer[1] & 0x03) << 8) + buffer[2];
}

int main() {
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

    printf("Starting water level monitoring...\n");
    printf("Press Ctrl+C to exit\n\n");

    while (1) {
        for (int sensor_id = 1; sensor_id <= 4; sensor_id++) {
            // 3번 측정하여 평균값 사용
            uint32_t adc_total = 0;
            for (int i = 0; i < 3; i++) {
                adc_total += read_adc(sensor_id);
                usleep(10000);  // 10ms 대기
            }
            uint16_t adc_value = adc_total / 3;
            
            float voltage = (adc_value / 1023.0) * 5.0;
            float water_level = calculate_water_level_percentage(sensor_id, voltage);
            
            printf("Sensor %d - ADC: %d, Voltage: %.2fV, Water Level: %.1f%%\n", 
                   sensor_id, adc_value, voltage, water_level);
            
            sleep(1);  // 1초 대기
        }
        printf("\n");  // 4개 센서 측정 후 줄바꿈
    }

    bcm2835_spi_end();
    bcm2835_close();
    return 0;
}