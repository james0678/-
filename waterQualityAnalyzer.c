#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 수질 상태 열거형
typedef enum {
    EXCELLENT,
    GOOD,
    FAIR,
    POOR,
    UNACCEPTABLE,
    UNKNOWN
} WaterQuality;

// pH 상태 열거형
typedef enum {
    TOO_ACIDIC,
    OPTIMAL,
    TOO_ALKALINE
} PHQuality;

// 수질 데이터 구조체
typedef struct {
    double tds;
    double ph;
    char timestamp[30];
    char sensor_id[20];
    char location[50];
} WaterData;

// 수질 평가 결과 구조체
typedef struct {
    WaterQuality overall_quality;
    WaterQuality tds_quality;
    PHQuality ph_quality;
    double quality_score;
} WaterQualityResult;

// TDS 값 평가
WaterQuality evaluate_tds(double tds_value) {
    if (tds_value < 300) return EXCELLENT;
    if (tds_value < 600) return GOOD;
    if (tds_value < 900) return FAIR;
    if (tds_value < 1200) return POOR;
    return UNACCEPTABLE;
}

// pH 값 평가
PHQuality evaluate_ph(double ph_value) {
    if (ph_value < 6.5) return TOO_ACIDIC;
    if (ph_value < 8.5) return OPTIMAL;
    return TOO_ALKALINE;
}

// 수질 점수 계산
double calculate_quality_score(WaterQuality tds_quality, PHQuality ph_quality) {
    double tds_score = 0;
    double ph_score = 0;

    // TDS 점수 계산
    switch (tds_quality) {
        case EXCELLENT: tds_score = 4.0; break;
        case GOOD: tds_score = 3.0; break;
        case FAIR: tds_score = 2.0; break;
        case POOR: tds_score = 1.0; break;
        case UNACCEPTABLE: tds_score = 0.0; break;
        default: tds_score = 0.0;
    }

    // pH 점수 계산
    switch (ph_quality) {
        case OPTIMAL: ph_score = 4.0; break;
        case TOO_ACIDIC:
        case TOO_ALKALINE: ph_score = 1.0; break;
        default: ph_score = 0.0;
    }

    return (tds_score + ph_score) / 2.0;
}

// 종합 수질 평가
WaterQualityResult analyze_water_quality(WaterData data) {
    WaterQualityResult result;
    
    // TDS와 pH 개별 평가
    result.tds_quality = evaluate_tds(data.tds);
    result.ph_quality = evaluate_ph(data.ph);
    
    // 종합 점수 계산
    result.quality_score = calculate_quality_score(result.tds_quality, result.ph_quality);
    
    // 종합 품질 평가
    if (result.quality_score >= 3.5) result.overall_quality = EXCELLENT;
    else if (result.quality_score >= 2.5) result.overall_quality = GOOD;
    else if (result.quality_score >= 1.5) result.overall_quality = FAIR;
    else if (result.quality_score >= 0.5) result.overall_quality = POOR;
    else result.overall_quality = UNACCEPTABLE;
    
    return result;
}

// 수질 상태 문자열 반환
const char* get_quality_string(WaterQuality quality) {
    switch (quality) {
        case EXCELLENT: return "최상";
        case GOOD: return "양호";
        case FAIR: return "보통";
        case POOR: return "나쁨";
        case UNACCEPTABLE: return "부적합";
        default: return "알 수 없음";
    }
}

// pH 상태 문자열 반환
const char* get_ph_quality_string(PHQuality quality) {
    switch (quality) {
        case TOO_ACIDIC: return "산성 과다";
        case OPTIMAL: return "적정";
        case TOO_ALKALINE: return "알칼리성 과다";
        default: return "알 수 없음";
    }
}

// 결과 출력 함수
void print_water_quality_result(WaterData data, WaterQualityResult result) {
    printf("\n======= 수질 분석 결과 =======\n");
    printf("측정 시간: %s\n", data.timestamp);
    printf("측정 위치: %s\n", data.location);
    printf("센서 ID: %s\n\n", data.sensor_id);
    
    printf("측정값:\n");
    printf("- 전도도(TDS): %.2f ppm\n", data.tds);
    printf("- 산성도(pH): %.2f\n\n", data.ph);
    
    printf("수질 평가:\n");
    printf("- 종합 수질 상태: %s (평가 점수: %.2f)\n", 
           get_quality_string(result.overall_quality), 
           result.quality_score);
    printf("- 전도도 상태: %s\n", 
           get_quality_string(result.tds_quality));
    printf("- pH 상태: %s\n", 
           get_ph_quality_string(result.ph_quality));
    
    printf("\n권장사항:\n");
    if (result.overall_quality <= FAIR) {
        if (result.tds_quality > GOOD) {
            printf("- 전도도가 높게 측정되었습니다. 수처리 또는 여과 시스템 점검이 필요합니다.\n");
        }
        if (result.ph_quality != OPTIMAL) {
            printf("- pH 수준이 적정 범위(6.5-8.5)를 벗어났습니다. 조정이 필요합니다.\n");
        }
    } else {
        printf("- 수질 상태가 양호합니다. 정기적인 모니터링을 지속하세요.\n");
    }
    printf("============================\n");
}

// 메인 함수 예시
int main() {
    // 테스트 데이터
    WaterData test_data = {
        .tds = 450.0,
        .ph = 7.2,
        .timestamp = "2024-03-19 10:30:00",
        .sensor_id = "SEN0244",
        .location = "수조_1"
    };
    
    // 수질 분석
    WaterQualityResult result = analyze_water_quality(test_data);
    // 결과 출력
    print_water_quality_result(test_data, result);
    
    return 0;
}