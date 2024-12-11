#include "adc.h"
#include "config.h"
#include <bcm2835.h>
#include <stdio.h>
#include <unistd.h>

bool adc_init(void) {
    if (!bcm2835_init()) {
        printf("bcm2835 초기화 실패\n");
        return false;
    }

    if (!bcm2835_spi_begin()) {
        printf("SPI 초기화 실패\n");
        bcm2835_close();
        return false;
    }

    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

    return true;
}

uint16_t adc_read(int channel) {
    if (channel < 0 || channel > 7) {
        return 0;
    }

    uint8_t buffer[3] = {0x01, (0x08 + channel) << 4, 0x00};
    bcm2835_spi_transfern((char*)buffer, 3);
    
    return ((buffer[1] & 0x03) << 8) + buffer[2];
}

float adc_to_voltage(uint16_t adc_value) {
    return (adc_value / (float)ADC_MAX_VALUE) * VOLTAGE_REF;
}

void adc_cleanup(void) {
    bcm2835_spi_end();
    bcm2835_close();
}

void adc_reinit(void) {
    bcm2835_spi_end();
    sleep(1);
    if (!bcm2835_spi_begin()) {
        printf("SPI 재초기화 실패\n");
        return;
    }
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
    printf("SPI 재초기화 완료\n");
} 