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

> 下表由 `python3 tools/skill_registry.py --write` 从 skill.json 生成，请勿手工编辑。

<!-- GENERATED:ROUTE_TABLE:START -->
| 意图 | 关键词或型号 | 读取文件 |
|------|------------|----------|
| 温度传感器 | 温度传感器、DS18B20、SHT30、NTC、PT100、LM35 | `references/temperature.md` |
| 湿度传感器 | 湿度传感器、DHT11、DHT22、BME280 | `references/humidity.md` |
| 压力与称重传感器 | 压力传感器、BMP280、HX711、称重传感器 | `references/pressure.md` |
| 光照与图像传感器 | 光照传感器、BH1750、TSL2561、OV2640、OV5640 | `references/light.md` |
| IMU 与姿态传感器 | IMU、MPU6050、BMI270、ICM42688、电子罗盘 | `references/imu.md` |
| 气体与空气质量传感器 | 气体传感器、MQ135、CCS811、SGP30、空气质量 | `references/gas.md` |
| 超声波、ToF 与红外测距 | 测距传感器、HC-SR04、VL53L0X、TFmini | `references/distance.md` |
| 霍尔与磁场传感器 | 霍尔传感器、HMC5883L、QMC5883L、MLX90393 | `references/magnetic.md` |
| GPS 与 GNSS | GPS 模块、GNSS、NEO-6M、ATGM336H、NMEA | `references/gps.md` |
| ADC 水位与 MQ135 示例 | 水位传感器示例、MQ135 示例 | `examples/adc-water/` |
| GP2Y1014AU 粉尘传感器示例 | GP2Y1014AU、粉尘传感器示例、PM2.5 示例 | `examples/gp2y1014au/` |
| BH1750 示例 | BH1750 示例 | `examples/bh1750-light/` |
| CCS811 与 SGP30 示例 | CCS811 示例、SGP30 示例 | `examples/ccs811-sgp30/` |
| DHT11 与 DHT22 时序示例 | DHT11 示例、DHT22 示例 | `examples/dht11-dht22/` |
| DS18B20 示例 | DS18B20 示例 | `examples/ds18b20/` |
| GPS 解析示例 | GPS 解析示例、NMEA 解析示例 | `examples/gps/` |
| HC-SR04 示例 | HC-SR04 示例 | `examples/hc-sr04/` |
| HX711 示例 | HX711 示例 | `examples/hx711/` |
| MAX30102 示例 | MAX30102、心率血氧 | `examples/max30102/` |
| MPU6050 示例 | MPU6050 示例 | `examples/mpu6050/` |
<!-- GENERATED:ROUTE_TABLE:END -->

## 输出要求

- 区分传感器原始数据、补偿后数据和业务单位。
- 给出上电稳定时间、采样间隔、CRC/校验、超量程与断线处理。
- 涉及多传感器融合或完整系统时交由 `mcu-system-design` 在编排层组合。
