#include "keyboard.h"

static const uint8_t keyMap[4][4] = {
    {KEY_1, KEY_2, KEY_3, KEY_A},
    {KEY_4, KEY_5, KEY_6, KEY_B},
    {KEY_7, KEY_8, KEY_9, KEY_C},
    {KEY_STAR, KEY_0, KEY_HASH, KEY_D}
};

#define KB_DEBOUNCE_MS 20U

/* Drive the PB group to avoid touch noise on PB12-PB15.
 * PCB left-to-right columns: PB12, PB13, PB14, PB15.
 * PCB top-to-bottom rows: PA0, PA1, PB8, PB9.
 */
static GPIO_TypeDef *drivePorts[4] = {KB_ROW0_PORT, KB_ROW1_PORT, KB_ROW2_PORT, KB_ROW3_PORT};
static const uint16_t drivePins[4] = {KB_ROW0_PIN, KB_ROW1_PIN, KB_ROW2_PIN, KB_ROW3_PIN};
static GPIO_TypeDef *readPorts[4] = {KB_COL0_PORT, KB_COL1_PORT, KB_COL2_PORT, KB_COL3_PORT};
static const uint16_t readPins[4] = {KB_COL0_PIN, KB_COL1_PIN, KB_COL2_PIN, KB_COL3_PIN};

static uint8_t pressedKey = KEY_NONE;
static uint8_t lastRawKey = KEY_NONE;
static uint32_t lastRawTick = 0U;

static void Keyboard_SetRows(GPIO_PinState state)
{
    uint8_t i;

    for (i = 0U; i < 4U; i++)
        HAL_GPIO_WritePin(drivePorts[i], drivePins[i], state);
}

static void Keyboard_Settle(void)
{
    volatile uint16_t d = 1500U;
    while (d--);
}

static void Keyboard_ConfigPins(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pin = KB_ROW0_PIN | KB_ROW1_PIN | KB_ROW2_PIN | KB_ROW3_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    Keyboard_SetRows(GPIO_PIN_SET);

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Pin = KB_COL0_PIN | KB_COL1_PIN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = KB_COL2_PIN | KB_COL3_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static uint8_t Keyboard_ReadRaw(void)
{
    uint8_t row;
    uint8_t col;

    for (col = 0U; col < 4U; col++)
    {
        Keyboard_SetRows(GPIO_PIN_SET);
        HAL_GPIO_WritePin(drivePorts[col], drivePins[col], GPIO_PIN_RESET);
        Keyboard_Settle();

        for (row = 0U; row < 4U; row++)
        {
            if (HAL_GPIO_ReadPin(readPorts[row], readPins[row]) == GPIO_PIN_RESET)
            {
                Keyboard_SetRows(GPIO_PIN_SET);
                return keyMap[row][col];
            }
        }
    }

    Keyboard_SetRows(GPIO_PIN_SET);
    return KEY_NONE;
}

void Keyboard_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    Keyboard_ConfigPins();
    pressedKey = KEY_NONE;
    lastRawKey = KEY_NONE;
    lastRawTick = HAL_GetTick();
}

uint8_t Keyboard_Scan(void)
{
    uint8_t raw = Keyboard_ReadRaw();
    uint32_t now = HAL_GetTick();

    if (raw != lastRawKey)
    {
        lastRawKey = raw;
        lastRawTick = now;
        return KEY_NONE;
    }

    if ((uint32_t)(now - lastRawTick) < KB_DEBOUNCE_MS)
        return KEY_NONE;

    if (raw == KEY_NONE)
    {
        pressedKey = KEY_NONE;
        return KEY_NONE;
    }

    if (pressedKey == KEY_NONE)
    {
        pressedKey = raw;
        return raw;
    }

    return KEY_NONE;
}

uint8_t Keyboard_HeldKey(void)
{
    return pressedKey;
}

