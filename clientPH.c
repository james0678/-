#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bcm2835.h>
#include <signal.h>
#include <stdint.h>

static int running = 1;

// ADC 값 읽기
uint16_t read_adc(void) {
    uint8_t buffer[3] = {0};
    buffer[0] = 0x01;  // Start bit
    buffer[1] = 0x80;  // Single-ended, channel 0
    buffer[2] = 0x00;

    bcm2835_spi_transfern((char*)buffer, 3);
    return ((buffer[1] & 0x03) << 8) + buffer[2];
}

void sigintHandler(int sig_num) {
    running = 0;
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
    
    signal(SIGINT, sigintHandler);
    printf("Starting pH monitoring... Press Ctrl+C to exit.\n");

    while (running) {
        // 10개 샘플의 평균 구하기
        uint32_t adc_sum = 0;
        for (int i = 0; i < 10; i++) {
            adc_sum += read_adc();
            usleep(10000);  // 10ms 대기
        }
        uint16_t adc_avg = adc_sum / 10;
        
        // 전압 계산 (5V 기준)
        float voltage = (adc_avg / 1023.0) * 5.0;
        
        // pH 값 계산 (보정된 값 사용)
        float ph = 7.0 + ((2.5 - voltage) / 0.18);
        
        // 결과 출력
        printf("Voltage: %.2fV, pH: %.2f\n", voltage, ph);
        
        sleep(1);
    }

    printf("\nShutting down...\n");
    bcm2835_spi_end();
    bcm2835_close();
    return 0;
}