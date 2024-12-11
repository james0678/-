// # 필요한 라이브러리 설치
// sudo apt-get install libcurl4-openssl-dev libjson-c-dev wiringpi

// # 컴파일
// gcc -o pumpControl pumpControl.c -lwiringPi -lsqlite3

// # 실행
// sudo ./pumpControl

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <signal.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <sqlite3.h>

#define SPI_CHANNEL 0
#define SPI_SPEED 1350000
#define QUEUE_SIZE 10
#define PUMPS_RELAY_PIN 16  // GPIO 16 (물리적 핀 36)

// pH 센서 관련 상수
#define REFERENCE_VOLTAGE 4.99  // 기준 전압
#define REFERENCE_PH 6.00      // 기준 pH
#define SLOPE -59.2            // mV/pH at 25°C (네른스트 방정식)
#define PH_TOLERANCE 0.5       // pH 허용 오차 범위

// 상수 정의 수정
#define TARGET_PH 7.0        // 목표 pH 값
#define PH_MIN 6.5          // 최소 허용 pH
#define PH_MAX 7.5          // 최대 허용 pH
#define DB_PATH "/path/to/your/database.db"  // SQLite DB 경로

static int running = 1;

// 이동 평균 필터 구조체
typedef struct {
    double queue[QUEUE_SIZE];
    int head;
    int count;
} MovingAverage;

// API 응답을 위한 구조체
struct MemoryStruct {
    char *memory;
    size_t size;
};

// API 응답 처리 콜백
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) return 0;
    
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    
    return realsize;
}

// pH 측정값 가져오기
double fetch_current_ph() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    double current_ph = 7.0;  // 기본값
    
    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        printf("데이터베이스 열기 실패: %s\n", sqlite3_errmsg(db));
        return current_ph;
    }
    
    const char *sql = "SELECT ph_value FROM ph_measurements ORDER BY timestamp DESC LIMIT 1;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            current_ph = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    sqlite3_close(db);
    return current_ph;
}

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

int main() {
    // wiringPi 초기화
    if (wiringPiSetupGpio() == -1) {
        printf("초기화 실패\n");
        return -1;
    }

    // SPI 초기화
    if (wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) == -1) {
        printf("SPI 초기화 실패\n");
        return -1;
    }

    // GPIO 설정
    pinMode(PUMPS_RELAY_PIN, OUTPUT);
    digitalWrite(PUMPS_RELAY_PIN, LOW);  // 초기 상태는 펌프 끄기

    // 펌프 연결 테스트
    printf("수중 펌프 연결 테스트 시작...\n");
    printf("펌프 켜기...\n");
    digitalWrite(PUMPS_RELAY_PIN, HIGH);
    sleep(1);
    printf("펌프 끄기...\n");
    digitalWrite(PUMPS_RELAY_PIN, LOW);
    sleep(1);
    printf("펌프 테스트 완료!\n\n");

    // Ctrl+C 핸들러 설정
    signal(SIGINT, sigintHandler);

    // 이동 평균 필터 초기화
    MovingAverage phFilter;
    initMovingAverage(&phFilter);

    printf("pH 모니터링 및 펌프 제어 시작... Ctrl+C로 종료\n");

    while (running) {
        // pH 측정값 가져오기
        double phValue = fetch_current_ph();
        double smoothedPH = addToMovingAverage(&phFilter, phValue);

        // 상태 결정 및 펌프 제어
        const char* water_status;
        if (smoothedPH < PH_MIN || smoothedPH > PH_MAX) {
            water_status = "물이 더러움 - 정화 필요";
            digitalWrite(PUMPS_RELAY_PIN, HIGH);  // 펌프 켜기
            printf("펌프 작동 중 - pH: %.2f (허용 범위: %.1f ~ %.1f)\n", 
                   smoothedPH, PH_MIN, PH_MAX);
        } else {
            water_status = "물이 깨끗함";
            digitalWrite(PUMPS_RELAY_PIN, LOW);   // 펌프 끄기
            printf("정상 상태 - pH: %.2f (허용 범위: %.1f ~ %.1f)\n", 
                   smoothedPH, PH_MIN, PH_MAX);
        }

        // 상태 출력
        printf("pH: %.2f, 상태: %s\n", smoothedPH, water_status);
        
        sleep(1);
    }

    // 종료 시 펌프 끄기
    digitalWrite(PUMPS_RELAY_PIN, LOW);
    printf("\n종료...\n");
    return 0;
}