/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  *
  * Presented by:
  * Michal Ben Haim - *******
  * Shani Haker - *******
  *
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "lcd.h"
#define MAX_CODE_SIZE 5
#define TIMEOUT_MS 50000

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
ADC_HandleTypeDef hadc1;

UART_HandleTypeDef hlpuart1;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */

// flags
volatile uint8_t is_reset = 1; // flags reset or startup
volatile uint8_t is_code_set = 0; // user has set the code's safe
uint8_t is_in_code_attempt = 0; // user starts inserting the code to unlock the safe
uint8_t is_in_code_attempt_done = 0; // user insertion to unlock done - 4 digits found

// counters
uint8_t in_code_counter = 0;
uint8_t in_code_cursor = 0;

// buffers for safe's code + user input code
volatile unsigned char code_buf[MAX_CODE_SIZE];
unsigned char in_code_buf[MAX_CODE_SIZE];

// UART - receive
volatile uint8_t buf_size = 0;
volatile uint8_t ch;

// reading and decoding adc values
uint8_t curr_digit;
uint8_t prev_digit = '5';
uint16_t adc;

// detecting timeout between user inputs to unlock the safe
uint32_t prev_press = 0;
uint32_t curr_press = 0;
uint32_t diff = 0;

// detecting vault reset with blue button
volatile uint16_t ticks_for_reset = 5000; // 0.5 second
volatile uint32_t ticks_counter_reset = 0;

// UART messages
uint8_t set_code_msg[] = "SET CODE (4 digits, 0-4):\r\n"
					 "0=SELECT,\r\n"
				     "1=LEFT,\r\n"
					 "2=UP,\r\n"
					 "3=DOWN,\r\n"
					 "4=RIGHT\r\n";
uint8_t code_success[] = "\r\nCode successfully set!\r\n";
uint8_t reset_msg[] = "** SAFE IS UNDER RESET **\r\n";


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ICACHE_Init(void);
static void MX_ADC1_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_TIM2_Init(void);
static uint16_t ADC_ReadOnce(void);
static uint8_t Keypad_Decode(uint16_t adc);
static void Insert_User_Code(uint8_t digit);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_ICACHE_Init();
  MX_ADC1_Init();
  MX_LPUART1_UART_Init();
  MX_TIM2_Init();
  LCD_Init();
  /* USER CODE BEGIN 2 */

  HAL_TIM_Base_Start(&htim2);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

		// state I --> resetting the safe's code or setting it on startup
		if (is_reset == 1) {

			LCD_Clear();

			// reset counters
			buf_size = 0;
			in_code_counter = 0;
			prev_press = 0;

			// reset flags
			is_reset = 0;
			is_code_set = 0;
			is_in_code_attempt = 0;
			is_in_code_attempt_done = 0;

			HAL_UART_Transmit(&hlpuart1, set_code_msg, sizeof(set_code_msg) - 1, HAL_MAX_DELAY);
			HAL_UART_Receive_IT(&hlpuart1, (uint8_t*) &ch, 1);
		}

		// state II --> the safe's code has been set - ready for unlock attempts
		else if (is_in_code_attempt == 0 && is_code_set == 1) {
			LCD_PrintAt(0, 0, "LOCKED");
			LCD_PrintAt(0, 1, "CODE:----");
			is_in_code_attempt = 1;
		}
		// state III --> attempting to unlock the safe
		else if (is_in_code_attempt_done == 0 && is_in_code_attempt == 1) { // user attempting to unlock the safe - reading adc values
			adc = ADC_ReadOnce();
			curr_digit = Keypad_Decode(adc);

			// polling for first press
			if(prev_press == 0) {
				if (curr_digit != '5' && prev_digit == '5') {
					// reset counter - for finding differences between the following keypad presses
					__HAL_TIM_SET_COUNTER(&htim2, 0);
					Insert_User_Code(curr_digit);
					prev_press = 1;
				}
				prev_digit = curr_digit;
			}
			else { // second press and onward --> continuous polling even if no press is detected to trigger timeout
				curr_press = __HAL_TIM_GET_COUNTER(&htim2);
				diff = curr_press - prev_press;
				if (diff <= TIMEOUT_MS) {

					if (curr_digit != '5' && prev_digit == '5') {
						Insert_User_Code(curr_digit);
						prev_press = curr_press;
					}
					prev_digit = curr_digit;

				} else { // input timeout
					LCD_PrintAt(0, 1, "TIMEOUT   ");
					HAL_Delay(2000);
					LCD_PrintAt(0, 1, "CODE:----");
					in_code_counter = 0;
					prev_press = 0;
				}
			}

			if (in_code_counter == MAX_CODE_SIZE - 1) {
				is_in_code_attempt_done = 1;
			}
		}
		// state IV --> User finished inserting the code for the current attempt
		else if (is_in_code_attempt_done == 1) { //else if (is_in_code_attempt_done == 1 && is_code_set == 1)
			in_code_buf[in_code_counter] = '\0';
			if (strcmp(in_code_buf, code_buf) == 0) {
				LCD_PrintAt(0, 0, "UNLOCKED");
				LCD_PrintAt(0, 1, "OK       ");
			}
			else {
				LCD_PrintAt(0, 1, "ERROR     ");
				HAL_Delay(2000);
				// resetting flags for new attempt of code!
				is_in_code_attempt_done = 0;
				in_code_counter = 0;
				prev_press = 0;
				LCD_PrintAt(0, 1, "CODE:----");
			}
		}

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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 55;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
static void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */

  /** Enable instruction cache in 1-way (direct mapped cache)
  */
  if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/**
  * @brief LPUART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 115200;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  hlpuart1.FifoMode = UART_FIFOMODE_DISABLE;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 11000-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  HAL_PWREx_EnableVddIO2();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, LCD_RS_Pin|LCD_D7_Pin|LCD_D4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, LCD_D6_Pin|LCD_D5_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LCD_BL_Pin|LCD_E_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : BLUE_BUTTON_Pin */
  GPIO_InitStruct.Pin = BLUE_BUTTON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BLUE_BUTTON_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_RS_Pin LCD_D7_Pin LCD_D4_Pin */
  GPIO_InitStruct.Pin = LCD_RS_Pin|LCD_D7_Pin|LCD_D4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_D6_Pin LCD_D5_Pin */
  GPIO_InitStruct.Pin = LCD_D6_Pin|LCD_D5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_BL_Pin LCD_E_Pin */
  GPIO_InitStruct.Pin = LCD_BL_Pin|LCD_E_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI13_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI13_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

// Non-blocking receive callback
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {

	if (huart->Instance == LPUART1) {

		if (buf_size < MAX_CODE_SIZE - 1) {

			// validate the inserted key is a number within the required field 0-4, by ascii value
			if (ch >= 48 && ch <= 52) {
				HAL_UART_Transmit(&hlpuart1, &ch, 1, HAL_MAX_DELAY);
				code_buf[buf_size] = ch;
				buf_size++;
			}
			HAL_UART_Receive_IT(&hlpuart1, (uint8_t*) &ch, 1);
		}
		if (buf_size == MAX_CODE_SIZE - 1) {
			// null terminating the safe's code-string
			code_buf[buf_size] = '\0';
			HAL_UART_Transmit(&hlpuart1, code_success, sizeof(code_success) - 1,
					HAL_MAX_DELAY);

			// reset/startup done - the safe's code is now complete
			is_code_set = 1;
		}
	}
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_PIN) {

	if(GPIO_PIN == BLUE_BUTTON_Pin) {
		__HAL_TIM_SET_COUNTER(&htim2, 0); // setting the timer's counter value to 0
	}
}


void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_PIN) {

	// measure ticks from blue button press to release - long press triggers reset
	if(GPIO_PIN == BLUE_BUTTON_Pin) {
		ticks_counter_reset = __HAL_TIM_GET_COUNTER(&htim2);

		if(ticks_counter_reset >= ticks_for_reset) {
			is_reset = 1;
			HAL_UART_Transmit(&hlpuart1, reset_msg, sizeof(reset_msg) - 1, HAL_MAX_DELAY);
		}
	}
}


// ADC - reads value
static uint16_t ADC_ReadOnce(void) {
	uint16_t value = 0;
	if (HAL_ADC_Start(&hadc1) == HAL_OK) {
		if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
			value = (uint16_t)HAL_ADC_GetValue(&hadc1);
	}
		(void)HAL_ADC_Stop(&hadc1);
	}
	return value;
}


// decoding the values received by ADC
static uint8_t Keypad_Decode(uint16_t adc) {

	if (adc <= 3000 && adc > 1800) {
		return '0'; // select
	} else if (adc <= 1800 && adc > 1200) {
		return '1'; // left
	} else if (adc <= 700 && adc > 200) {
		return '2'; // up
	} else if (adc <= 1200 && adc > 700) {
		return '3'; // down
	} else if (adc <= 200) {
		return '4'; // right
	}
		return '5'; // none
}

static void Insert_User_Code(uint8_t digit) {
	in_code_buf[in_code_counter] = digit;
	in_code_cursor = in_code_counter + 5;
	LCD_PrintAt(in_code_cursor, 1, "*");
	in_code_counter++;
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
