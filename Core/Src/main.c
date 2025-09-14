/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
typedef void (*ptrF)(uint32_t dlyticks);
typedef void (*pFunction)(void);
//-------------API HEADER'in olusturulmasi--------------------
// burada bootloader ve api ayni yapiyi kullanmasi gerektigi icin bir template sarti koyuyoruz
// butlooder basta api su yapida diye belirtir ->struct bootloder_api{}
// bu hedarin aynisi api'de de tanimlanir boylelikle alanlarin sirasi ve imzasi ayni kalir
struct BootloaderSharedAPI
{
	void(*blink)(uint32_t dlyticks);
	void(*turn_on_led)(void);
	void(*turn_off_ked)(void);
};
// daha sonra boot tarafi icin tabloyu dolduracagiz
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
#ifdef TUTORIAL
unsigned char __attribute__((section(".myBufSectionRAM"))) buf_ram[128];
const unsigned char __attribute__((section(".myBufSectionFLASH"))) buf_flash[10] = {0,1,2,3,4,5,6,7,8,9};
#endif

#define LOCATE_FUNC __attribute__((section(".mysection")))
#define FLASH_APP_ADDR 0x8008000

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
int _write(int file, char*ptr, int len);
void LOCATE_FUNC blink(uint32_t delay_tick); // bu fonksiyonlar MY_MEMORY icindeki msection section'inda
void LOCATE_FUNC turn_on_led(void);
void LOCATE_FUNC turn_off_led(void);
void __attribute__((__section__(".RamFunc"))) TurnOnLED(GPIO_PinState PinState);
#ifdef TUTORIAL
static ptrF Functions[] =
{
		blink
};
#endif
void go2APP(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void LOCATE_FUNC blink(uint32_t delay_tick)
{
	HAL_GPIO_TogglePin(USER_LED_GPIO_Port, USER_LED_Pin);
	HAL_Delay(delay_tick);
}

void __attribute__((__section__(".RamFunc"))) TurnOnLED(GPIO_PinState PinState)
{
	if(PinState != GPIO_PIN_RESET)
	{
		USER_LED_GPIO_Port->BSRR = (uint32_t)USER_LED_Pin;
	}
	else
	{
		USER_LED_GPIO_Port->BSRR = (uint32_t)USER_LED_Pin;
	}
}

/*FLASH
0x08004000  +-----------------------------+  ← APP2 tabanı = VTOR değeri
            | VEKTÖR TABLOSU              |  [0] = başlangıç MSP (stack tepe değeri)
            |  - [0] init MSP             |  [1] = Reset_Handler adresi
            |  - [1] Reset_Handler        |  [2..] ISR adresleri
            +-----------------------------+
            | .text  (kod)                |
            | .rodata (const)             |
            | .data'nın FLASH kopyası     |  (resette RAM’e kopyalanır)
            +-----------------------------+
            | (kalan flash)               |
            +-----------------------------+
 * */

/*RAM
0x20008000  +-----------------------------+  ← _estack (MSP’nin başlangıç değeri)
            |           STACK             |  ↓ aşağı doğru büyür
            |  (fonksiyon çağrıları,      |
            |   ISR'ler burada çerçeve     |
            |   açar-kapatır)             |
            +-----------------------------+
            |        serbest alan         |  (stack ↔ heap tampon bölge)
            +-----------------------------+
            |           HEAP              |  ↑ yukarı büyür (malloc/new)
            +-----------------------------+
            | .bss  (sıfırlanır)          |
            | .data (FLASH’tan kopya)     |
0x20000000  +-----------------------------+
 *
 * */

void go2APP(void)
{
	uint32_t jump_address;
	pFunction jump_to_app;
	printf("[SYSTEM]: BOOTLOADER START\r\n");

	// --------------  flash app bolgesinde yuklu bir app var mi diye kontrol ediyoruz --------------------------------
	// ( *(uint32_t*)FLASH_APP_ADDR ) -> vektor tablosundaki ilk word'u okur  yani MSP degerine bakiyoruz
	// MSP degeri RAM'deki stack degerinin tepesini gosteriyor
	// & 0x2FFE0000 -> bu maskelemenin amaci MSP degeri gercekten de RAM'i isaret ediyor mu
	// MSP RAM'i gosteriyorsa -> burada gecerli bir app imaji var (kaba bir kontrol)
	if(  (( *(uint32_t*)FLASH_APP_ADDR ) & 0x2FFE0000) == 0x20000000  )
	{
		printf("[SYSTEM]: APP START\r\n");
		HAL_Delay(100);

			// --------------------app'e atlama----------------------------------------
		jump_address = *(uint32_t*)(FLASH_APP_ADDR + 4); // Reset handler adresini jump_addr olarak tut
		// bu deger reset handler fonksiyonuna dallanacagimiz giris adresi
		// neden buna ziplayacagiz -> cunku reset handler : .data kopyalama, .bss sifirlama, system clocklarini kurmayi halledip main()'e geciren gercek baslangic noktasi
		jump_to_app = (pFunction) jump_address; // artik fonksiyon pointer'la fonksiyonlara erisebilirim

		__set_MSP(*(uint32_t*)(FLASH_APP_ADDR)); // app'in stack pointerini kuruyoruz
		jump_to_app();                           // app'in reset handler'ini cagiriyoruz
	}
	else
	{
		printf("[SYSTEM]: APP NOT FOUND\r\n");
	}
}

int _write(int file, char*ptr, int len)
{
	int data_idx;

	for(data_idx = 0; data_idx < len; data_idx++)
		HAL_UART_Transmit(&huart2, (uint8_t*)ptr++, 1, 100);

	return len;
}

void LOCATE_FUNC turn_on_led(void)
{
	HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, GPIO_PIN_SET);
}

void LOCATE_FUNC turn_off_led(void)
{
	HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, GPIO_PIN_RESET);
}

//--------------------------API TABLOSUNU DOLDURMA (HEADER REFARANSINA GORE OLMALI)---------------
// iste bu asamada gercekten bu api tablosunu dolduruyor, fonksiyon adreslerini iceren bir struct
// bu fonksiyon adresleri -> api'daki gercek fonksiyonlari isaret ediyor, apiya dallandiriyor
struct BootloaderSharedAPI api __attribute__((section(".API_SHARED"))) ={
		blink,
		turn_on_led,
		turn_off_led
};
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
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  //blink(100);
	  //TurnOnLED(0);
#ifdef TUTORIAL
	  (*Functions[0])(100);
#endif
	  go2APP();
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLL_DIV3;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : USER_LED_Pin */
  GPIO_InitStruct.Pin = USER_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(USER_LED_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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

#ifdef  USE_FULL_ASSERT
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
