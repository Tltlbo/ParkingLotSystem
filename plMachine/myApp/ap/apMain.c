#include "apMain.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_tim.h"
#include "myHcSr04.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

float distance_cm[HCSR04_SENSOR_COUNT] = {0.0f};




void apInit()
{
    hcSr04Init();
    
}

void apMain()

{
    while(1)
    {
      for (uint8_t sensor_num = 0; sensor_num < HCSR04_SENSOR_COUNT; sensor_num++)
      {
        if (!hcSr04Read(&distance_cm[sensor_num], sensor_num))
        {
          distance_cm[sensor_num] = -1.0f;
        }

        printf("dis[%u] : %6.1f\r\n", sensor_num + 1, distance_cm[sensor_num]);
        HAL_Delay(10);
      }
    }


}

