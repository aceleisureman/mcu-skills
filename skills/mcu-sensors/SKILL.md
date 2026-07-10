---
name: mcu-sensors
description: MCU 传感器选型与驱动 Skill，覆盖温度、湿度、压力、光照与图像、IMU、气体、距离、磁性、GPS/GNSS 的硬件设计、通信协议、驱动实现、标定、调试和故障排查。当用户提到 DS18B20、SHT30、BME280、MPU6050、BH1750、MQ 系列、HC-SR04、VL53L0X、GPS 等传感器时使用。
---

# MCU 传感器

## 使用流程

1. 根据测量对象或具体型号从路由表读取一篇 `references/` 文档。
2. 先完成量程、精度、接口、供电和环境条件选型，再设计电路和驱动。
3. 需要总线骨架、硬件审查或测试方法时联合读取 `skill://mcu-driver-core/SKILL.md`。
4. 输出中说明采样周期、滤波、标定、异常值处理和功耗策略。

## 意图路由

| 需求或型号 | 读取文件 |
|------------|----------|
| DS18B20、SHT30、NTC、PT100、LM35 | `references/temperature.md` |
| DHT11、DHT22、SHT 系列、BME280 | `references/humidity.md` |
| BMP280、HX711、MPX 系列 | `references/pressure.md` |
| BH1750、TSL2561、LDR、OV2640/OV5640 | `references/light.md` |
| MPU6050、BMI270、ICM、电子罗盘 | `references/imu.md` |
| MQ 系列、CCS811、SGP30 | `references/gas.md` |
| HC-SR04、VL53L0X、TFmini、红外测距 | `references/distance.md` |
| 霍尔、HMC5883L、QMC5883L、MLX90393 | `references/magnetic.md` |
| NEO-6M、ATGM336H、NMEA、RTK | `references/gps.md` |

## 示例

| 场景 | 路径 |
|------|------|
| ADC 水位与 MQ135 | `examples/adc-water/` |
| BH1750 | `examples/bh1750-light/` |
| CCS811/SGP30 | `examples/ccs811-sgp30/` |
| DHT11/DHT22 多种时序实现 | `examples/dht11-dht22/` |
| DS18B20 | `examples/ds18b20/` |
| GPS 解析 | `examples/gps/` |
| HC-SR04 | `examples/hc-sr04/` |
| HX711 | `examples/hx711/` |
| MAX30102 | `examples/max30102/` |
| MPU6050 | `examples/mpu6050/` |

## 输出要求

- 区分传感器原始数据、补偿后数据和业务单位。
- 给出上电稳定时间、采样间隔、CRC/校验、超量程与断线处理。
- 涉及多传感器融合或完整系统时交由 `mcu-system-design` 在编排层组合。
