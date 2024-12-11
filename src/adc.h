#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include <stdbool.h>

bool adc_init(void);
void adc_cleanup(void);
uint16_t adc_read(int channel);
void adc_reinit(void);
float adc_to_voltage(uint16_t adc_value);

#endif 