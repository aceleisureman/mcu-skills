#include "key.h"

/* 引脚表: 索引 0..2 对应 PB12/PB13/PB15 -> 布防/撤防/SOS */
static GPIO_TypeDef *const keyPort[3] = {GPIOB, GPIOB, GPIOB};
static const uint16_t         keyPin[3] = {GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_15};
static const uint8_t          keyCode[3] = {KEY_ARM, KEY_DISARM, KEY_SOS};

#define KEY_NUM       3U
#define KEY_DEBOUNCE  2U   /* 连续两次(约 20ms)读到按下才确认 */

static uint8_t keyCount[KEY_NUM];   /* 去抖计数 */
static uint8_t keyPressed[KEY_NUM]; /* 已确认的按下状态 */

void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    uint8_t i;

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    for (i = 0U; i < KEY_NUM; i++)
    {
        keyCount[i] = 0U;
        keyPressed[i] = 0U;
    }
}

uint8_t Key_Scan(void)
{
    uint8_t i;
    uint8_t emit = KEY_NONE;

    for (i = 0U; i < KEY_NUM; i++)
    {
        uint8_t raw = (HAL_GPIO_ReadPin(keyPort[i], keyPin[i]) == GPIO_PIN_RESET) ? 1U : 0U;

        if (raw != 0U)
        {
            if (keyCount[i] < KEY_DEBOUNCE)
                keyCount[i]++;
        }
        else
        {
            keyCount[i] = 0U;
        }

        if (keyCount[i] >= KEY_DEBOUNCE)
        {
            if (keyPressed[i] == 0U)        /* 按下沿: 返回该键码 */
            {
                keyPressed[i] = 1U;
                if (emit == KEY_NONE)
                    emit = keyCode[i];
            }
        }
        else
        {
            keyPressed[i] = 0U;             /* 已松开, 允许下次按下沿 */
        }
    }

    return emit;
}
