#pragma once

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define HCSR04_SENSOR_COUNT 4

void hcSr04Init(void);
bool hcSr04Read(float *distance_cm,uint8_t num);
float hcSr04GetDistance(uint8_t num);
