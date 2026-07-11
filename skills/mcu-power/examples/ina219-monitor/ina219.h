#ifndef __INA219_H
#define __INA219_H

#include <stdint.h>

/* INA219 I2C 电流/电压/功率监控示例驱动 (地址 A1A0 接地时 0x40).
 * 硬件: VIN+/VIN- 串入采样电阻 (示例 0.1R)，总线电压范围 0~26V.
 * 依赖: 平台提供 i2c_write_reg16 / i2c_read_reg16 (16 位大端寄存器). */

#define INA219_ADDR            0x40u

#define INA219_REG_CONFIG      0x00u
#define INA219_REG_SHUNT_V     0x01u
#define INA219_REG_BUS_V       0x02u
#define INA219_REG_POWER       0x03u
#define INA219_REG_CURRENT     0x04u
#define INA219_REG_CALIB       0x05u

typedef struct {
    uint8_t addr;          /* 7 位 I2C 地址 */
    uint16_t current_lsb;  /* uA/LSB, 由量程计算 */
} INA219;

/* 初始化并按最大预期电流(mA)与采样电阻(mR)写入校准值, 返回 0=成功 */
int INA219_Init(INA219 *dev, uint8_t addr, uint32_t max_current_ma, uint32_t shunt_mohm);

/* 总线电压, 单位 mV */
int INA219_ReadBusVoltage(INA219 *dev, uint32_t *mv);

/* 负载电流, 单位 uA (符号表示方向) */
int INA219_ReadCurrent(INA219 *dev, int32_t *ua);

/* 功率, 单位 mW */
int INA219_ReadPower(INA219 *dev, uint32_t *mw);

#endif
