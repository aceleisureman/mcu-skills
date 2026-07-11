/**
 * GP2Y1014AU 粉尘传感器示例 — STM32F103 + OLED 显示 + 串口输出
 *
 * 接线:
 *   GP2Y1014AU  →  STM32F103C8T6
 *     VCC       →  5V
 *     GND       →  GND
 *     AO (VOUT) →  PA0 (ADC1_CH0, 经分压板)
 *     ILED      →  PA2 (GPIO 推挽)
 *
 *   OLED 0.96" I2C →  STM32F103C8T6
 *     VCC          →  3.3V
 *     GND          →  GND
 *     SCL          →  PB8
 *     SDA          →  PB9
 *
 *   串口 (USART1, 9600 baud)
 *     RX (白)      →  PA9
 *     TX (绿)      →  PA10
 */

#include "stm32f10x.h"
#include "delay.h"
#include "usart.h"
#include "gp2y1014au.h"
#include "oled.h"
#include <stdio.h>

int main(void)
{
    uint16_t raw, voltage;
    float    density;
    char     buf[32];

    /* 系统初始化 */
    delay_init();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    uart_init(9600);

    /* 模块初始化 */
    gp2y_adc_init();
    gp2y_iled_init();
    OLED_Init();
    OLED_Clear();

    /* 静态界面 */
    OLED_ShowString(0, 0, "GP2Y1014AU", 16);
    OLED_ShowString(0, 2, "Raw:      ", 16);
    OLED_ShowString(0, 4, "Vol:    mV", 16);
    OLED_ShowString(0, 6, "PM :  ug/m3", 16);

    printf("GP2Y1014AU Dust Sensor Ready.\r\n");

    while (1) {
        gp2y_read(&raw, &voltage, &density);

        /* OLED 显示 */
        sprintf(buf, "%4d", raw);
        OLED_ShowString(40, 2, buf, 16);

        sprintf(buf, "%4d", voltage);
        OLED_ShowString(40, 4, buf, 16);

        sprintf(buf, "%4.1f", density);
        OLED_ShowString(40, 6, buf, 16);

        /* 串口打印 */
        printf("Raw:%d, Vol:%d mV, PM:%.1f ug/m3\r\n", raw, voltage, density);

        delay_ms(200);
    }
}
