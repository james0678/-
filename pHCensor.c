#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <signal.h>

#define SPI_CHANNEL 0
#define SPI_SPEED 1350000
#define QUEUE_SIZE 10

// 전역 변수
static int running = 1;

// 이동 평균 필터 구조체
typedef struct {
    double queue[QUEUE_SIZE];
    int head;
    int count;
} MovingAverage;

// 이동 평균 필터 초기화
void initMovingAverage(MovingAverage* filter) {
    filter->head = 0;
    filter->count = 0;
    for (int i = 0; i < QUEUE_SIZE; i++) {
        filter->queue[i] = 0.0;
    }
}

// 이동 평균 필터에 값 추가 및 평균 계산
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

// ADC 읽기
uint16_t readADC(int channel) {
    if (channel < 0 || channel > 7) return 0;

    uint8_t buffer[3];
    buffer[0] = 0x01;  // 시작 비트
    buffer[1] = 0x80 | ((channel & 0x07) << 4);  // 단일 엔드 + 채널 선택
    buffer[2] = 0x00;

    printf("전송 전: %02x %02x %02x\n", buffer[0], buffer[1], buffer[2]);
    wiringPiSPIDataRW(SPI_CHANNEL, buffer, 3);
    printf("수신 후: %02x %02x %02x\n", buffer[0], buffer[1], buffer[2]);

    // 10비트 값 추출 방식 변경
    uint16_t value = ((buffer[1] & 0x03) << 8) | buffer[2];
    return value;
}

// 평균 ADC 값 읽기
uint16_t readADCAvg(int channel, int samples) {
    uint32_t total = 0;
    for (int i = 0; i < samples; i++) {
        total += readADC(channel);
        usleep(2000); // 2ms 딜레이
    }
    return total / samples;
}

// 전압을 pH로 변환 (4.99V = pH 6.00 기준점 사용)
double convertToPH(double voltage) {
    const double REFERENCE_VOLTAGE = 4.99;  // 기준 전압
    const double REFERENCE_PH = 6.00;       // 기준 pH
    const double SLOPE = -59.2;             // mV/pH at 25°C (네른스트 방정식)

    // 전압 차이를 pH 차이로 변환 (1000을 곱하여 V를 mV로 변환)
    double pH = REFERENCE_PH - ((voltage - REFERENCE_VOLTAGE) * 1000.0 / SLOPE);
    return pH;
}

// Ctrl+C 핸들러
void sigintHandler(int sig_num) {
    running = 0;
}

int main() {
    // wiringPi 초기화
    if (wiringPiSetupGpio() == -1) {
        printf("GPIO 초기화 실패\n");
        return -1;
    }

    // SPI 속도를 더 낮춰서 시도 (1MHz)
    if (wiringPiSPISetup(SPI_CHANNEL, 1000000) == -1) {
        printf("SPI 초기화 실패\n");
        return -1;
    }

    printf("SPI 초기화 성공\n");
    
    // Ctrl+C 핸들러 설정
    signal(SIGINT, sigintHandler);

    // 이동 평균 필터 초기화
    MovingAverage phFilter;
    initMovingAverage(&phFilter);

    printf("pH 센서 값을 읽는 중... Ctrl+C로 종료\n");

    while (running) {
        // ADC 값 읽기
        uint16_t adcValue = readADCAvg(0, 50);
        
        // 전압으로 변환
        double voltage = (adcValue / 1023.0) * 5.0;
        
        // pH로 변환
        double phValue = convertToPH(voltage);
        
        // 이동 평균 필터 적용
        double smoothedPH = addToMovingAverage(&phFilter, phValue);

        // 결과 출력
        printf("Raw ADC: %d, Voltage: %.2f V, Smoothed pH: %.2f\n", 
               adcValue, voltage, smoothedPH);
        
        sleep(1);
    }

    printf("\n종료 중...\n");
    return 0;
}