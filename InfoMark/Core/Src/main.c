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
#include "parkingModel.h"
#include "parkingDisplay.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
#define RX_BUF_SIZE 256
uint8_t rx_buf[RX_BUF_SIZE];        // Teleplot -> USART2 수신용 버퍼
uint8_t tx_buf[RX_BUF_SIZE];        // USART2 -> 점퍼선 -> USART1 수신용 버퍼

char lcd_display_buf[RX_BUF_SIZE];  // LCD 출력용 버퍼
volatile uint8_t lcd_update_flag = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void parkingLcdRender(const ParkingSystem_t *sys);
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 10);
    return ch;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  // 0. I2C 버스 스캐너 실행 (연결된 모든 I2C 디바이스 감지)
  printf("\r\n--- Scanning I2C bus ---\r\n");
  for (uint8_t i = 1; i < 128; i++) {
      if (HAL_I2C_IsDeviceReady(&hi2c1, (i << 1), 1, 10) == HAL_OK) {
          printf("[I2C Found] Device address: 0x%02X (8-bit: 0x%02X)\r\n", i, (i << 1));
      }
  }
  printf("------------------------\r\n");

  // 주차 모델 초기화 (기본 10개 슬롯 활성화)
  parkingModelInit();

  // 1. LCD 초기화 및 초기 주차 현황 출력
  lcdInit(&hi2c1, I2C_LCD_ADDR);
  lcdClear();
  parkingLcdRender(parkingModelGetSystem());

  // OLED 주차 디스플레이 초기화 (LCD와 동일한 I2C 버스 공유)
  parkingDisplayInit(&hi2c1);
  parkingDisplayRenderSlots(parkingModelGetSystem());

  // 2. Teleplot 시작 안내
  char init_msg[] = "\r\n[System] InfoMark DMA Pipeline Ready! Enter chat:\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t *)init_msg, strlen(init_msg), 100);

  HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_buf, RX_BUF_SIZE);
  __HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT);

  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, tx_buf, RX_BUF_SIZE);
  __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      if (lcd_update_flag)
      {
        lcd_update_flag = 0;
        parkingLcdRender(parkingModelGetSystem());
        parkingDisplayRenderSlots(parkingModelGetSystem());
        
        /* Teleplot용 출력 */
        const ParkingSystem_t *sys = parkingModelGetSystem();
        printf(">EmptyCount:%d\r\n>OccupiedCount:%d\r\n", sys->empty_count, sys->occupied_count);
        for (int j = 0; j < MAX_PARKING_SLOTS; j++) {
            if (sys->slots[j].is_valid) {
                printf(">%c%02d:%d\r\n", sys->slots[j].section, sys->slots[j].number, sys->slots[j].is_occupied ? 1 : 0);
            }
        }
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
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
    // [2단계] 점퍼선 또는 외부에서 USART1 수신 완료 -> LCD 출력
    else if (huart->Instance == USART1)
    {
        HAL_UART_DMAStop(&huart1);

        while (Size > 0 && (tx_buf[Size - 1] == '\r' || tx_buf[Size - 1] == '\n' || tx_buf[Size - 1] == 0)) {
            Size--;
        }

        if (Size > 0) {
            uint16_t pkt_idx = 0;
            bool found = false;
            for (uint16_t i = 0; i < Size - 4; i++) {
                if (tx_buf[i] == '$' && tx_buf[i+1] == 'P' && tx_buf[i+2] == 'A' && tx_buf[i+3] == 'R' && tx_buf[i+4] == 'K') {
                    pkt_idx = i;
                    found = true;
                    break;
                }
                if (i < Size - 5 && tx_buf[i] == '$' && tx_buf[i+1] == 'E' && tx_buf[i+2] == 'V' && tx_buf[i+3] == 'E' && tx_buf[i+4] == 'N' && tx_buf[i+5] == 'T') {
                    pkt_idx = i;
                    found = true;
                    break;
                }
            }

            if (found) {
                uint16_t copy_len = Size - pkt_idx;
                if (copy_len >= sizeof(lcd_display_buf)) copy_len = sizeof(lcd_display_buf) - 1;
                memcpy(lcd_display_buf, &tx_buf[pkt_idx], copy_len);
                lcd_display_buf[copy_len] = '\0';
                
                // 중간에 있는 null 문자들을 모두 공백으로 변환 (strtok 등에서 끊기지 않도록)
                for(uint16_t i=0; i<copy_len; i++){
                    if(lcd_display_buf[i] == '\0') {
                        lcd_display_buf[i] = ' ';
                    }
                }

                /* 1. 수신 패킷 파싱 */
                if (parkingModelParsePacket(lcd_display_buf)) {
                    lcd_update_flag = 1; /* 파싱 성공 시 LCD/OLED 갱신 플래그 ON */
                }
            }
            
            /* UART2로 원본 수신 디버그 출력 (주석 처리) */
            // printf("\r\n[UART1 Rx (%d bytes)] %s (HEX:", Size, lcd_display_buf);
            // for (int i = 0; i < Size && i < 16; i++) {
            //     printf(" %02X", (uint8_t)lcd_display_buf[i]);
            // }
            // printf(")\r\n");
        }

        memset(tx_buf, 0, sizeof(tx_buf));
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, tx_buf, RX_BUF_SIZE);
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        /* 오버런 등 에러 플래그 클리어 후 DMA 재시작 */
        // __HAL_UART_CLEAR_OREFLAG(huart);
        // HAL_UARTEx_ReceiveToIdle_DMA(&huart1, tx_buf, RX_BUF_SIZE);
        __HAL_UART_CLEAR_OREFLAG(huart);
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, tx_buf, RX_BUF_SIZE);
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    }
    else if (huart->Instance == USART2)
    {
        // __HAL_UART_CLEAR_OREFLAG(huart);
        // HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_buf, RX_BUF_SIZE);
        __HAL_UART_CLEAR_OREFLAG(huart);
        HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_buf, RX_BUF_SIZE);
        __HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT);
    }
}

void parkingLcdRender(const ParkingSystem_t *sys) {
    char line1[17];
    char line2[17];

    /* 1번째 줄: 전체 빈자리 현황 (예: "Empty: 4 / 6") */
    char temp_line1[32];
    snprintf(temp_line1, sizeof(temp_line1), "PARK Empty:%u/%u", sys->empty_count, sys->total_count);
    snprintf(line1, sizeof(line1), "%-16s", temp_line1);

    /* 2번째 줄: 각 구역별 슬롯 상태 요약 ('X': 주차됨, 'O': 빈자리) */
    // /* 예: "A: O X O O X O" */
    // int offset = snprintf(line2, sizeof(line2), "A:");
    // for (int i = 0; i < sys->total_count && offset < 16; i++) {
    //     if (sys->slots[i].is_valid && (sys->slots[i].section == 'A')) {
    //         offset += snprintf(line2 + offset, sizeof(line2) - offset, " %c", 
    //                            sys->slots[i].is_occupied ? 'X' : 'O');
    //     }
    // }

    // /* 디버그 출력 또는 실제 LCD 드라이버 함수 호출 */
    // printf("\r\n--- LCD DISPLAY --- \r\n");
    // printf("Line 1: %s\r\n", line1);
    // printf("Line 2: %s\r\n", line2);
    // printf("-------------------\r\n");

    char status_str[11] = {0};
    for (int i = 0; i < sys->total_count && i < 10; i++) {
        if (sys->slots[i].is_valid) {
            status_str[i] = sys->slots[i].is_occupied ? 'X' : 'O';
        } else {
            status_str[i] = '-';
        }
    }
    
    char temp_line2[32];
    snprintf(temp_line2, sizeof(temp_line2), "S:%s", status_str);
    snprintf(line2, sizeof(line2), "%-16s", temp_line2);

    
    lcdSetCursor(0, 0);
    lcdSendString(line1);
    lcdSetCursor(1, 0);
    lcdSendString(line2);
    
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
