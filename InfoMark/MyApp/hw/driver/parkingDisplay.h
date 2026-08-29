#pragma once

#include "main.h"
#include "parkingModel.h"

// I2C 주소 정의
#define I2C_OLED_ADDR (0x3C << 1)

// 주차 구역 UI 렌더링 초기화
void parkingDisplayInit(I2C_HandleTypeDef *hi2c);

// 주차 상태를 받아서 화면에 렌더링
void parkingDisplayRenderSlots(const ParkingSystem_t *sys);
