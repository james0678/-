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
#define PUMPS_RELAY_PIN 16  // GPIO 16 (물리적 핀 36)

// pH 임계값 설정
#define PH_THRESHOLD 6.5    // pH가 6.5 이하면 펌프 작동
#define WATER_LEVEL_LOW 2.0 // 수위가 2.0V 이하면 펌프 중지

// pH 센서 관련 상수 추가
#define REFERENCE_VOLTAGE 4.02  // 기준 전압
#define REFERENCE_PH 6.48       // 기준 pH
#define SLOPE -59.2             // mV/pH at 25°C (네른스트 방정식)

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

void sigintHandler(int sig_num) {
    running = 0;
}

// ADC 읽기 함수 수정
uint16_t readADC(int channel) {
    uint8_t buffer[3];
    buffer[0] = 1;
    buffer[1] = (8 + channel) << 4;
    buffer[2] = 0;

    wiringPiSPIDataRW(SPI_CHANNEL, buffer, 3);
    return ((buffer[1] & 3) << 8) + buffer[2];
}

// ADC 평균값 읽기 함수 수정
uint16_t readADCAvg(int channel, int samples) {
    uint32_t total = 0;
    for (int i = 0; i < samples; i++) {
        total += readADC(channel);
        usleep(2000); // 2ms 딜레이
    }
    return total / samples;
}

// pH 변환 함수 수정
double convertToPH(double voltage) {
    // 전압 차이를 pH 차이로 변환 (1000을 곱하여 V를 mV로 변환)
    double pH = REFERENCE_PH - ((voltage - REFERENCE_VOLTAGE) * 1000.0 / SLOPE);
    return pH;
}

int main() {
    // wiringPi 초기화를 GPIO 번호 모드로 변경
    if (wiringPiSetupGpio() == -1) {  // wiringPiSetup() 대신 wiringPiSetupGpio() 사용
        printf("초기화 실패\n");
        return -1;
    }

    if (wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) == -1) {
        printf("SPI 초기화 실패\n");
        return -1;
    }

    // GPIO 설정
    pinMode(PUMPS_RELAY_PIN, OUTPUT);
    digitalWrite(PUMPS_RELAY_PIN, LOW);  // 초기 상태는 펌프 끄기

    // 릴레이 초기 상태 확인을 위한 테스트
    printf("릴레이 테스트 시작...\n");
    digitalWrite(PUMPS_RELAY_PIN, HIGH);
    printf("릴레이 ON\n");
    sleep(2);
    digitalWrite(PUMPS_RELAY_PIN, LOW);
    printf("릴레이 OFF\n");
    sleep(2);

    // Ctrl+C 핸들러 설정
    signal(SIGINT, sigintHandler);

    // 이동 평균 필터 초기화
    MovingAverage phFilter;
    MovingAverage waterLevelFilter;
    initMovingAverage(&phFilter);
    initMovingAverage(&waterLevelFilter);

    printf("pH 및 수위 모니터링 시작... Ctrl+C로 종료\n");

    while (running) {
        // pH 센서 읽기 (ADC 채널 0)
        uint16_t phADC = readADCAvg(0, 50);
        double phVoltage = (phADC / 1023.0) * 5.0;
        double phValue = convertToPH(phVoltage);
        double smoothedPH = addToMovingAverage(&phFilter, phValue);

        // 수위 센서 읽기 (ADC 채널 1)
        uint16_t waterADC = readADCAvg(1, 50);
        double waterVoltage = (waterADC / 1023.0) * 5.0;
        double smoothedWaterLevel = addToMovingAverage(&waterLevelFilter, waterVoltage);

        // 수위 상태 결정
        const char* waterStatus;
        if (waterVoltage > 3.35) {
            waterStatus = "정상 수위";
        } else if (waterVoltage < 2.95) {
            waterStatus = "수위 부족";
        } else {
            waterStatus = "경계 수위";
        }

        // 상태 출력
        printf("pH: %.2f, 수위: %.2fV (%s)\n", smoothedPH, smoothedWaterLevel, waterStatus);

        // 펌프 제어 ���직
        if (smoothedPH < PH_THRESHOLD && smoothedWaterLevel > WATER_LEVEL_LOW) {
            digitalWrite(PUMPS_RELAY_PIN, HIGH);
            printf("펌프 작동 중 (pH 낮음)\n");
        } else {
            digitalWrite(PUMPS_RELAY_PIN, LOW);
            if (smoothedPH < PH_THRESHOLD) {
                printf("수위가 낮아 펌프 중지\n");
            } else {
                printf("pH 정상, 펌프 중지\n");
            }
        }

        sleep(1);
    }

    // 종료 시 펌프 끄기
    digitalWrite(PUMPS_RELAY_PIN, LOW);
    printf("\n종료...\n");
    return 0;
}