#include "servo.h"

static TIM_HandleTypeDef htim3;
static TIM_OC_InitTypeDef sConfigOC;

void Servo_Init(void)
{
    /* PA7 = TIM3_CH2, PB0 = TIM3_CH3 */
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_AFIO_REMAP_TIM3_DISABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = SERVO_PSC;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = SERVO_ARR;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim3);

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 1500;  /* 90° neutral position */
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3);

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    Servo_SetAngle(0);  /* start at 0° */
    Servo_SetAngle2(0);
}

/*
 * Set servo angle from 0 to 180 degrees.
 * Maps SG90 pulse width: 500us (0°) to 2500us (180°).
 */
void Servo_SetAngle(uint8_t angle)
{
    if (angle > 180) angle = 180;
    uint32_t pulse = SERVO_MIN_PULSE + ((uint32_t)angle * (SERVO_MAX_PULSE - SERVO_MIN_PULSE)) / 180;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pulse);
}

void Servo_SetAngle2(uint8_t angle)
{
    if (angle > 180) angle = 180;
    uint32_t pulse = SERVO_MIN_PULSE + ((uint32_t)angle * (SERVO_MAX_PULSE - SERVO_MIN_PULSE)) / 180;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, pulse);
}

/*
 * Feed action: open gate (120°) for 1 second, then close (0°)
 */
void Servo_Feed(void)
{
    Servo_SetAngle(120);       /* open gate */
    HAL_Delay(1000);           /* hold 1 second */
    Servo_SetAngle(0);         /* close gate */
}

void Servo_Water(void)
{
    Servo_SetAngle2(120);
    HAL_Delay(1000);
    Servo_SetAngle2(0);
}
