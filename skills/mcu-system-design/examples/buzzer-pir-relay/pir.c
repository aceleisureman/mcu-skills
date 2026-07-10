#include "pir.h"

#define PIR_DEBOUNCE  3U   /* 连续 3 次(约 30ms)高电平才确认, 避免毛刺 */

static uint8_t pirCount;

void Pir_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = PIR_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;       /* PIR 模块自身有输出电平, 不上下拉 */
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PIR_PORT, &GPIO_InitStruct);

    pirCount = 0U;
}

uint8_t Pir_Triggered(void)
{
    if (HAL_GPIO_ReadPin(PIR_PORT, PIR_PIN) == GPIO_PIN_SET)
    {
        if (pirCount < PIR_DEBOUNCE)
            pirCount++;
    }
    else
    {
        pirCount = 0U;
    }

    return (pirCount >= PIR_DEBOUNCE) ? 1U : 0U;
}

uint8_t Pir_DbgCount(void)
{
    return pirCount;
}
