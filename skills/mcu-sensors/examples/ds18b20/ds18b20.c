/**
 * DS18B20 温度传感器驱动 — STM32 HAL 库
 *
 * 改进点（参考探头资料）:
 *   1. 时序参数按 1-Wire 标准严格配置，高低温环境稳定
 *   2. 增加 CRC-8/MAXIM 校验，防止通信错误
 *   3. 正确处理负温度转换
 *   4. GPIO 模式切换使用浮空输入避免毛刺（STM32 由输出切上拉输入会产生 1~2μs 毛刺）
 *
 * 接线:
 *   DS18B20 VDD  → 3.3V/5V
 *   DS18B20 DQ   → PA8 (需 4.7kΩ 上拉电阻至 VCC)
 *   DS18B20 GND  → GND
 */

#include "ds18b20.h"

/* ====== 微秒延时（基于 DWT 周期计数器，精度高） ====== */
static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < ticks);
}

/* ====== DWT 初始化 ====== */
void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* ====== GPIO 模式切换 ====== */
/* 注意: 使用浮空输入(INPUT) 而非上拉输入(PULL_UP)
 * STM32 由输出模式切换为上拉输入时会产生 1~2μs 毛刺，导致 CRC 出错 */
static void ds18b20_pin_out(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = DS18B20_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;   /* 开漏输出 */
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DS18B20_PORT, &GPIO_InitStruct);
}

static void ds18b20_pin_in(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = DS18B20_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;       /* 浮空输入，避免毛刺 */
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(DS18B20_PORT, &GPIO_InitStruct);
}

/* ====== 初始化 ====== */
void DS18B20_Init(void)
{
    DWT_Init();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    ds18b20_pin_out();
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
}

/* ====== 复位脉冲 ======
 * MCU 拉低 ≥ 480μs，释放后等待 65μs 检测存在脉冲
 * 存在脉冲窗口: 60~240μs
 * 返回: 1 = 器件存在, 0 = 无器件 */
uint8_t DS18B20_Reset(void)
{
    uint8_t presence;

    __disable_irq();            /* 关中断保证时序 */
    ds18b20_pin_out();
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
    delay_us(500);              /* 复位脉冲 ≥ 480μs */
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
    ds18b20_pin_in();
    delay_us(65);               /* 等待 65μs 后采样存在脉冲 */
    presence = (HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN) == GPIO_PIN_RESET) ? 1 : 0;
    __enable_irq();
    delay_us(420);              /* 等待存在脉冲结束 */
    return presence;
}

/* ====== 写一位 ======
 * 下拉 > 1μs 后输出数据，维持 ≥ 60μs
 * 写 1: 下拉 1~2μs 后释放
 * 写 0: 保持低 ≥ 60μs 后释放 */
static void ds18b20_write_bit(uint8_t bit)
{
    __disable_irq();
    ds18b20_pin_out();
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
    delay_us(2);                /* 下拉 > 1μs */
    if (bit)
        HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);  /* 写 1: 释放 */
    delay_us(60);               /* 维持 ≥ 60μs 让芯片采样 */
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);      /* 释放总线 */
    delay_us(2);                /* 恢复时间 > 1μs */
    __enable_irq();
}

/* ====== 读一位 ======
 * 下拉 > 1μs 后释放，在 < 15μs 内采样
 * 总周期 ≥ 60μs */
static uint8_t ds18b20_read_bit(void)
{
    uint8_t bit;

    __disable_irq();
    ds18b20_pin_out();
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
    delay_us(2);                /* 下拉 > 1μs */
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);  /* 释放总线 */
    delay_us(8);                /* 等待后在 < 15μs 内采样 */
    ds18b20_pin_in();
    bit = HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN);
    delay_us(50);               /* 确保总周期 ≥ 60μs */
    __enable_irq();
    return bit;
}

/* ====== 写一个字节 (LSB first) ====== */
void DS18B20_WriteByte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        ds18b20_write_bit(data & 0x01);
        data >>= 1;
    }
}

/* ====== 读一个字节 (LSB first) ====== */
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

/* ====== CRC-8/MAXIM 校验 ======
 * 多项式: 0x31, 初始值: 0x00, 输入/输出反射: 是 */
uint8_t DS18B20_CRC8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00;
    while (len--)
    {
        uint8_t inbyte = *data++;
        for (uint8_t i = 0; i < 8; i++)
        {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;   /* 0x8C = 反射后的 0x31 */
            inbyte >>= 1;
        }
    }
    return crc;
}

/* ====== 启动温度转换 ====== */
void DS18B20_StartConvert(void)
{
    if (DS18B20_Reset())
    {
        DS18B20_WriteByte(0xCC);  /* SKIP ROM */
        DS18B20_WriteByte(0x44);  /* CONVERT T */
    }
}

/* ====== 读取温度（带 CRC 校验） ======
 * 返回: 温度值 (°C)，错误时返回 DS18B20_ERR_TEMP (-999.0f) */
float DS18B20_ReadTemp(void)
{
    uint8_t buf[9];   /* Scratchpad: 9 bytes (8 data + 1 CRC) */
    int16_t raw;
    float temp;

    if (!DS18B20_Reset())
        return DS18B20_ERR_TEMP;

    DS18B20_WriteByte(0xCC);  /* SKIP ROM */
    DS18B20_WriteByte(0xBE);  /* READ SCRATCHPAD */

    /* 读取 9 字节: LSB, MSB, TH, TL, Config, Reserved×3, CRC */
    for (uint8_t i = 0; i < 9; i++)
        buf[i] = DS18B20_ReadByte();

    /* CRC 校验 */
    if (DS18B20_CRC8(buf, 8) != buf[8])
        return DS18B20_ERR_TEMP;

    /* 温度转换: 12bit 模式, 0.0625°C/LSB */
    raw = (int16_t)((buf[1] << 8) | buf[0]);
    temp = raw * 0.0625f;

    /* 过滤上电默认值 85°C (0x5500) */
    if (raw == 0x5500)
        return DS18B20_ERR_TEMP;

    return temp;
}
