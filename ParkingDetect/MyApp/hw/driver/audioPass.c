#include "audioPass.h"
#include "i2s.h"
#include "tim.h"
#include <stdlib.h>

#define RING_BUF_SIZE       2048
#define PWM_MID_DUTY        (PWM_ARR_MAX / 2) // 420

/* 32비트 수신 버퍼 */
int16_t mic_dma_buf[AUDIO_BUF_SIZE * 2];

/* 링 버퍼 */
static int16_t audio_ring[RING_BUF_SIZE];
static volatile uint16_t ring_head = 0;
static volatile uint16_t ring_tail = 0;

static volatile int32_t g_peak_amplitude = 0;
static volatile int16_t g_last_sample = 0;

static int16_t temp_mono_frame[AUDIO_FRAME_SAMPLES];

/**
 * @brief INMP441 실제 유효 16비트 오디오 추출 (바이트 정렬 보정)
 */
static inline int16_t parse_sample(int32_t raw32) {
    /* 캡처된 실제 유효 PCM 데이터가 들어있는 하위 16비트 추출 */
    return (int16_t)(raw32 >> 16);
}

/**
 * @brief 16비트 PCM -> TIM1 PWM 듀티비 변환
 */
static inline uint16_t pcm_to_pwm(int16_t sample) {
    /* 클리핑 방지 및 적정 볼륨 스케일링 */
    int32_t duty = ((int32_t)sample * (PWM_ARR_MAX / 2)) / 32768 + PWM_MID_DUTY;

    if (duty < 0) duty = 0;
    if (duty > PWM_ARR_MAX) duty = PWM_ARR_MAX;

    return (uint16_t)duty;
}

void audioInit(void) {
    __HAL_TIM_SET_PRESCALER(&htim1, 0);
    __HAL_TIM_SET_AUTORELOAD(&htim1, PWM_ARR_MAX);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_MID_DUTY);

    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    __HAL_TIM_MOE_ENABLE(&htim1);

    /* 100kHz 기본 타이머 인터럽트 활성화 */
    HAL_TIM_Base_Start_IT(&htim1);

    /* I2S DMA 수신 시작 */
    HAL_I2S_Receive_DMA(&hi2s2, (uint16_t *)mic_dma_buf, AUDIO_BUF_SIZE * 2);
}

/* 16.6kHz 속도로 스피커 출력 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        static uint8_t div_cnt = 0;
        if (++div_cnt >= 6) {
            div_cnt = 0;

            if (ring_tail != ring_head) {
                int16_t sample = audio_ring[ring_tail];
                ring_tail = (ring_tail + 1) % RING_BUF_SIZE;
                TIM1->CCR1 = pcm_to_pwm(sample);
            }
        }
    }
}

void audioProcessHalf(void) {
    int32_t peak = 0;
    bool is_mic_active = (HAL_GPIO_ReadPin(USER_BTN_GPIO_Port, USER_BTN_Pin) == GPIO_PIN_RESET);
    uint16_t tx_idx = 0;

    for (int i = 0; i < AUDIO_BUF_SIZE; i += 2) {   /* 4 -> 2 */
        int16_t sample = mic_dma_buf[i];

        if (!is_mic_active) {
            sample = 0;
        }

        audio_ring[ring_head] = sample;
        ring_head = (ring_head + 1) % RING_BUF_SIZE;

        if (tx_idx < AUDIO_FRAME_SAMPLES) {
            temp_mono_frame[tx_idx++] = sample;
        }

        int32_t abs_v = abs((int32_t)sample);
        if (abs_v > peak) peak = abs_v;
    }
    g_peak_amplitude = peak;
    g_last_sample = mic_dma_buf[0];
}

void audioProcessFull(void) {
    int32_t peak = 0;
    bool is_mic_active = (HAL_GPIO_ReadPin(USER_BTN_GPIO_Port, USER_BTN_Pin) == GPIO_PIN_RESET);
    uint16_t tx_idx = 0;

    for (int i = 0; i < AUDIO_BUF_SIZE; i += 2) {   /* 4 -> 2 */
        int16_t sample = mic_dma_buf[AUDIO_BUF_SIZE + i];

        if (!is_mic_active) {
            sample = 0;
        }

        audio_ring[ring_head] = sample;
        ring_head = (ring_head + 1) % RING_BUF_SIZE;

        if (tx_idx < AUDIO_FRAME_SAMPLES) {
            temp_mono_frame[tx_idx++] = sample;
        }

        int32_t abs_v = abs((int32_t)sample);
        if (abs_v > peak) peak = abs_v;
    }
    g_peak_amplitude = peak;
    g_last_sample = mic_dma_buf[AUDIO_BUF_SIZE];
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