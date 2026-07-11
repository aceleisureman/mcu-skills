#include "ina219.h"

/* 平台适配: 由工程提供 I2C 16 位寄存器读写 (寄存器值大端), 返回 0=成功 */
extern int i2c_write_reg16(uint8_t addr, uint8_t reg, uint16_t value);
extern int i2c_read_reg16(uint8_t addr, uint8_t reg, uint16_t *value);

/* CONFIG: 32V 量程/PGA /8, 12bit 16 次平均, 连续采样 shunt+bus */
#define INA219_CONFIG_DEFAULT  0x3F1Fu

int INA219_Init(INA219 *dev, uint8_t addr, uint32_t max_current_ma, uint32_t shunt_mohm)
{
    dev->addr = addr;

    /* current_lsb = 最大电流 / 2^15, 向上取整到 1uA 粒度 */
    uint32_t lsb_ua = (max_current_ma * 1000u + 32767u) / 32768u;
    if (lsb_ua == 0u) {
        lsb_ua = 1u;
    }
    dev->current_lsb = (uint16_t)lsb_ua;

    /* cal = 0.04096 / (current_lsb[A] * Rshunt[Ohm])
     *     = 40960000 / (lsb_ua * shunt_mohm) */
    uint32_t cal = 40960000u / (lsb_ua * shunt_mohm);
    if (cal > 0xFFFEu) {
        cal = 0xFFFEu;
    }

    if (i2c_write_reg16(dev->addr, INA219_REG_CALIB, (uint16_t)cal) != 0) {
        return -1;
    }
    return i2c_write_reg16(dev->addr, INA219_REG_CONFIG, INA219_CONFIG_DEFAULT);
}

int INA219_ReadBusVoltage(INA219 *dev, uint32_t *mv)
{
    uint16_t raw;
    if (i2c_read_reg16(dev->addr, INA219_REG_BUS_V, &raw) != 0) {
        return -1;
    }
    /* bit0 OVF 溢出标志, bit1 CNVR; 电压位从 bit3 开始, LSB = 4mV */
    if (raw & 0x0001u) {
        return -2; /* 校准溢出, 需增大 current_lsb */
    }
    *mv = (uint32_t)(raw >> 3) * 4u;
    return 0;
}

int INA219_ReadCurrent(INA219 *dev, int32_t *ua)
{
    uint16_t raw;
    if (i2c_read_reg16(dev->addr, INA219_REG_CURRENT, &raw) != 0) {
        return -1;
    }
    *ua = (int32_t)(int16_t)raw * (int32_t)dev->current_lsb;
    return 0;
}

int INA219_ReadPower(INA219 *dev, uint32_t *mw)
{
    uint16_t raw;
    if (i2c_read_reg16(dev->addr, INA219_REG_POWER, &raw) != 0) {
        return -1;
    }
    /* power_lsb = 20 * current_lsb, 结果换算为 mW */
    *mw = ((uint32_t)raw * 20u * dev->current_lsb) / 1000u;
    return 0;
}
