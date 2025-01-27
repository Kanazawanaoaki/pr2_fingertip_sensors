/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sensor.h"
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

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;
I2C_HandleTypeDef hi2c3;

I2S_HandleTypeDef hi2s2;
DMA_HandleTypeDef hdma_spi2_rx;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi3;
DMA_HandleTypeDef hdma_spi3_rx;
DMA_HandleTypeDef hdma_spi3_tx;

TIM_HandleTypeDef htim2;

osThreadId SPIslaveTaskHandle;
osThreadId IdleTaskHandle;
osThreadId IMUTaskHandle;
osThreadId ADCTaskHandle;
osThreadId TXBUFFTaskHandle;
osThreadId PSTaskHandle;
osThreadId SerialTaskHandle;
osTimerId I2STimerHandle;
osTimerId SPISlaveTimerHandle;
/* USER CODE BEGIN PV */
uint8_t rxBuffer[1] = {};
uint8_t txBuffer[44] = {};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_I2S2_Init(void);
static void MX_SPI3_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C3_Init(void);
static void MX_SPI1_Init(void);
void StartSPIslaveTask(void const * argument);
void StartIdleTask(void const * argument);
void StartIMUTask(void const * argument);
void StartADCTask(void const * argument);
void StartTXBUFFTask(void const * argument);
void StartPSTask(void const * argument);
void StartSerialTask(void const * argument);
void I2SCallback(void const * argument);
void SPISlaveCallback(void const * argument);

/* USER CODE BEGIN PFP */
void mpuWrite(uint8_t, uint8_t);
void imu_init();
void imu_init_i2c();
void imu_update();
void imu_update_i2c();
void imu_update_i2c_DMA_gyro();
void adc_init();
void adc_update();
void ps_init();
void ps_update();
void txbuff_update();
void *memset();
int sprintf();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// PR2に送信したいuint16型の数字を、uint8型の数字2つに分解する。
// 引数のvalは分解元のuint16型の数字
// 引数のdigitは分解された2つ数字のうち上から何番目を取り出すか
uint8_t decomposeDigit(uint16_t val, uint8_t digit){
	uint8_t num;
	num = val >> ((1 - digit) * 8);
	return num;
}

// PR2に送信したいuint16型の数字を、グローバル変数txBuffer[44]に格納する
// 引数のvalは、PR2に送信したいuint16型の数字
// 引数のidxは、PR2のトピック(/pressure/l(r)_gripper_motor)の配列の何番目にvalを格納するか。
// idx = 0, 1, 2, ... 21
// 44個の要素は2個で1つの数字となり、PR2に送信される。
// 1つの数字を2要素に分解するための関数がdecomposeDigit()
void setTxBuffer(uint16_t val, uint8_t idx) {
	for(int i = 0; i < 2; i++) {
	    txBuffer[idx * 2 + i] = decomposeDigit(val, i);
	}
}

float getTimeUs(uint32_t count)
{
  float us = 1000000 * (float)count / (float)SystemCoreClock;
  return us;
}

float getTimeMs(uint32_t count)
{
  float ms = 1000 * (float)count / (float)SystemCoreClock;
  return ms;
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
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_I2S2_Init();
  MX_SPI3_Init();
  MX_TIM2_Init();
  MX_I2C3_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  sp.imu_select = SELECT_ICM_42688_SPI;
  sp.flag = 0;
  sp.error_count = 0;
  sp.rx_counter = 0;
  sp.i2c1_dma_flag = 0x00;
  sp.imu_dma_en = 0x00;
  sp.imu_prev_frame = 0;
  sp.imu_count_frame = 0;
  sp.ps_dma[0] = 0;
  sp.ps_dma[1] = 0;
  //HAL_I2S_DeInit(&hi2s2);
  //HAL_I2S_Init(&hi2s2);
  if(sp.imu_select == SELECT_ICM_20600 || sp.imu_select == SELECT_ICM_42605 || sp.imu_select == SELECT_ICM_42688_SPI){
#if IMU_SPI_MODE
	  imu_init(&hspi1);
#endif
  }else if(sp.imu_select == SELECT_ICM_42688_I2C){
	  //imu_init_i2c(&hi2c1);
	  //imu_init_i2c(&hi2c2);
	  imu_init_i2c(&hi2c3);
  }
  ps_init(&hi2c1);
  memset(sp.txbuff, 0, sizeof(sp.txbuff));

  // /pressure/l(r)_gripper_motor values will be
  // [0, 100, 200, 300, ..., 2100]
  for(int i = 0; i < 22; i++) {
	  setTxBuffer(i*100, i);
  }

  for(int i=0;i<3;i++){
	  sp.acc_print[i] = 0;
	  sp.gyro_print[i] = 0;
  }

#if MIC_DMA
  HAL_I2S_Receive_DMA( &hi2s2, (uint16_t*)&sp.i2s_rx_buff, MIC_BUFF_SIZE);
#endif
  //int8_t ret = HAL_I2S_Receive( &hi2s2, (uint16_t*)&sp.i2s_rx_buff, MIC_BUFF_SIZE ,1);

  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* Create the timer(s) */
  /* definition and creation of I2STimer */
  osTimerDef(I2STimer, I2SCallback);
  I2STimerHandle = osTimerCreate(osTimer(I2STimer), osTimerPeriodic, NULL);

  /* definition and creation of SPISlaveTimer */
  osTimerDef(SPISlaveTimer, SPISlaveCallback);
  SPISlaveTimerHandle = osTimerCreate(osTimer(SPISlaveTimer), osTimerPeriodic, NULL);

  /* USER CODE BEGIN RTOS_TIMERS */
  //osTimerStart(I2STimerHandle, MIC_PERIOD);
#if	!UPDATE_SINGLE_THREAD
#if MIC_TIMER
  uint32_t period = 2.0 * 1000 * MIC_BUFF_SIZE / (hi2s2.Init.AudioFreq);//[ms]
  //osTimerStart(I2STimerHandle, MIC_PERIOD);
  osTimerStart(I2STimerHandle, period);
#endif
#endif
#if SPI_SLAVE
#if  TIMER_SPISLAVE
  osTimerStart(SPISlaveTimerHandle, SPISLAVE_PERIOD);
#endif
#endif
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of SPIslaveTask */
  osThreadDef(SPIslaveTask, StartSPIslaveTask, osPriorityLow, 0, 128);
  SPIslaveTaskHandle = osThreadCreate(osThread(SPIslaveTask), NULL);

  /* definition and creation of IdleTask */
  osThreadDef(IdleTask, StartIdleTask, osPriorityIdle, 0, 128);
  IdleTaskHandle = osThreadCreate(osThread(IdleTask), NULL);

  /* definition and creation of IMUTask */
  osThreadDef(IMUTask, StartIMUTask, osPriorityIdle, 0, 256);
  IMUTaskHandle = osThreadCreate(osThread(IMUTask), NULL);

  /* definition and creation of ADCTask */
  osThreadDef(ADCTask, StartADCTask, osPriorityIdle, 0, 128);
  ADCTaskHandle = osThreadCreate(osThread(ADCTask), NULL);

  /* definition and creation of TXBUFFTask */
  osThreadDef(TXBUFFTask, StartTXBUFFTask, osPriorityIdle, 0, 128);
  TXBUFFTaskHandle = osThreadCreate(osThread(TXBUFFTask), NULL);

  /* definition and creation of PSTask */
  osThreadDef(PSTask, StartPSTask, osPriorityIdle, 0, 128);
  PSTaskHandle = osThreadCreate(osThread(PSTask), NULL);

  /* definition and creation of SerialTask */
  osThreadDef(SerialTask, StartSerialTask, osPriorityIdle, 0, 128);
  SerialTaskHandle = osThreadCreate(osThread(SerialTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  int count = 0;
  // https://garberas.com/archives/244
  HAL_TIM_Encoder_Start( &htim2, TIM_CHANNEL_ALL );
#if PS_I2C_DMA
  ps_update(&hi2c1);
#endif
#if IMU_I2C_DMA
  //imu_update_i2c_DMA_gyro(&hi2c1);
  imu_update_i2c_DMA_gyro(&hi2c3);
#endif
#if SPI_SLAVE
#if SPI_SLAVE_DMA
  HAL_SPI_Receive_DMA(&hspi3, sp.rxbuff, 1);
#endif
#endif
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  /*
	  HAL_GPIO_TogglePin(LD2_GPIO_Port,LD2_Pin);
	  ps_update(&hi2c1);
	  adc_update(&hadc1);
	  imu_update_i2c(&hi2c2);
	  HAL_I2S_Receive( &hi2s2, (uint16_t*)&sp.i2s_rx_buff, MIC_BUFF_SIZE ,1000);
	  for(int i = 0; i < MIC_BUFF_SIZE; i++){
		  sp.i2s_buff_sifted[i] = sp.i2s_rx_buff[i] >> 14;
	  }
	  txbuff_update();
	  if (HAL_SPI_Receive(&hspi3, sp.rxbuff, 1, 1000) != HAL_OK) {
		  uint8_t dummy = sp.rxbuff[0];
	  }
	  if(sp.rxbuff[0] == READ_COMMAND){
		  HAL_SPI_Transmit(&hspi3, sp.txbuff, sizeof(sp.txbuff), 1000);
	  }
	  */
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV6;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the peripherals clocks
  */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_I2C2
                              |RCC_PERIPHCLK_I2C3|RCC_PERIPHCLK_I2S
                              |RCC_PERIPHCLK_ADC12;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
  PeriphClkInit.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
  PeriphClkInit.I2c3ClockSelection = RCC_I2C3CLKSOURCE_PCLK1;
  PeriphClkInit.I2sClockSelection = RCC_I2SCLKSOURCE_HSI;
  PeriphClkInit.Adc12ClockSelection = RCC_ADC12CLKSOURCE_SYSCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
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
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.GainCompensation = 0;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 4;
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
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_8;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x30A0A7FB;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x30A0A7FB;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x30A0A7FB;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief I2S2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S2_Init(void)
{

  /* USER CODE BEGIN I2S2_Init 0 */

  /* USER CODE END I2S2_Init 0 */

  /* USER CODE BEGIN I2S2_Init 1 */

  /* USER CODE END I2S2_Init 1 */
  hi2s2.Instance = SPI2;
  hi2s2.Init.Mode = I2S_MODE_MASTER_RX;
  hi2s2.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s2.Init.DataFormat = I2S_DATAFORMAT_24B;
  hi2s2.Init.MCLKOutput = I2S_MCLKOUTPUT_DISABLE;
  hi2s2.Init.AudioFreq = 64000;
  hi2s2.Init.CPOL = I2S_CPOL_LOW;
  if (HAL_I2S_Init(&hi2s2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S2_Init 2 */

  /* USER CODE END I2S2_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_SLAVE;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_HARD_INPUT;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 7;
  hspi3.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi3.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

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
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4.294967295E9;
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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA2_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel1_IRQn);
  /* DMA2_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel2_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LPUART1_RX_Pin */
  GPIO_InitStruct.Pin = LPUART1_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF12_LPUART1;
  HAL_GPIO_Init(LPUART1_RX_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PB14 */
  GPIO_InitStruct.Pin = GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PD2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 4 */
void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s) {
	// Send I2S data to serial monitor
    for(int i = 0; i < MIC_BUFF_SIZE; i++){
    	sp.i2s_buff_sifted[i] = sp.i2s_rx_buff[i] >> 14;
    }
    //sprintf(buffer, "buff[0]:%d buff[1]:%d buff[2]:%d buff[3]:%d \r\n", buff_sifted[0], buff_sifted[1], buff_sifted[2], buff_sifted[3]);
    //HAL_UART_Transmit(&hlpuart1, buffer, 1024, 10);
    // Receive I2S data again
    HAL_I2S_Receive_DMA( hi2s, (uint16_t*)&sp.i2s_rx_buff, MIC_BUFF_SIZE);
    //HAL_I2S_Receive_DMA( hi2s, (uint16_t*)&I2S_RX_BUFFER, MIC_BUFF_SIZE);
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
	// hspi3: PR2 SPI slave
	if (hspi->Instance == SPI3) {
	  if(sp.rxbuff[0] == READ_COMMAND){
		  sp.rx_counter += 1;
		  //taskENTER_CRITICAL();
		  HAL_SPI_Transmit_DMA(hspi, sp.txbuff, sizeof(sp.txbuff));
		  //taskEXIT_CRITICAL();
	  }else{
		  sp.error_count += 1;
		  HAL_SPI_Receive_DMA(hspi, sp.rxbuff, 1);
	  }
	}
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
	// hspi3: PR2 SPI slave
	if (hspi->Instance == SPI3) {
	  HAL_SPI_Receive_DMA(hspi, sp.rxbuff, 1);
	}
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartSPIslaveTask */
/**
  * @brief  Function implementing the SPIslaveTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartSPIslaveTask */
void StartSPIslaveTask(void const * argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
#if SPI_SLAVE
#if UPDATE_SINGLE_THREAD & !TIMER_SPISLAVE
	  HAL_GPIO_TogglePin(LD2_GPIO_Port,LD2_Pin);

	  //imu_update_i2c(&hi2c2);
	  //if(sp.flag == 1){
	  uint32_t freqCount = htim2.Instance->CNT;
	  float start_time_us = getTimeUs(freqCount);
	  imu_update_i2c(&hi2c1);
	  freqCount = htim2.Instance->CNT;
	  sp.imu_elapsed_time = getTimeUs(freqCount) - start_time_us;

	  //sp.flag = 0;
		  //HAL_I2S_Init(&hi2s2);
	  //}
	  //HAL_Delay(6);
	  freqCount = htim2.Instance->CNT;
	  start_time_us = getTimeUs(freqCount);
	  ps_update(&hi2c1);
	  freqCount = htim2.Instance->CNT;
	  sp.ps_elapsed_time = getTimeUs(freqCount) - start_time_us;

	  freqCount = htim2.Instance->CNT;
	  start_time_us = getTimeUs(freqCount);
	  adc_update(&hadc1);
	  freqCount = htim2.Instance->CNT;
	  sp.adc_elapsed_time = getTimeUs(freqCount) - start_time_us;

	  //if(sp.flag == 0){
		  //int8_t ret = HAL_I2S_Receive(&hi2s2, (uint16_t*)&sp.i2s_rx_buff, MIC_BUFF_SIZE ,1000);
		  //HAL_I2S_DeInit(&hi2s2);
		  //int8_t ret = HAL_OK;
		  //if(ret == HAL_OK){
		  //for(int i = 0; i < MIC_BUFF_SIZE; i++){
			  //sp.i2s_buff_sifted[i] = sp.i2s_rx_buff[i] >> 14;
		  //}
		  //}
		  //sp.flag = 1;
	  //}
	  //HAL_Delay(10);

#endif

#if !SPI_SLAVE_DMA
#if !TIMER_SPISLAVE
#if SPI_SLAVE_SENSOR_EN
	  //if (HAL_SPI_Receive(&hspi3, sp.rxbuff, 1, 1000) != HAL_OK) {
	  if (HAL_SPI_Receive(&hspi3, sp.rxbuff, 1, 1000) != HAL_OK) {
		  uint8_t dummy = sp.rxbuff[0];
	  }
	  if(sp.rxbuff[0] == READ_COMMAND){
		  sp.rx_counter += 1;
		  /*
		  if (HAL_SPI_Transmit(&hspi3, sp.txbuff, sizeof(sp.txbuff), 1000) != HAL_OK) {
			  printf("HAL_SPI_Transmit failed.\r\n");
		  }*/
		  txbuff_update();
		  taskENTER_CRITICAL();
		  //delayUs(10);
		  HAL_SPI_Transmit(&hspi3, sp.txbuff, sizeof(sp.txbuff), 2000);
		  taskEXIT_CRITICAL();
	  }else{
		  sp.error_count += 1;
		  //if(sp.error_count > 0){
			  //HAL_SPI_DeInit(&hspi3);
			  //MX_SPI3_Init();
			  ///sp.error_count = 0;
		  //}
	  }
#else
	  if (HAL_SPI_Receive(&hspi3, rxBuffer, 1, 1000) != HAL_OK) {
		  uint8_t dummy = rxBuffer[0];
	  }
	  if(rxBuffer[0] == READ_COMMAND){
		  /*
		  if (HAL_SPI_Transmit(&hspi3, txBuffer, sizeof(txBuffer), 1000) != HAL_OK) {
			  printf("HAL_SPI_Transmit failed.\r\n");
		  }*/
		  taskENTER_CRITICAL();
		  HAL_SPI_Transmit(&hspi3, txBuffer, sizeof(txBuffer), 1000);
		  taskEXIT_CRITICAL();
	  }
#endif
#endif
#endif
#endif
//	  osDelay(1);
	  osDelay(30);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartIdleTask */
/**
* @brief Function implementing the IdleTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartIdleTask */
void StartIdleTask(void const * argument)
{
  /* USER CODE BEGIN StartIdleTask */
  /* Infinite loop */
  for(;;)
  {
#if !UPDATE_SINGLE_THREAD
	  HAL_GPIO_TogglePin(LD2_GPIO_Port,LD2_Pin);
#endif
	  osDelay(1 + sp.adc_print[0] / 10);//adc read sample
  }
  /* USER CODE END StartIdleTask */
}

/* USER CODE BEGIN Header_StartIMUTask */
/**
* @brief Function implementing the IMUTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartIMUTask */
void StartIMUTask(void const * argument)
{
  /* USER CODE BEGIN StartIMUTask */
  /* Infinite loop */
  for(;;)
  {
#if !UPDATE_SINGLE_THREAD
	  if(sp.imu_select == SELECT_ICM_20600 || sp.imu_select == SELECT_ICM_42605 || sp.imu_select == SELECT_ICM_42688_SPI){
#if IMU_SPI_MODE
		  uint32_t freqCount = htim2.Instance->CNT;
		  float start_time_us = getTimeUs(freqCount);
		  imu_update(&hspi1);
		  freqCount = htim2.Instance->CNT;
		  sp.imu_elapsed_time = getTimeUs(freqCount) - start_time_us;
#endif
	  }else if(sp.imu_select == SELECT_ICM_42688_I2C){
#if !IMU_I2C_DMA
		  //uint32_t freqCount = htim2.Instance->CNT;
		  //float start_time_us = getTimeUs(freqCount);
		  //imu_update_i2c(&hi2c1);
		  //imu_update_i2c(&hi2c2);
		  imu_update_i2c(&hi2c3);
		  //freqCount = htim2.Instance->CNT;
		  //sp.imu_elapsed_time = getTimeUs(freqCount) - start_time_us;
#else
		  /*
		  sp.imu_count_frame = getUs() - sp.imu_prev_frame;
		  if(sp.imu_dma_en == 0 || sp.imu_count_frame > 100000){
			  sp.imu_prev_frame = getUs();
			  imu_update_i2c_DMA_gyro(&hi2c1);
			  sp.imu_dma_en = 1;
		  }
		  */
#endif
	  }
#endif
	  osDelay(1);
  }
  /* USER CODE END StartIMUTask */
}

/* USER CODE BEGIN Header_StartADCTask */
/**
* @brief Function implementing the ADCTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartADCTask */
void StartADCTask(void const * argument)
{
  /* USER CODE BEGIN StartADCTask */
  /* Infinite loop */
  for(;;)
  {
#if !UPDATE_SINGLE_THREAD
	  uint32_t freqCount = htim2.Instance->CNT;
	  float start_time_us = getTimeUs(freqCount);
	  adc_update(&hadc1);
	  freqCount = htim2.Instance->CNT;
	  sp.adc_elapsed_time = getTimeUs(freqCount) - start_time_us;
#endif
	  osDelay(1);
  }
  /* USER CODE END StartADCTask */
}

/* USER CODE BEGIN Header_StartTXBUFFTask */
/**
* @brief Function implementing the TXBUFFTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTXBUFFTask */
void StartTXBUFFTask(void const * argument)
{
  /* USER CODE BEGIN StartTXBUFFTask */
  /* Infinite loop */
  for(;;)
  {
#if !UPDATE_SINGLE_THREAD
	  txbuff_update();
#endif
	  osDelay(1);
  }
  /* USER CODE END StartTXBUFFTask */
}

/* USER CODE BEGIN Header_StartPSTask */
/**
* @brief Function implementing the PSTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartPSTask */
void StartPSTask(void const * argument)
{
  /* USER CODE BEGIN StartPSTask */
  /* Infinite loop */
  for(;;)
  {
#if !PS_I2C_DMA
#if !UPDATE_SINGLE_THREAD
	  uint32_t freqCount = htim2.Instance->CNT;
	  float start_time_us = getTimeUs(freqCount);
	  ps_update(&hi2c1);
	  freqCount = htim2.Instance->CNT;
	  sp.ps_elapsed_time = getTimeUs(freqCount) - start_time_us;
#endif
#endif
    osDelay(1);
  }
  /* USER CODE END StartPSTask */
}

/* USER CODE BEGIN Header_StartSerialTask */
/**
* @brief Function implementing the SerialTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSerialTask */
void StartSerialTask(void const * argument)
{
  /* USER CODE BEGIN StartSerialTask */
	sp.count = 0;
  /* Infinite loop */
  for(;;)
  {
#if DEBUG_EN
	  sprintf(acc_buffer, "imu_en:%d acc[0]:%d acc[1]:%d acc[2]:%d\r\n", sp.imu_en, sp.acc_print[0], sp.acc_print[1], sp.acc_print[2]);
	  //sprintf(acc_buffer, "acc[0]:%d acc[1]:%d acc[2]:%d\r\n", sp.count, sp.acc_print[1], sp.acc_print[2]);
	  sprintf(gyro_buffer, "gyro[0]:%d gyro[1]:%d gyro[2]:%d\r\n", sp.gyro_print[0], sp.gyro_print[1], sp.gyro_print[2]);
	  sprintf(adc_buffer, "adc[0]:%d adc[1]:%d adc[2]:%d adc[3]:%d\r\n", sp.adc_print[0], sp.adc_print[1], sp.adc_print[2], sp.adc_print[3]);
	  sprintf(i2s_buffer, "i2s[0]:%d i2s[1]:%d i2s[2]:%d i2s[3]:%d\r\n", sp.i2s_buff_sifted[0], sp.i2s_buff_sifted[1], sp.i2s_buff_sifted[2], sp.i2s_buff_sifted[3]);
	  sprintf(ps_buffer, "ps[0]:%d\r\n", sp.ps_print[0]);
	  sprintf(debug_buffer, "%s%s%s%s%s\r\n", acc_buffer, gyro_buffer, adc_buffer, i2s_buffer, ps_buffer);
	  HAL_UART_Transmit(&hlpuart1, debug_buffer, 2048, 100);
	  sp.count += 1;
	  if(sp.count == 30){
		  sp.count = 0;
	  }
#endif
	  // 2000[ms] is very important value.
	  // Changing delay time or adding HAL_Delay causes I2S reading error.
	  osDelay(SERIAL_PERIOD);
  }
  /* USER CODE END StartSerialTask */
}

/* I2SCallback function */
void I2SCallback(void const * argument)
{
  /* USER CODE BEGIN I2SCallback */
	  //HAL_Delay(MIC_PERIOD / 2);

	uint32_t freqCount = htim2.Instance->CNT;
	//HAL_I2S_Init(&hi2s2);
	//HAL_Delay(200);
	float start_time_us = getTimeUs(freqCount);

	  //int8_t ret = HAL_I2S_Receive( &hi2s2, (uint16_t*)&sp.i2s_rx_buff, MIC_BUFF_SIZE ,1000);
	taskENTER_CRITICAL();
	int8_t ret = HAL_I2S_Receive( &hi2s2, (uint16_t*)&sp.i2s_rx_buff, MIC_BUFF_SIZE ,1000);
	taskEXIT_CRITICAL();
	  //HAL_I2S_DeInit(&hi2s2);
	  freqCount = htim2.Instance->CNT;
	  sp.mic_elapsed_time = getTimeUs(freqCount) - start_time_us;
	  if(ret == HAL_OK){
		  for(int i = 0; i < MIC_BUFF_SIZE; i++){
			  sp.i2s_buff_sifted[i] = sp.i2s_rx_buff[i] >> 14;
		  }
	  }

	  //uint32_t freqCount = htim2.Instance->CNT;
	  //float start_time_us = getTimeUs(freqCount);
	  //imu_update_i2c(&hi2c2);
	  //imu_update_i2c(&hi2c1);
	  //HAL_Delay(6);
	  //freqCount = htim2.Instance->CNT;
	  //sp.imu_elapsed_time = getTimeUs(freqCount) - start_time_us;
	  //osDelay(MIC_PERIOD / 2);
	  //HAL_Delay(MIC_PERIOD / 2);

  /* USER CODE END I2SCallback */
}

/* SPISlaveCallback function */
void SPISlaveCallback(void const * argument)
{
  /* USER CODE BEGIN SPISlaveCallback */
#if UPDATE_SINGLE_THREAD
	  HAL_SPI_Receive(&hspi3, sp.rxbuff, 1, 1000);
	  if(sp.rxbuff[0] == READ_COMMAND){
		  // taskENTER_CRITICAL();
		  HAL_SPI_Transmit(&hspi3, sp.txbuff, sizeof(sp.txbuff), 1000);
		  // taskEXIT_CRITICAL();
	  }

	  int8_t ret = HAL_I2S_Receive( &hi2s2, (uint16_t*)&sp.i2s_rx_buff, MIC_BUFF_SIZE ,1000);
	  if(ret == HAL_OK){
		  for(int i = 0; i < MIC_BUFF_SIZE; i++){
			  sp.i2s_buff_sifted[i] = sp.i2s_rx_buff[i] >> 14;
		  }
	  }
#endif
	  osDelay(SPISLAVE_PERIOD / 2);

  /* USER CODE END SPISlaveCallback */
}

 /**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
