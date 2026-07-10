/**
  ******************************************************************************
  * @file    bsp.h
  * @brief   板级支持包：引脚定义与外设初始化（家用智能药箱）
  ******************************************************************************
  */
#ifndef __BSP_H
#define __BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ============================ 引脚分配 ============================ */
/* OLED SSD1306 (I2C1) : PB6=SCL, PB7=SDA                            */
/* HX711 称重          : PA4=SCK(out), PA5=DOUT(in)                  */
/* DHT11 温湿度        : PA8                                         */
/* MQ135 气体          : PB1=AO(ADC1_IN9), PB0=DO(数字量)            */
/* SG90 舵机锁         : PA6 (TIM3_CH1 硬件 PWM 50Hz)              */
/* 门磁(干簧管)        : PA0 (EXTI0)                                 */
/* 按键 K1/K2/K3/K4    : PB13(上) / PB12(下) / PB14(确认) / PB15(模式)*/
/* ESP-01S WiFi (USART2): PA2=TX, PA3=RX                             */
/* JQ8900语音  (USART3): PB10=TX, PB11=RX                            */
/* 状态LED/蜂鸣器      : PC13                                        */

/* HX711 */
#define HX711_SCK_PORT   GPIOA
#define HX711_SCK_PIN    GPIO_PIN_4
#define HX711_DT_PORT    GPIOA
#define HX711_DT_PIN     GPIO_PIN_5

/* DHT11 */
#define DHT11_PORT       GPIOA
#define DHT11_PIN        GPIO_PIN_8

/* MQ135 DO（数字报警输出，可选） */
#define MQ135_DO_PORT    GPIOB
#define MQ135_DO_PIN     GPIO_PIN_0

/* 门磁 */
#define DOOR_PORT        GPIOA
#define DOOR_PIN         GPIO_PIN_0
#define DOOR_EXTI_IRQn   EXTI0_IRQn

/* 按键（低电平有效，内部上拉）: K1=上 K2=下 K3=确认 K4=模式 */
#define KEY_UP_PORT      GPIOB
#define KEY_UP_PIN       GPIO_PIN_13   /* K1 */
#define KEY_DOWN_PORT    GPIOB
#define KEY_DOWN_PIN     GPIO_PIN_12   /* K2 */
#define KEY_OK_PORT      GPIOB
#define KEY_OK_PIN       GPIO_PIN_14   /* K3 */
#define KEY_MODE_PORT    GPIOB
#define KEY_MODE_PIN     GPIO_PIN_15   /* K4 */

/* 状态 LED（板载，低电平点亮） */
#define LED_PORT         GPIOC
#define LED_PIN          GPIO_PIN_13

/* 外设句柄 */
extern I2C_HandleTypeDef  hi2c1;
extern TIM_HandleTypeDef  htim3;    /* SG90 舵机硬件 PWM (TIM3_CH1/PA6) */
extern ADC_HandleTypeDef  hadc1;
extern UART_HandleTypeDef huart2;   /* ESP-01S WiFi */
extern UART_HandleTypeDef huart3;   /* JQ8900 语音  */
extern RTC_HandleTypeDef  hrtc;

/* 初始化入口（在 main() 时钟配置之后调用） */
void BSP_Init(void);

/* 简单工具 */
void BSP_LED_Set(uint8_t on);
void BSP_LED_Toggle(void);
void BSP_DelayUs(uint32_t us);

#ifdef __cplusplus
}
#endif
#endif /* __BSP_H */
