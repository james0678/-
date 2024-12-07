#include <bcm2835.h>
#include <stdio.h>
#include <unistd.h>

#define PUMPS_RELAY_PIN 16 

int main() {
    // BCM2835 라이브러리 초기화
    if (!bcm2835_init()) {
        printf("bcm2835 초기화 실패\n");
        return 1;
    }

    // GPIO 핀 설정
    bcm2835_gpio_fsel(PUMPS_RELAY_PIN, BCM2835_GPIO_FSEL_OUTP);
    
    printf("펌프 테스트를 시작합니다...\n");
    
    while(1) {
        printf("펌프 켜기\n");
        bcm2835_gpio_write(PUMPS_RELAY_PIN, HIGH);
        sleep(3);  // 3초 동안 켜짐
        
        printf("펌프 끄기\n");
        bcm2835_gpio_write(PUMPS_RELAY_PIN, LOW);
        sleep(2);  // 2초 동안 꺼짐
    }

    bcm2835_close();
    return 0;
}