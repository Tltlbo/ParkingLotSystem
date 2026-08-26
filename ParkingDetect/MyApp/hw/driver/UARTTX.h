#pragma once

#include "main.h"
#include "sensorADC.h"
#include <stdint.h>
#include <stdbool.h>

/* 통신 모듈 초기화 */
void commUartInit(void);

/* 6개 슬롯 상태 문자열 패킷 전송 ($PARK,A-01:1,A-02:0,...\n) */
void commUartSendParkingStatus(void);

/* 특정 슬롯 상태 변경 이벤트 단일 전송 (EVENT,A-01:OCCUPIED\n) */
void commUartSendSlotEvent(ParkingSlotID_t slot, ParkingStatus_t status);