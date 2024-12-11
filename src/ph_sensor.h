#ifndef PH_SENSOR_H
#define PH_SENSOR_H

#include "types.h"
#include <stdbool.h>

bool ph_sensor_init(void);
PhData read_ph_with_filtering(void);
void ph_sensor_cleanup(void);

#endif 