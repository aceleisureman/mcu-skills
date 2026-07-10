#include "door_io.h"

static void DoorIO_Write(GPIO_TypeDef *port, uint16_t pin, uint8_t active, uint8_t on)
{
  GPIO_PinState s;

  if (active != 0U)
    s = (on != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
  else
    s = (on != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET;

  HAL_GPIO_WritePin(port, pin, s);
}

void DoorIO_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* 先把电平拉到关断态(再切换为输出), 避免上电瞬间误鸣/误亮 */
  DoorIO_Write(BUZZER_PORT, BUZZER_PIN, BUZZER_ACTIVE_HIGH, 0U);
  DoorIO_Write(LED_OK_PORT, LED_OK_PIN, LED_ACTIVE_HIGH, 0U);
  DoorIO_Write(LED_ALARM_PORT, LED_ALARM_PIN, LED_ACTIVE_HIGH, 0U);

  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

  gpio.Pin = BUZZER_PIN;
  HAL_GPIO_Init(BUZZER_PORT, &gpio);
  gpio.Pin = LED_OK_PIN;
  HAL_GPIO_Init(LED_OK_PORT, &gpio);
  gpio.Pin = LED_ALARM_PIN;
  HAL_GPIO_Init(LED_ALARM_PORT, &gpio);

  DoorIO_AllOff();
}

void Buzzer_On(void)   { DoorIO_Write(BUZZER_PORT, BUZZER_PIN, BUZZER_ACTIVE_HIGH, 1U); }
void Buzzer_Off(void)  { DoorIO_Write(BUZZER_PORT, BUZZER_PIN, BUZZER_ACTIVE_HIGH, 0U); }
void Led_Ok(uint8_t on)    { DoorIO_Write(LED_OK_PORT, LED_OK_PIN, LED_ACTIVE_HIGH, on); }
void Led_Alarm(uint8_t on) { DoorIO_Write(LED_ALARM_PORT, LED_ALARM_PIN, LED_ACTIVE_HIGH, on); }

void DoorIO_AllOff(void)
{
  Buzzer_Off();
  Led_Ok(0U);
  Led_Alarm(0U);
}

void DoorIO_BeepKey(void)
{
  Buzzer_On();
  HAL_Delay(15);
  Buzzer_Off();
}

void DoorIO_BeepOk(void)
{
  uint8_t i;

  Led_Ok(1U);
  for (i = 0U; i < 2U; i++)
  {
    Buzzer_On();
    HAL_Delay(60);
    Buzzer_Off();
    HAL_Delay(60);
  }
}

void DoorIO_ErrorAlarm(void)
{
  Led_Alarm(1U);
  Buzzer_On();
  HAL_Delay(400);
  Buzzer_Off();
  Led_Alarm(0U);
}

void DoorIO_AlarmPulse(uint8_t on)
{
  if (on != 0U)
  {
    Led_Alarm(1U);
    Buzzer_On();
  }
  else
  {
    Led_Alarm(0U);
    Buzzer_Off();
  }
}
