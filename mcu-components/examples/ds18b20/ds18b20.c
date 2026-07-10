#include "ds18b20.h"

// Microsecond delay for 8MHz HSI (no PLL)
// ~10 cycles per iteration, 10/8MHz = 1.25us per count
static void delay_us(uint32_t us)
{
    uint32_t count = us * 8;
    while (count--)
    {
        __NOP();
    }
}

static void ds18b20_pin_out(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = DS18B20_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DS18B20_PORT, &GPIO_InitStruct);
}

static void ds18b20_pin_in(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = DS18B20_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(DS18B20_PORT, &GPIO_InitStruct);
}

void DS18B20_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    ds18b20_pin_out();
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
}

// Reset pulse, returns 1 if device present
uint8_t DS18B20_Reset(void)
{
    uint8_t presence;
    ds18b20_pin_out();
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
    delay_us(480);
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
    delay_us(60);
    presence = (HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN) == GPIO_PIN_RESET) ? 1 : 0;
    delay_us(420);
    return presence;
}

// Write one bit
static void ds18b20_write_bit(uint8_t bit)
{
    ds18b20_pin_out();
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
    delay_us(2);
    if (bit)
        HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
    delay_us(60);
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
    delay_us(2);
}

// Read one bit
static uint8_t ds18b20_read_bit(void)
{
    uint8_t bit;
    ds18b20_pin_out();
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
    delay_us(2);
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
    delay_us(8);
    ds18b20_pin_in();
    bit = HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN);
    delay_us(50);
    return bit;
}

void DS18B20_WriteByte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        ds18b20_write_bit(data & 0x01);
        data >>= 1;
    }
}

uint8_t DS18B20_ReadByte(void)
{
    uint8_t data = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        data >>= 1;
        if (ds18b20_read_bit())
            data |= 0x80;
    }
    return data;
}

void DS18B20_StartConvert(void)
{
    if (DS18B20_Reset())
    {
        DS18B20_WriteByte(0xCC); // Skip ROM
        DS18B20_WriteByte(0x44); // Convert T
    }
}

float DS18B20_ReadTemp(void)
{
    uint8_t tl, th;
    int16_t raw;

    if (!DS18B20_Reset())
        return -999.0f;

    DS18B20_WriteByte(0xCC); // Skip ROM
    DS18B20_WriteByte(0xBE); // Read Scratchpad

    tl = DS18B20_ReadByte();
    th = DS18B20_ReadByte();

    raw = (th << 8) | tl;

    return (float)raw / 16.0f;
}
