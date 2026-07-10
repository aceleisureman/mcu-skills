# STM32 项目实践快速参考

> 本文档从 12 个实际 STM32 项目中提炼关键模式。各元器件的具体实际代码已整合到对应 `references/` 文档的第 6 章「实际项目代码参考」中，完整源码见 `examples/` 目录。

## 项目代码组织

### 两种目录结构

| 风格 | 代表项目 | 驱动文件位置 |
|------|----------|-------------|
| Core/Src 扁平式 | 老年人心率监控、教室环境调控器、窑池环境调控、家用智能药箱 | `Core/Src/xxx.c` + `Core/Inc/xxx.h` |
| Hardware 分目录式 | 门禁系统密码锁、智能宠物喂食 | `Hardware/OLED/oled.c`, `Hardware/Keyboard/keyboard.c` |

### BSP 统一初始化（推荐）

```c
void BSP_Init(void) {
    DWT_Init();        // DWT 周期计数器（微秒延时基础，必须最先初始化）
    MX_GPIO_Init();    // 所有 GPIO 一次性初始化
    MX_I2C1_Init();    // 硬件 I2C
    MX_TIM3_Init();    // PWM 定时器
    MX_ADC1_Init();    // ADC
    MX_USART2_Init();  // UART
}
```

## 微秒延时方案对比

| 方案 | 代码 | 精度 | 占用资源 | 推荐场景 |
|------|------|------|----------|----------|
| DWT CYCCNT（首选） | `DWT->CYCCNT` 读取周期计数 | 最高 | 无 | 所有需要精确微秒延时的场景 |
| SysTick VAL 倒计数 | 读 `SysTick->VAL` 差值 | 高 | 占用 SysTick | 无法用 DWT 的场景 |
| 循环 NOP | `while(count--) __NOP()` | 低（依赖时钟频率） | 无 | 简单低速场景 |

```c
/* DWT 微秒延时 — 实际项目最常用 */
void BSP_DelayUs(uint32_t us) {
    uint32_t tick_per_us = HAL_RCC_GetHCLKFreq() / 1000000U;
    uint32_t start = DWT->CYCCNT;
    uint32_t wait = us * tick_per_us;
    while ((DWT->CYCCNT - start) < wait) { }
}
```

## 软件 I2C 三种性能档次

| 档次 | GPIO 操作 | 延时方案 | 适用场景 | 示例代码 |
|------|-----------|----------|----------|----------|
| 最快 | `port->BSRR = pin` | 循环 NOP | OLED 高刷屏 | `examples/oled/oled_v1_dirty_page.c` |
| 中等 | `HAL_GPIO_WritePin` | DWT CYCCNT | 通用 I2C 器件 | `examples/soft-i2c/soft_i2c.c` |
| 最慢 | `HAL_GPIO_WritePin` | 循环递减 | 低速 OLED | `examples/oled/oled_v2_burst_write.c` |

## 关键实践模式速查

| 模式 | 说明 | 实际代码位置 |
|------|------|-------------|
| 上电安全 | GPIO 配置输出前先设关断电平 | `examples/buzzer-pir-relay/door_io.c` |
| 有效电平抽象 | 统一函数处理高/低有效器件 | `examples/buzzer-pir-relay/door_io.c` + `relays.c` |
| 环形缓冲+中断接收 | volatile head/tail + 取模索引 | `examples/esp8266-wifi/esp8266_at.c` |
| UART 错误恢复 | ORE 清除后重新启用接收 | `examples/esp8266-wifi/esp8266_at.c` |
| AT 指令 wait_token | 流式匹配, 逐字节比较 | `examples/esp8266-wifi/esp01s_mqtt.c` |
| 自适应波特率 | 遍历常见波特率发送 AT 确认 | `examples/esp8266-wifi/esp01s_mqtt.c` |
| MQTT 手工组帧 | 不依赖 MQTT 库, 手工构建报文 | `examples/esp8266-wifi/esp01s_mqtt.c` |
| OLED 脏页刷新 | 只刷脏页的脏列区间 | `examples/oled/oled_v1_dirty_page.c` |
| OLED 批量写入 | 一次 START 连续写 N 字节 | `examples/oled/oled_v2_burst_write.c` |
| wait_level 超时 | 单总线传感器通用超时模式 | `examples/dht11-dht22/dht11_v1_wait_level.c` |
| DWT WaitPin | DWT CYCCNT 精确超时检测 | `examples/dht11-dht22/dht11_v2_dwt_waitpin.c` |
| 矩阵键盘行列扫描 | 列拉低+行读取+消抖 | `examples/keyboard/keyboard_matrix_4x4.c` |
| 连续计数消抖 | 连续 N 次读到高电平才确认 | `examples/buzzer-pir-relay/pir.c` |
| Handle+HAL_I2C_Mem | 硬件 I2C 寄存器读写封装 | `examples/max30102/max30102.c` |
| 地址自动探测 | 尝试多个 I2C 地址读 ID | `examples/max30102/max30102.c` |
| HX711 24bit 读取 | GPIO 位操作+符号扩展+去皮 | `examples/hx711/hx711.c` |
| Flash 末页存储 | 内部 Flash 参数存储+magic+CRC | `examples/flash-param/flash_param.c` |
| 多路继电器独立电平 | 每路继电器可配置不同有效电平 | `examples/buzzer-pir-relay/relays.c` |
| 双通道 PWM 舵机 | TIM3 双通道独立角度控制 | `examples/servo-pwm/servo.c` |

## 各项目使用的元器件一览

| 项目 | 元器件 |
|------|--------|
| 老年人心率+视频监控 | OLED(脏页刷新) / DS18B20 / MAX30102(心率血氧) |
| 家用智能药箱 | OLED / DHT11 / HX711(称重) / ESP-01S(MQTT) / MQ135 / 舵机 / RTC / 矩阵键盘 / 门磁 / JQ8900语音 |
| 门禁系统密码锁 | OLED(批量写入) / 4x4矩阵键盘 / 舵机 / 继电器 / 蜂鸣器 / LED / Flash密码存储 |
| 人体红外检测报警 | OLED / PIR人体感应 / 蜂鸣器 / 3按键 / LED |
| 教室环境调控器 | OLED / DHT11 / BH1750(光照) / MQ135 / DS1302(RTC) / Flash参数 / 蜂鸣器 / 继电器 / PIR / 按键 |
| 窑池环境调控 | OLED / DHT22 / CCS811/SGP30(空气质量) / ESP8266 / 软件 I2C / 继电器(4路) / 按键 |
| 智能宠物喂食 | OLED / DHT11 / ESP8266 / 舵机(双通道) / 水位传感器(ADC) / 按键 / Flash配置 |
