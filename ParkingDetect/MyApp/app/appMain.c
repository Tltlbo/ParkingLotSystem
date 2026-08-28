// #include "appMain.h"
// #include "audioPass.h"
// #include "sensorADC.h"
// #include "UARTTX.h"
// #include <stdio.h>
// #include "tim.h"
// #include "i2s.h"
// #define PWM_MID_DUTY        (PWM_ARR_MAX / 2)

// extern int16_t mic_dma_buf[AUDIO_BUF_SIZE * 2];
// extern uint16_t adc_dma_buf[CDS_CHANNEL_COUNT];

// void appInit(void) {
//     adcInit();

//     commUartInit();
//     printf("step2: uart done\r\n");

//     audioInit();
//     printf("step3: tim regs set\r\n");

//     HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
//     printf("step4: pwm start done\r\n");

//     __HAL_TIM_MOE_ENABLE(&htim1);
//     printf("step5: moe enable done\r\n");

//     HAL_TIM_Base_Start_IT(&htim1);
//     printf("step6: tim base it start done\r\n");

//     HAL_I2S_Receive_DMA(&hi2s2, (uint16_t *)mic_dma_buf, AUDIO_BUF_SIZE * 2);
//     printf("step7: i2s dma start done\r\n");

//     printf("=== Parking Detect & Audio Pass System Online ===\r\n");
// }

// void appMain(void) {
//     static uint16_t prev_slot_mask = 0xFFFF;
//     static uint32_t prev_dbg_tick = 0;
//     static uint32_t prev_plot_tick = 0;
//     uint32_t now = HAL_GetTick();

//     /* 1. ADC DMA 주차 센서 업데이트 */
//     adcUpdate();

//     /* 2. Teleplot 파형 모니터링 (50ms 주기) */
//     if (now - prev_plot_tick >= 50) {
//         prev_plot_tick = now;
//         printf(">mic_peak:%ld\n>mic_sample:%d\n", audioGetPeakAmplitude(), audioGetLastSample());
//     }

//     /* 3. 주차 슬롯 상태 취합 */
//     uint16_t current_slot_mask = 0;
//     for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
//         if (adcIsOccupied((ParkingSlotID_t)i)) {
//             current_slot_mask |= (1 << i);
//         }
//     }

//     /* 4. 상태 변경 시에만 패킷 송신 */
//     if (current_slot_mask != prev_slot_mask) {
//         prev_slot_mask = current_slot_mask;
//         commUartSendParkingStatus();
//         printf("[EVENT] State Changed! Mask: 0x%02X\r\n", current_slot_mask);
//     }

//     /* 5. 500ms 주기 콘솔 디버그 출력 */
//     if (now - prev_dbg_tick >= 500) {
//         prev_dbg_tick = now;
//     }
// }

#include "appMain.h"
#include "audioPass.h"
#include "sensorADC.h"
#include "UARTTX.h"
#include <stdio.h>

extern int16_t mic_dma_buf[AUDIO_BUF_SIZE * 2];
extern uint16_t adc_dma_buf[CDS_CHANNEL_COUNT];

void appInit(void) {
    adcInit();
    printf("step1: adc done\r\n");

    commUartInit();
    printf("step2: uart done\r\n");

    /* audioInit() 하나로 타이머, PWM, I2S DMA 깔끔하게 시작 */
    audioInit();
    printf("step3: audio pipeline done\r\n");

    printf("=== Parking Detect & Audio Pass System Online ===\r\n");
}

void appMain(void) {
    static uint16_t prev_slot_mask = 0xFFFF;
    static uint32_t prev_dbg_tick = 0;
    static uint32_t prev_plot_tick = 0;
    uint32_t now = HAL_GetTick();

    /* 1. ADC DMA 주차 센서 업데이트 */
    adcUpdate();

    /* 1.5 UART RX DMA 처리 (지속적으로 호출) */
    commUartProcessRx();

    /* 2. Teleplot 파형 모니터링 (50ms 주기) */
    if (now - prev_plot_tick >= 50) {
        prev_plot_tick = now;
        printf(">mic_peak:%ld\n>mic_sample:%d\n", audioGetPeakAmplitude(), audioGetLastSample());
    }

    /* 3. 10개 슬롯 상태 취합 (uint16_t 사용) */
    uint16_t current_slot_mask = 0;
    for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
        if (adcIsOccupied((ParkingSlotID_t)i)) {
            current_slot_mask |= (uint16_t)(1U << i);
        }
    }

    /* 4. 상태 변경 시에만 패킷 송신 */
    if (current_slot_mask != prev_slot_mask) {
        prev_slot_mask = current_slot_mask;
        commUartSendParkingStatus();
        printf("[EVENT] State Changed! Mask: 0x%04X\r\n", current_slot_mask);
    }

    /* 5. 500ms 주기 콘솔 디버그 출력 */
    if (now - prev_dbg_tick >= 500) {
        prev_dbg_tick = now;
        printf("[MIC] Raw: 0x%08lX | Sample: %6d | Peak: %5ld | [SLOT MASK] 0x%04X\r\n",
               (uint32_t)mic_dma_buf[0],
               audioGetLastSample(),
               audioGetPeakAmplitude(),
               current_slot_mask);
    }
}