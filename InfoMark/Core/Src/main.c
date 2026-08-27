/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (Chat Display to Full LCD)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "myLcd.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
#define RX_BUF_SIZE 64
uint8_t rx_buf[RX_BUF_SIZE];        // Teleplot -> USART2 수신용 버퍼
uint8_t tx_buf[RX_BUF_SIZE];        // USART2 -> 점퍼선 -> USART1 수신용 버퍼

char lcd_display_buf[RX_BUF_SIZE];  // LCD 출력용 버퍼
volatile uint8_t lcd_update_flag = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN 0 */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 10);
    return ch;
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */

  // 1. LCD 초기화
  lcdInit(&hi2c1, I2C_LCD_ADDR);
  lcdClear();
  lcdSetCursor(0, 0);
  lcdSendString("DMA Chat Ready!");

  // 2. Teleplot 시작 안내
  char init_msg[] = "\r\n[System] InfoMark DMA Pipeline Ready! Enter chat:\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t *)init_msg, strlen(init_msg), 100);

  // 3. USART2, USART1 IDLE DMA 수신 시작
  HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_buf, RX_BUF_SIZE);
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, tx_buf, RX_BUF_SIZE);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      if (lcd_update_flag)
      {
          lcd_update_flag = 0;

          // LCD 화면 클리어 후 새로운 채팅 출력
          lcdClear();
          HAL_Delay(5); // Clear 후 약간의 지연 필요

          char line1[17] = {0};
          char line2[17] = {0};

          // 1번째 줄 출력 (최대 16자)
          strncpy(line1, lcd_display_buf, 16);
          lcdSetCursor(0, 0);
          lcdSendString(line1);

          // 16자를 넘으면 2번째 줄로 연결 출력
          if (strlen(lcd_display_buf) > 16)
          {
              strncpy(line2, lcd_display_buf + 16, 16);
              lcdSetCursor(1, 0);
              lcdSendString(line2);
          }

          printf("\r\n[LCD Printed]: \"%s\"\r\n", lcd_display_buf);
      }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    // [1단계] Teleplot(USART2) 수신 -> USART1(점퍼선)으로 송신
    if (huart->Instance == USART2)
    {
        HAL_UART_DMAStop(&huart2);

        // huart1으로 쏴주어야 USART1의 RX(PA10) DMA가 수신 완료를 감지함
        HAL_UART_Transmit(&huart1, rx_buf, Size, 100);

        memset(rx_buf, 0, Size);
        HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_buf, RX_BUF_SIZE);
    }
    // [2단계] 점퍼선을 통해 USART1 수신 완료 -> LCD 출력
    else if (huart->Instance == USART1)
    {
        HAL_UART_DMAStop(&huart1);

        // 끝의 \r, \n 제거
        while (Size > 0 && (tx_buf[Size - 1] == '\r' || tx_buf[Size - 1] == '\n'))
        {
            Size--;
        }

        if (Size > 0)
        {
            memcpy(lcd_display_buf, tx_buf, Size);
            lcd_display_buf[Size] = '\0';
            lcd_update_flag = 1; // 화면 갱신 트리거
        }

        //memset(tx_buf, 0, sizeof(tx_buf));
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, tx_buf, RX_BUF_SIZE);
    }
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}