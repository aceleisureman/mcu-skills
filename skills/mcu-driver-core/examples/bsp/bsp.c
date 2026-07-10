#include "bsp.h"

I2C_HandleTypeDef  hi2c1;
TIM_HandleTypeDef  htim3;
ADC_HandleTypeDef  hadc1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
RTC_HandleTypeDef  hrtc;

static void DWT_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void BSP_DelayUs(uint32_t us)
{
  uint32_t tick_per_us = HAL_RCC_GetHCLKFreq() / 1000000U;
  uint32_t start = DWT->CYCCNT;
  uint32_t wait = us * tick_per_us;
  while ((DWT->CYCCNT - start) < wait) { }
}

void BSP_LED_Set(uint8_t on)
{
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void BSP_LED_Toggle(void)
{
  HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef g = {0};

  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
  g.Pin = LED_PIN;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_PORT, &g);

  HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_RESET);
  g.Pin = HX711_SCK_PIN;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(HX711_SCK_PORT, &g);

  g.Pin = HX711_DT_PIN;
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(HX711_DT_PORT, &g);

  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
  g.Pin = DHT11_PIN;
  g.Mode = GPIO_MODE_OUTPUT_OD;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DHT11_PORT, &g);

  g.Pin = MQ135_DO_PIN;
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(MQ135_DO_PORT, &g);

  g.Pin = DOOR_PIN;
  g.Mode = GPIO_MODE_IT_RISING_FALLING;
  g.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DOOR_PORT, &g);
  HAL_NVIC_SetPriority(DOOR_EXTI_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DOOR_EXTI_IRQn);

  g.Pin = KEY_UP_PIN | KEY_DOWN_PIN | KEY_OK_PIN | KEY_MODE_PIN;
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &g);
}

/* ============================ I2C1 (OLED) ============================ */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) { Error_Handler(); }
}

/* ==================== TIM3_CH1 (SG90/PA6 硬件 PWM) ====================
 * 1us 计数、周期 20ms => 50Hz；CCR1 即高电平宽度(us)，500~2500 对应 0~180°。 */
static void MX_TIM3_Init(void)
{
  TIM_MasterConfigTypeDef sMaster = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

/* 由实际 APB1 定时器时钟推导预分频，保证 1MHz 计数(1计数=1us)。
     F1 中 APB1 预分频≠1 时定时器时钟为 PCLK1 的2倍；当前 HSI 8MHz，开 PLL 后亦正确。 */
  uint32_t tim_clk = HAL_RCC_GetPCLK1Freq();
  if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0U) { tim_clk *= 2U; }

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = (tim_clk / 1000000U) - 1U;   /* 1MHz counter */
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 20000 - 1;          /* 20ms => 50Hz */
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) { Error_Handler(); }

  sMaster.MasterOutputTrigger = TIM_TRGO_RESET;
  sMaster.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMaster);

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1500;                 /* 中位 1.5ms */
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) { Error_Handler(); }
}

/* ============================ ADC1_IN9 (MQ135/PB1) ============================ */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK) { Error_Handler(); }

  sConfig.Channel = ADC_CHANNEL_9;        /* PB1 = ADC1_IN9 */
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) { Error_Handler(); }

  HAL_ADCEx_Calibration_Start(&hadc1);
}

/* ============================ USART2 (ESP-01S WiFi) ============================ */
static void MX_USART2_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK) { Error_Handler(); }
  __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);   /* ESP-01S 接收中断 */
}

/* ============================ USART3 (JQ8900 语音) ============================ */
static void MX_USART3_Init(void)
{
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 9600;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK) { Error_Handler(); }
}

/* ============================ RTC (LSI) ============================ */
static void MX_RTC_Init(void)
{
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = 40000 - 1;     /* LSI ~40kHz => 1Hz */
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK) { Error_Handler(); }
}

/* ============================ MSP 回调 ============================ */
void HAL_I2C_MspInit(I2C_HandleTypeDef* i2c)
{
  GPIO_InitTypeDef g = {0};
  if (i2c->Instance == I2C1) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /* PB6=SCL, PB7=SDA : 复用开漏 */
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_OD;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &g);
    __HAL_RCC_I2C1_CLK_ENABLE();
  }
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef* tim)
{
  GPIO_InitTypeDef g = {0};
  if (tim->Instance == TIM3) {
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /* PA6 = TIM3_CH1 复用推挽 */
    g.Pin = GPIO_PIN_6;
    g.Mode = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
  }
}

void HAL_ADC_MspInit(ADC_HandleTypeDef* adc)
{
  GPIO_InitTypeDef g = {0};
  if (adc->Instance == ADC1) {
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV6);   /* 64MHz/6 = 10.67MHz <= 14MHz */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /* PB1 = ADC1_IN9 模拟输入 */
    g.Pin = GPIO_PIN_1;
    g.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOB, &g);
  }
}

void HAL_UART_MspInit(UART_HandleTypeDef* uart)
{
  GPIO_InitTypeDef g = {0};
  if (uart->Instance == USART2) {
    /* ESP-01S WiFi : PA2=TX, PA3=RX */
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_2;  g.Mode = GPIO_MODE_AF_PP;      g.Speed = GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_3;  g.Mode = GPIO_MODE_INPUT;      g.Pull = GPIO_PULLUP;           HAL_GPIO_Init(GPIOA, &g);
    HAL_NVIC_SetPriority(USART2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
  } else if (uart->Instance == USART3) {
    /* JQ8900 语音 : PB10=TX, PB11=RX */
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    g.Pin = GPIO_PIN_10; g.Mode = GPIO_MODE_AF_PP;      g.Speed = GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(GPIOB, &g);
    g.Pin = GPIO_PIN_11; g.Mode = GPIO_MODE_INPUT;      g.Pull = GPIO_PULLUP;           HAL_GPIO_Init(GPIOB, &g);
  }
}

void HAL_RTC_MspInit(RTC_HandleTypeDef* rtc)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_PeriphCLKInitTypeDef pclk = {0};
  if (rtc->Instance == RTC) {
    /* 允许访问备份域（RTC/BKP 寄存器） */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* 打开 LSI 并选作 RTC 时钟源 */
    osc.OscillatorType = RCC_OSCILLATORTYPE_LSI;
    osc.LSIState = RCC_LSI_ON;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { Error_Handler(); }

    pclk.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    pclk.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) { Error_Handler(); }

    __HAL_RCC_RTC_ENABLE();
  }
}

/* ============================ 顶层初始化 ============================ */
void BSP_Init(void)
{
  DWT_Init();
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  MX_ADC1_Init();
  MX_USART2_Init();
  MX_USART3_Init();
  MX_RTC_Init();
}
