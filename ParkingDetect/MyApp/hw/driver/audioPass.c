#include "audioPass.h"
#include "i2s.h"
#include "tim.h"
#include <stdlib.h>

/* I2S DMA 수신 버퍼 (32비트 단위) */
static int32_t mic_dma_buf[AUDIO_BUF_SIZE * 2];

static volatile int32_t g_peak_amplitude = 0;
static volatile int16_t g_last_sample = 0;

/**
 * @brief INMP441 샘플(16비트 오디오)을 TIM1 PWM 듀티비(0 ~ 839)로 매핑
 */
static inline uint16_t pcm_to_pwm(int16_t sample) {
    /* 마이크 입력 신호 증폭을 위해 4배 게인(Gain) 적용 */
    int32_t amplified = (int32_t)sample * 4;

    /* -32768 ~ +32767 범위를 0 ~ PWM_ARR_MAX (중앙값: 420)으로 변환 */
    int32_t pwm_val = (amplified * (PWM_ARR_MAX / 2)) / 32768 + (PWM_ARR_MAX / 2);

    if (pwm_val < 0) pwm_val = 0;
    if (pwm_val > PWM_ARR_MAX) pwm_val = PWM_ARR_MAX;

    return (uint16_t)pwm_val;
}

static inline int16_t parse_sample(int32_t raw32) {
    return (int16_t)(raw32 >> 16);
}

void audioInit(void) {
    /* 1. TIM1 설정 (84MHz 기준 Prescaler 0, ARR 839 -> 100kHz PWM 캐리어) */
    __HAL_TIM_SET_PRESCALER(&htim1, 0);
    __HAL_TIM_SET_AUTORELOAD(&htim1, PWM_ARR_MAX);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_ARR_MAX / 2);

    /* 2. TIM1 PWM 시작 및 출력 활성화 */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    __HAL_TIM_MOE_ENABLE(&htim1);

    /* 3. I2S 마이크 수신 DMA 시작 (PWM DMA는 사용하지 않음) */
    HAL_I2S_Receive_DMA(&hi2s2, (uint16_t *)mic_dma_buf, AUDIO_BUF_SIZE * 2);
}

/* 전반부 수신 처리 */
void audioProcessHalf(void) {
    int32_t peak = 0;
    for (int i = 0; i < AUDIO_BUF_SIZE; i++) {
        int16_t sample = parse_sample(mic_dma_buf[i]);

        /* 수음된 마지막 샘플의 듀티비를 PWM 레지스터에 실시간 반영 */
        TIM1->CCR1 = pcm_to_pwm(sample);

        int32_t abs_val = abs((int32_t)sample);
        if (abs_val > peak) peak = abs_val;
    }
    g_peak_amplitude = peak;
    g_last_sample = parse_sample(mic_dma_buf[0]);
}

/* 후반부 수신 처리 */
void audioProcessFull(void) {
    int32_t peak = 0;
    for (int i = 0; i < AUDIO_BUF_SIZE; i++) {
        int16_t sample = parse_sample(mic_dma_buf[AUDIO_BUF_SIZE + i]);

        /* 수음된 마지막 샘플의 듀티비를 PWM 레지스터에 실시간 반영 */
        TIM1->CCR1 = pcm_to_pwm(sample);

        int32_t abs_val = abs((int32_t)sample);
        if (abs_val > peak) peak = abs_val;
    }
    g_peak_amplitude = peak;
    g_last_sample = parse_sample(mic_dma_buf[AUDIO_BUF_SIZE]);
}

int32_t audioGetPeakAmplitude(void) { return g_peak_amplitude; }
int16_t audioGetLastSample(void) { return g_last_sample; }

void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s) {
    if (hi2s->Instance == SPI2) {
        audioProcessHalf();
    }
}

void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s) {
    if (hi2s->Instance == SPI2) {
        audioProcessFull();
    }
}