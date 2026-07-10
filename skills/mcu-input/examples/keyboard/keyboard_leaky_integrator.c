#include "keyboard.h"

static const uint8_t keyMap[4][4] = {
    {KEY_1, KEY_2, KEY_3, KEY_A},
    {KEY_4, KEY_5, KEY_6, KEY_B},
    {KEY_7, KEY_8, KEY_9, KEY_C},
    {KEY_STAR, KEY_0, KEY_HASH, KEY_D}
};

/* 漏积分去抖: 连续多次读到同一键才确认, 偶发漏读不会立刻清零。 */
#define KB_SCORE_MAX 8U
#define KB_CONFIRM   4U

/* 本板实际扫描方向: 输出 PA0/PA1/PB8/PB9, 读取 PB12-PB15。 */
static GPIO_TypeDef *scanRowPorts[4] = {KB_COL0_PORT, KB_COL1_PORT, KB_COL2_PORT, KB_COL3_PORT};
static const uint16_t scanRowPins[4] = {KB_COL0_PIN, KB_COL1_PIN, KB_COL2_PIN, KB_COL3_PIN};
static GPIO_TypeDef *scanColPorts[4] = {KB_ROW0_PORT, KB_ROW1_PORT, KB_ROW2_PORT, KB_ROW3_PORT};
static const uint16_t scanColPins[4] = {KB_ROW0_PIN, KB_ROW1_PIN, KB_ROW2_PIN, KB_ROW3_PIN};

static uint8_t pressedKey = KEY_NONE;
static uint8_t candKey = KEY_NONE;
static uint8_t kbScore = 0U;

static void Keyboard_ConfigNormalPins(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pin = KB_COL0_PIN | KB_COL1_PIN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = KB_COL2_PIN | KB_COL3_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(KB_COL0_PORT, KB_COL0_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(KB_COL1_PORT, KB_COL1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(KB_COL2_PORT, KB_COL2_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(KB_COL3_PORT, KB_COL3_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Pin = KB_ROW0_PIN | KB_ROW1_PIN | KB_ROW2_PIN | KB_ROW3_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static void Keyboard_RowSettle(void)
{
    volatile uint16_t d = 1500U;
    while (d--);
}

static uint8_t Keyboard_ScanMatrix(void)
{
    uint8_t row, col, i;

    for (row = 0U; row < 4U; row++)
    {
        for (i = 0U; i < 4U; i++)
            HAL_GPIO_WritePin(scanRowPorts[i], scanRowPins[i], GPIO_PIN_SET);

        HAL_GPIO_WritePin(scanRowPorts[row], scanRowPins[row], GPIO_PIN_RESET);
        Keyboard_RowSettle();

        for (col = 0U; col < 4U; col++)
        {
            if (HAL_GPIO_ReadPin(scanColPorts[col], scanColPins[col]) == GPIO_PIN_RESET)
            {
                uint8_t ret = keyMap[row][col];
                for (i = 0U; i < 4U; i++)
                    HAL_GPIO_WritePin(scanRowPorts[i], scanRowPins[i], GPIO_PIN_SET);
                return ret;
            }
        }
    }

    for (i = 0U; i < 4U; i++)
        HAL_GPIO_WritePin(scanRowPorts[i], scanRowPins[i], GPIO_PIN_SET);

    return KEY_NONE;
}

void Keyboard_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    Keyboard_ConfigNormalPins();
    pressedKey = KEY_NONE;
    candKey = KEY_NONE;
    kbScore = 0U;
}

uint8_t Keyboard_Scan(void)
{
    uint8_t raw = Keyboard_ScanMatrix();
    uint8_t emit = KEY_NONE;

    if (pressedKey == KEY_NONE)
    {
        if (raw != KEY_NONE)
        {
            if (raw == candKey)
            {
                if (kbScore < KB_SCORE_MAX)
                    kbScore++;
            }
            else
            {
                candKey = raw;
                kbScore = 1U;
            }
        }
        else if (kbScore > 0U)
        {
            kbScore--;
        }

        if (candKey != KEY_NONE && kbScore >= KB_CONFIRM)
        {
            pressedKey = candKey;
            kbScore = KB_SCORE_MAX;
            emit = pressedKey;
        }
    }
    else
    {
        if (raw == pressedKey)
        {
            kbScore = KB_SCORE_MAX;
        }
        else
        {
            if (kbScore > 0U)
                kbScore--;
            if (kbScore == 0U)
            {
                pressedKey = KEY_NONE;
                candKey = raw;
                kbScore = (raw != KEY_NONE) ? 1U : 0U;
            }
        }
    }

    return emit;
}

uint8_t Keyboard_GetKey(void)
{
    uint8_t key;

    do {
        key = Keyboard_Scan();
    } while (key == KEY_NONE);
    return key;
}
