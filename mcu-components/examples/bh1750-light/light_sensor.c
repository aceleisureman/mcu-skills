#include "light_sensor.h"

#define BH1750_ADDR     0x46
#define BH1750_POWER_ON  0x01
#define BH1750_RESET     0x07
#define BH1750_CONT_HRES 0x10

static void SCL_H(void) { HAL_GPIO_WritePin(BH1750_SCL_PORT, BH1750_SCL_PIN, GPIO_PIN_SET); }
static void SCL_L(void) { HAL_GPIO_WritePin(BH1750_SCL_PORT, BH1750_SCL_PIN, GPIO_PIN_RESET); }
static void SDA_H(void) { HAL_GPIO_WritePin(BH1750_SDA_PORT, BH1750_SDA_PIN, GPIO_PIN_SET); }
static void SDA_L(void) { HAL_GPIO_WritePin(BH1750_SDA_PORT, BH1750_SDA_PIN, GPIO_PIN_RESET); }

static void SDA_Out(void) {
    GPIO_InitTypeDef g = {0};
    g.Pin = BH1750_SDA_PIN;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BH1750_SDA_PORT, &g);
}

static void SDA_In(void) {
    GPIO_InitTypeDef g = {0};
    g.Pin = BH1750_SDA_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BH1750_SDA_PORT, &g);
}

static void IIC_Dly(void) { volatile uint8_t i = 5; while (i--); }

static void IIC_Start(void) {
    SDA_Out(); SDA_H(); SCL_H(); IIC_Dly();
    SDA_L(); IIC_Dly(); SCL_L();
}

static void IIC_Stop(void) {
    SDA_Out(); SDA_L(); SCL_H(); IIC_Dly();
    SDA_H(); IIC_Dly();
}

static uint8_t IIC_WaitAck(void) {
    uint8_t ack;
    SDA_In(); IIC_Dly();
    SCL_H(); IIC_Dly();
    ack = HAL_GPIO_ReadPin(BH1750_SDA_PORT, BH1750_SDA_PIN) == GPIO_PIN_RESET ? 1 : 0;
    SCL_L(); IIC_Dly();
    return ack;
}

static void IIC_WriteByte(uint8_t data) {
    SDA_Out();
    for (uint8_t i = 0; i < 8; i++) {
        if (data & 0x80) SDA_H(); else SDA_L();
        data <<= 1;
        IIC_Dly(); SCL_H(); IIC_Dly(); SCL_L();
    }
}

static uint8_t IIC_ReadByte(uint8_t ack) {
    uint8_t data = 0;
    SDA_In();
    for (uint8_t i = 0; i < 8; i++) {
        data <<= 1;
        SCL_H(); IIC_Dly();
        if (HAL_GPIO_ReadPin(BH1750_SDA_PORT, BH1750_SDA_PIN)) data |= 1;
        SCL_L(); IIC_Dly();
    }
    SDA_Out();
    if (ack) SDA_L(); else SDA_H();
    IIC_Dly(); SCL_H(); IIC_Dly(); SCL_L();
    return data;
}

static void BH1750_WriteCmd(uint8_t cmd) {
    IIC_Start();
    IIC_WriteByte(BH1750_ADDR);
    IIC_WaitAck();
    IIC_WriteByte(cmd);
    IIC_WaitAck();
    IIC_Stop();
}

void LightSensor_Init(void) {
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();

    g.Pin = BH1750_SCL_PIN;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BH1750_SCL_PORT, &g);

    g.Pin = BH1750_SDA_PIN;
    HAL_GPIO_Init(BH1750_SDA_PORT, &g);

    SCL_H(); SDA_H();
    HAL_Delay(10);

    BH1750_WriteCmd(BH1750_POWER_ON);
    BH1750_WriteCmd(BH1750_CONT_HRES);
    HAL_Delay(180);
}

uint16_t LightSensor_Read(void) {
    uint16_t lux;

    IIC_Start();
    IIC_WriteByte(BH1750_ADDR | 0x01);
    IIC_WaitAck();
    HAL_Delay(10);
    lux = IIC_ReadByte(1);
    lux <<= 8;
    lux |= IIC_ReadByte(0);
    IIC_Stop();

    return lux / 1.2;
}
