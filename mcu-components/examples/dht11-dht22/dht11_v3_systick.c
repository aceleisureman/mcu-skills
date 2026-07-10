#include "dht11.h"

static void DHT11_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = DHT11_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

static void DHT11_Delay_us(uint32_t us)
{
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    uint32_t told  = SysTick->VAL;
    uint32_t tnow, tcnt = 0;

    while (tcnt < ticks) {
        tnow = SysTick->VAL;
        if (tnow != told) {
            if (tnow < told)
                tcnt += told - tnow;
            else
                tcnt += SysTick->LOAD - tnow + told;
            told = tnow;
        }
    }
}

static uint8_t DHT11_ReadByte(void)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        uint32_t timeout = 100;
        while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET && timeout--) {
            DHT11_Delay_us(1);
        }
        DHT11_Delay_us(40);
        byte <<= 1;
        if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
            byte |= 1;
        timeout = 100;
        while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET && timeout--) {
            DHT11_Delay_us(1);
        }
    }
    return byte;
}

uint8_t DHT11_Init(void)
{
    DHT11_SetOutput();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    DHT11_Delay_us(30);
    DHT11_SetInput();

    uint32_t timeout = 100;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET && timeout--) {
        DHT11_Delay_us(1);
    }
    if (timeout == 0) return 0;

    timeout = 100;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET && timeout--) {
        DHT11_Delay_us(1);
    }
    if (timeout == 0) return 0;

    timeout = 100;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET && timeout--) {
        DHT11_Delay_us(1);
    }
    if (timeout == 0) return 0;

    return 1;
}

uint8_t DHT11_Read(DHT11_Data *data)
{
    uint8_t buf[5];

    DHT11_SetOutput();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    DHT11_Delay_us(30);
    DHT11_SetInput();

    uint32_t timeout = 100;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET && timeout--) {
        DHT11_Delay_us(1);
    }
    if (timeout == 0) return 0;

    timeout = 100;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET && timeout--) {
        DHT11_Delay_us(1);
    }
    if (timeout == 0) return 0;

    timeout = 100;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET && timeout--) {
        DHT11_Delay_us(1);
    }
    if (timeout == 0) return 0;

    for (int i = 0; i < 5; i++) {
        buf[i] = DHT11_ReadByte();
    }

    if ((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4])
        return 0;

    data->humidity    = buf[0];
    data->temperature = buf[2];

    return 1;
}
