#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <bcm2835.h>
#include <signal.h>
#include <fcntl.h>

#define SPI_CHANNEL 0
#define NUM_SENSORS 4

static int running = 1;

// ADC 값 읽기
uint16_t read_adc(int channel) {
    if (channel < 0 || channel > 7) {
        fprintf(stderr, "Channel must be between 0 and 7\n");
        return 0;
    }

    uint8_t buffer[3] = {0};
    buffer[0] = 0x01;  // Start bit
    buffer[1] = (0x08 + channel) << 4;  // Single-ended, channel select
    buffer[2] = 0x00;

    bcm2835_spi_transfern((char*)buffer, 3);
    
    return ((buffer[1] & 0x03) << 8) + buffer[2];
}

// 평균 ADC 값 읽기
uint16_t read_adc_avg(int channel, float interval, float duration) {
    int total_samples = (int)(duration / interval);
    uint32_t total = 0;

    for (int i = 0; i < total_samples; i++) {
        total += read_adc(channel);
        usleep(interval * 1000000);  // interval을 마이크로초 단위로 변환
    }

    return total / total_samples;
}

// 전압을 수위 퍼센트로 변환하는 함수
float calculate_water_level_percentage(int sensor_id, float voltage) {
    float percentage = 0.0;
    float base_voltage; // 'not submerged' 기준 전압
    float reference_voltage; // 'mostly submerged' 기준 전압 (100% 기준점)
    
    if (sensor_id == 4) {
        // 4번 센서: 1.5V가 0%, 3.0V가 100% 기준
        base_voltage = 1.5;
        reference_voltage = 3.0;
    }
    else if (sensor_id == 3) {
        // 3번 센서: 2.5V가 0%, 3.0V가 100% 기준
        base_voltage = 2.0;
        reference_voltage = 3.5;
    }
    else {
        // 1번, 2번 센서: 2.95V가 0%, 3.35V가 100% 기준
        base_voltage = 2.95;
        reference_voltage = 3.35;
    }

    // 기준 전압 미만이면 0% 처리
    if (voltage <= base_voltage) {
        return 0.0;
    }
        // 기준 전압 초과면 100% 처리
    if (voltage >= reference_voltage) {
        return 100.0;
    }

    // 전압에 따른 비율 계산 (100% 이상도 허용)
    percentage = ((voltage - base_voltage) / (reference_voltage - base_voltage)) * 100.0;
    
    return percentage;
}

void sigintHandler(int sig_num) {
    running = 0;
}

int main() {
    // bcm2835 라이브러리 초기화
    if (!bcm2835_init()) {
        printf("bcm2835 초기화 실패\n");
        return -1;
    }

    // SPI 초기화
    if (!bcm2835_spi_begin()) {
        printf("SPI 초기화 실패\n");
        bcm2835_close();
        return -1;
    }

    // SPI 모드 설정
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256); // 약 1MHz
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);

    // Ctrl+C 핸들러 설정
    signal(SIGINT, sigintHandler);

    printf("Reading water level sensor values... Press Ctrl+C to exit.\n");

    while (running) {
        for (int sensor_id = 1; sensor_id <= NUM_SENSORS; sensor_id++) {
            // 1초 동안 0.1초 간격으로 샘플링하여 평균 ADC 값 읽기
            uint16_t adc_value = read_adc_avg(sensor_id, 0.1, 1.0);
            
            // ADC 값을 전압으로 변환 (5V 기준)
            float voltage = (adc_value / 1023.0) * 5.0;
            
            // 수위 퍼센트 계산
            float water_level = calculate_water_level_percentage(sensor_id, voltage);
            
            // 수위 상태 결정
            const char* water_status;
            if (sensor_id == 4) {  // 4번 센서용 특별 임계값
                if (voltage >= 3.0) {  // 완전히 잠겼을 때
                    water_status = "Sensor mostly submerged";
                } else if (voltage <= 1.5) {  // 전혀 잠기지 않았을 때
                    water_status = "Sensor not submerged";
                } else {  // 부분적으로 잠겼을 때
                    water_status = "Sensor partially submerged";
                }
            } 
            else if (sensor_id == 3) {  // 3번 센서용 특별 임계값
                if (voltage > 3.0) {
                    water_status = "Sensor mostly submerged";
                } else if (voltage < 2.5) {
                    water_status = "Sensor not submerged";
                } else {
                    water_status = "Sensor partially submerged";
                }
            }
            else {  // 1번, 2번 센서용 기존 임계값
                if (voltage > 3.35) {
                    water_status = "Sensor mostly submerged";
                } else if (voltage < 2.95) {
                    water_status = "Sensor not submerged";
                } else {
                    water_status = "Sensor partially submerged";
                }
            }

            // 결과 출력
            printf("Sensor %d: Raw ADC: %d, Voltage: %.2f V, Water Level: %.1f%%, Status: %s\n",
                   sensor_id, adc_value, voltage, water_level, water_status);
        }
        
        // 구분선 출력
        printf("--------------------------------------------------\n");
        sleep(1);
    }

    printf("\nExiting...\n");
    bcm2835_spi_end();
    bcm2835_close();
    return 0;
}