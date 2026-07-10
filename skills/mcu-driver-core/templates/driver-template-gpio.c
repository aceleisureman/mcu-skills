/**
 * @file    driver-template-gpio.c
 * @brief   GPIO 驱动模板 (跨平台)
 *
 * 本模板提供 GPIO 驱动的标准化框架，包含：
 *   - GPIO HAL 抽象层定义 (平台无关接口)
 *   - 输入 / 输出配置
 *   - 外部中断回调
 *   - 消抖处理
 *   - 软件 SPI / I2C 的 GPIO 位操作
 *
 * ## 使用方法
 *   1. 实现 gpio_hal_t 中的平台适配函数
 *   2. 用 gpio_config() 配置引脚模式
 *   3. 输入读取: gpio_read()
 *   4. 输出设置: gpio_write() / gpio_toggle()
 *   5. 中断: gpio_register_callback() + ISR 中调用 gpio_irq_handler()
 *   6. 软件 I2C/SPI: 使用 sw_i2c_* / sw_spi_* 系列函数
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================
 * 1. GPIO HAL 抽象层定义
 * ============================================================ */

typedef enum {
    GPIO_OK = 0,
    GPIO_ERR_PARAM,
    GPIO_ERR_UNSUPPORTED,
} gpio_status_t;

typedef enum {
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_INPUT_PULLUP,
    GPIO_MODE_INPUT_PULLDOWN,
    GPIO_MODE_OUTPUT_PUSH_PULL,
    GPIO_MODE_OUTPUT_OPEN_DRAIN,
    GPIO_MODE_ANALOG,
} gpio_mode_t;

typedef enum {
    GPIO_IRQ_NONE = 0,
    GPIO_IRQ_RISING,
    GPIO_IRQ_FALLING,
    GPIO_IRQ_BOTH,
    GPIO_IRQ_LOW_LEVEL,
    GPIO_IRQ_HIGH_LEVEL,
} gpio_irq_type_t;

typedef struct {
    gpio_status_t (*config)(void *ud, uint8_t port, uint8_t pin, gpio_mode_t mode);
    bool (*read)(void *ud, uint8_t port, uint8_t pin);
    void (*write)(void *ud, uint8_t port, uint8_t pin, bool high);
    void (*toggle)(void *ud, uint8_t port, uint8_t pin);
    gpio_status_t (*config_irq)(void *ud, uint8_t port, uint8_t pin, gpio_irq_type_t t);
    void (*irq_enable)(void *ud, uint8_t port, uint8_t pin, bool enable);
    void (*delay_us)(uint32_t us);
    void (*delay_ms)(uint32_t ms);
    uint32_t (*get_tick_ms)(void);
    void (*enter_critical)(void);
    void (*exit_critical)(void);
    void *user_data;
} gpio_hal_t;

typedef struct {
    gpio_hal_t *hal;
    bool        initialized;
} gpio_device_t;

/* ============================================================
 * 2. 初始化与基础操作
 * ============================================================ */

gpio_status_t gpio_init(gpio_device_t *dev, gpio_hal_t *hal)
{
    if (!dev || !hal)
        return GPIO_ERR_PARAM;
    dev->hal = hal;
    dev->initialized = true;
    return GPIO_OK;
}

gpio_status_t gpio_config(gpio_device_t *dev, uint8_t port, uint8_t pin,
                           gpio_mode_t mode)
{
    if (!dev || !dev->hal)
        return GPIO_ERR_PARAM;
    return dev->hal->config(dev->hal->user_data, port, pin, mode);
}

bool gpio_read(gpio_device_t *dev, uint8_t port, uint8_t pin)
{
    if (!dev || !dev->hal)
        return false;
    return dev->hal->read(dev->hal->user_data, port, pin);
}

void gpio_write(gpio_device_t *dev, uint8_t port, uint8_t pin, bool high)
{
    if (!dev || !dev->hal)
        return;
    dev->hal->write(dev->hal->user_data, port, pin, high);
}

void gpio_toggle(gpio_device_t *dev, uint8_t port, uint8_t pin)
{
    if (!dev || !dev->hal)
        return;
    dev->hal->toggle(dev->hal->user_data, port, pin);
}

/* ============================================================
 * 3. 外部中断回调
 * ============================================================ */

#define GPIO_MAX_CALLBACKS  16

typedef void (*gpio_callback_t)(uint8_t port, uint8_t pin, void *arg);

typedef struct {
    uint8_t          port;
    uint8_t          pin;
    gpio_callback_t  callback;
    void            *arg;
    bool             used;
} gpio_callback_entry_t;

static gpio_callback_entry_t s_callbacks[GPIO_MAX_CALLBACKS];

gpio_status_t gpio_register_callback(gpio_device_t *dev,
                                      uint8_t port, uint8_t pin,
                                      gpio_irq_type_t irq_type,
                                      gpio_callback_t callback, void *arg)
{
    if (!dev || !dev->hal || !callback)
        return GPIO_ERR_PARAM;

    int slot = -1;
    for (int i = 0; i < GPIO_MAX_CALLBACKS; i++) {
        if (s_callbacks[i].used && s_callbacks[i].port == port
            && s_callbacks[i].pin == pin) {
            slot = i;
            break;
        }
        if (!s_callbacks[i].used && slot < 0)
            slot = i;
    }
    if (slot < 0)
        return GPIO_ERR_UNSUPPORTED;

    s_callbacks[slot].port     = port;
    s_callbacks[slot].pin      = pin;
    s_callbacks[slot].callback = callback;
    s_callbacks[slot].arg      = arg;
    s_callbacks[slot].used     = true;

    return dev->hal->config_irq(dev->hal->user_data, port, pin, irq_type);
}

void gpio_unregister_callback(uint8_t port, uint8_t pin)
{
    for (int i = 0; i < GPIO_MAX_CALLBACKS; i++) {
        if (s_callbacks[i].used && s_callbacks[i].port == port
            && s_callbacks[i].pin == pin) {
            s_callbacks[i].used = false;
            return;
        }
    }
}

void gpio_irq_handler(uint8_t port, uint8_t pin)
{
    for (int i = 0; i < GPIO_MAX_CALLBACKS; i++) {
        if (s_callbacks[i].used && s_callbacks[i].port == port
            && s_callbacks[i].pin == pin) {
            s_callbacks[i].callback(port, pin, s_callbacks[i].arg);
            return;
        }
    }
}

/* ============================================================
 * 4. 消抖处理
 * ============================================================ */

#define DEBOUNCE_TIME_MS  20

typedef struct {
    uint8_t  port;
    uint8_t  pin;
    bool     last_raw;        /* 上次原始读数 */
    bool     stable;          /* 消抖后的稳定值 */
    uint32_t last_change_ms;  /* 上次电平变化时间 */
} gpio_debounce_t;

void debounce_init(gpio_debounce_t *db, uint8_t port, uint8_t pin)
{
    db->port = port;
    db->pin = pin;
    db->last_change_ms = 0;
    db->stable = false;
    db->last_raw = false;
}

/**
 * 消抖检测 (在主循环中周期调用, 建议 5~10ms 间隔)
 * @return true 如果稳定值发生翻转
 */
bool debounce_update(gpio_device_t *dev, gpio_debounce_t *db)
{
    if (!dev || !db)
        return false;

    bool raw = gpio_read(dev, db->port, db->pin);
    uint32_t now = dev->hal->get_tick_ms();

    if (raw != db->last_raw) {
        db->last_raw = raw;
        db->last_change_ms = now;
        return false;
    }

    if ((now - db->last_change_ms) >= DEBOUNCE_TIME_MS) {
        if (raw != db->stable) {
            db->stable = raw;
            return true;
        }
    }
    return false;
}

/**
 * 消抖读取 (阻塞, 适用于按键检测)
 */
bool gpio_read_debounced(gpio_device_t *dev, uint8_t port, uint8_t pin)
{
    if (!dev || !dev->hal)
        return false;

    bool first = gpio_read(dev, port, pin);
    dev->hal->delay_ms(DEBOUNCE_TIME_MS);
    bool second = gpio_read(dev, port, pin);

    return (first == second) ? first : false;
}

/* ============================================================
 * 5. 软件软件 I2C 的 GPIO 位操作
 * ============================================================ */

typedef struct {
    gpio_device_t *dev;
    uint8_t  scl_port, scl_pin;
    uint8_t  sda_port, sda_pin;
    uint32_t delay_us;   /* 半周期延时, 100kHz -> 5us, 400kHz -> 1.25us */
} sw_i2c_t;

static inline void sw_i2c_scl_high(sw_i2c_t *i2c)
{
    /* 开漏模式: 设为输入即释放 (靠上拉拉高) */
    i2c->dev->hal->config(i2c->dev->hal->user_data,
                           i2c->scl_port, i2c->scl_pin, GPIO_MODE_INPUT_PULLUP);
}

static inline void sw_i2c_scl_low(sw_i2c_t *i2c)
{
    i2c->dev->hal->config(i2c->dev->hal->user_data,
                           i2c->scl_port, i2c->scl_pin, GPIO_MODE_OUTPUT_OPEN_DRAIN);
    i2c->dev->hal->write(i2c->dev->hal->user_data,
                          i2c->scl_port, i2c->scl_pin, false);
}

static inline void sw_i2c_sda_high(sw_i2c_t *i2c)
{
    i2c->dev->hal->config(i2c->dev->hal->user_data,
                           i2c->sda_port, i2c->sda_pin, GPIO_MODE_INPUT_PULLUP);
}

static inline void sw_i2c_sda_low(sw_i2c_t *i2c)
{
    i2c->dev->hal->config(i2c->dev->hal->user_data,
                           i2c->sda_port, i2c->sda_pin, GPIO_MODE_OUTPUT_OPEN_DRAIN);
    i2c->dev->hal->write(i2c->dev->hal->user_data,
                          i2c->sda_port, i2c->sda_pin, false);
}

static inline bool sw_i2c_sda_read(sw_i2c_t *i2c)
{
    return i2c->dev->hal->read(i2c->dev->hal->user_data,
                                i2c->sda_port, i2c->sda_pin);
}

void sw_i2c_init(sw_i2c_t *i2c)
{
    i2c->dev->hal->enter_critical();
    sw_i2c_sda_high(i2c);
    sw_i2c_scl_high(i2c);
    i2c->dev->hal->exit_critical();
}

bool sw_i2c_start(sw_i2c_t *i2c)
{
    i2c->dev->hal->enter_critical();
    sw_i2c_sda_high(i2c);
    i2c->dev->hal->delay_us(i2c->delay_us);
    sw_i2c_scl_high(i2c);
    i2c->dev->hal->delay_us(i2c->delay_us);
    sw_i2c_sda_low(i2c);
    i2c->dev->hal->delay_us(i2c->delay_us);
    sw_i2c_scl_low(i2c);
    i2c->dev->hal->delay_us(i2c->delay_us);
    i2c->dev->hal->exit_critical();
    return true;
}

void sw_i2c_stop(sw_i2c_t *i2c)
{
    i2c->dev->hal->enter_critical();
    sw_i2c_sda_low(i2c);
    i2c->dev->hal->delay_us(i2c->delay_us);
    sw_i2c_scl_high(i2c);
    i2c->dev->hal->delay_us(i2c->delay_us);
    sw_i2c_sda_high(i2c);
    i2c->dev->hal->delay_us(i2c->delay_us);
    i2c->dev->hal->exit_critical();
}

bool sw_i2c_write_byte(sw_i2c_t *i2c, uint8_t byte)
{
    i2c->dev->hal->enter_critical();
    for (int i = 7; i >= 0; i--) {
        if (byte & (1 << i))
            sw_i2c_sda_high(i2c);
        else
            sw_i2c_sda_low(i2c);
        i2c->dev->hal->delay_us(i2c->delay_us);
        sw_i2c_scl_high(i2c);
        i2c->dev->hal->delay_us(i2c->delay_us);
        sw_i2c_scl_low(i2c);
    }
    /* 读取 ACK */
    sw_i2c_sda_high(i2c);
    i2c->dev->hal->delay_us(i2c->delay_us);
    sw_i2c_scl_high(i2c);
    i2c->dev->hal->delay_us(i2c->delay_us);
    bool ack = !sw_i2c_sda_read(i2c);
    sw_i2c_scl_low(i2c);
    i2c->dev->hal->delay_us(i2c->delay_us);
    i2c->dev->hal->exit_critical();
    return ack;
}

uint8_t sw_i2c_read_byte(sw_i2c_t *i2c, bool send_ack)
{
    uint8_t byte = 0;
    i2c->dev->hal->enter_critical();
    sw_i2c_sda_high(i2c);
    for (int i = 7; i >= 0; i--) {
        i2c->dev->hal->delay_us(i2c->delay_us);
        sw_i2c_scl_high(i2c);
        i2c->dev->hal->delay_us(i2c->delay_us);
        if (sw_i2c_sda_read(i2c))
            byte |= (1 << i);
        sw_i2c_scl_low(i2c);
    }
    /* 发送 ACK/NACK */
    if (send_ack)
        sw_i2c_sda_low(i2c);
    else
        sw_i2c_sda_high(i2c);
    i2c->dev->hal->delay_us(i2c->delay_us);
    sw_i2c_scl_high(i2c);
    i2c->dev->hal->delay_us(i2c->delay_us);
    sw_i2c_scl_low(i2c);
    sw_i2c_sda_high(i2c);
    i2c->dev->hal->delay_us(i2c->delay_us);
    i2c->dev->hal->exit_critical();
    return byte;
}

/* ============================================================
 * 6. 软件 SPI 的 GPIO 位操作
 * ============================================================ */

typedef struct {
    gpio_device_t *dev;
    uint8_t  sck_port, sck_pin;
    uint8_t  mosi_port, mosi_pin;
    uint8_t  miso_port, miso_pin;
    uint8_t  cs_port, cs_pin;
    uint32_t delay_us;   /* 半位延时, 1MHz -> 0.5us */
    bool     cpol;       /* 时钟极性 */
    bool     cpha;       /* 时钟相位 */
} sw_spi_t;

void sw_spi_init(sw_spi_t *spi)
{
    spi->dev->hal->config(spi->dev->hal->user_data,
                           spi->sck_port, spi->sck_pin, GPIO_MODE_OUTPUT_PUSH_PULL);
    spi->dev->hal->config(spi->dev->hal->user_data,
                           spi->mosi_port, spi->mosi_pin, GPIO_MODE_OUTPUT_PUSH_PULL);
    spi->dev->hal->config(spi->dev->hal->user_data,
                           spi->miso_port, spi->miso_pin, GPIO_MODE_INPUT_PULLUP);
    spi->dev->hal->config(spi->dev->hal->user_data,
                           spi->cs_port, spi->cs_pin, GPIO_MODE_OUTPUT_PUSH_PULL);
    /* CS 空闲高 */
    spi->dev->hal->write(spi->dev->hal->user_data,
                          spi->cs_port, spi->cs_pin, true);
    /* SCK 空闲电平 = CPOL */
    spi->dev->hal->write(spi->dev->hal->user_data,
                          spi->sck_port, spi->sck_pin, spi->cpol);
}

void sw_spi_cs_select(sw_spi_t *spi)
{
    spi->dev->hal->write(spi->dev->hal->user_data,
                          spi->cs_port, spi->cs_pin, false);
}

void sw_spi_cs_deselect(sw_spi_t *spi)
{
    spi->dev->hal->write(spi->dev->hal->user_data,
                          spi->cs_port, spi->cs_pin, true);
}

uint8_t sw_spi_transfer(sw_spi_t *spi, uint8_t tx)
{
    uint8_t rx = 0;
    spi->dev->hal->enter_critical();

    for (int i = 7; i >= 0; i--) {
        bool bit = (tx >> i) & 1;

        if (spi->cpha) {
            /* CPHA=1: 数据在第二个边沿采样 */
            spi->dev->hal->write(spi->dev->hal->user_data,
                                  spi->sck_port, spi->sck_pin, !spi->cpol);
            spi->dev->hal->delay_us(spi->delay_us);
            spi->dev->hal->write(spi->dev->hal->user_data,
                                  spi->mosi_port, spi->mosi_pin, bit);
            spi->dev->hal->write(spi->dev->hal->user_data,
                                  spi->sck_port, spi->sck_pin, spi->cpol);
            spi->dev->hal->delay_us(spi->delay_us);
            if (spi->dev->hal->read(spi->dev->hal->user_data,
                                     spi->miso_port, spi->miso_pin))
                rx |= (1 << i);
        } else {
            /* CPHA=0: 数据在第一个边沿采样 */
            spi->dev->hal->write(spi->dev->hal->user_data,
                                  spi->mosi_port, spi->mosi_pin, bit);
            spi->dev->hal->write(spi->dev->hal->user_data,
                                  spi->sck_port, spi->sck_pin, !spi->cpol);
            spi->dev->hal->delay_us(spi->delay_us);
            if (spi->dev->hal->read(spi->dev->hal->user_data,
                                     spi->miso_port, spi->miso_pin))
                rx |= (1 << i);
            spi->dev->hal->write(spi->dev->hal->user_data,
                                  spi->sck_port, spi->sck_pin, spi->cpol);
            spi->dev->hal->delay_us(spi->delay_us);
        }
    }
    spi->dev->hal->exit_critical();
    return rx;
}

/* ============================================================
 * 注意事项
 * ============================================================
 * 1. 浮空输入: 未使用的 GPIO 不要配置为浮空输入, 会增加功耗,
 *    应配置为模拟模式或上拉/下拉输入。
 * 2. 复用冲突: 配置 GPIO 前确认引脚未被其他外设 (UART/SPI/TIM) 占用,
 *    部分 MCU 的引脚映射需要检查 AF (Alternate Function) 表。
 * 3. 驱动能力: MCU GPIO 驱动能力通常 4~25mA,
 *    直接驱动 LED 需串联限流电阻, 驱动继电器/电机需加三极管/MOSFET。
 * 4. 中断处理: 中断回调中避免耗时操作 (如 delay, printf),
 *    仅设置标志位, 在主循环中处理逻辑。
 * 5. 消抖: 机械按键抖动 5~20ms, 软件消抖用延时或状态机,
 *    硬件消抖加 RC 滤波 (R=10k, C=100nF) 或施密特触发器。
 * 6. 软件 I2C/SPI: 关中断保证时序完整性, 速度受 delay_us 精度限制,
 *    适用于无硬件外设的引脚或引脚不够时的扩展。
 * 7. 低功耗: 进入低功耗模式前, 将所有 GPIO 配置为已知状态,
 *    避免悬空导致漏电流。外部中断引脚需保留唤醒功能。
 * 8. 上电安全 (实际项目验证): 配置 GPIO 为输出前, 先设置电平到关断态,
 *    避免上电瞬间误触发蜂鸣器/继电器/LED:
 *      HAL_GPIO_WritePin(PORT, PIN, GPIO_PIN_SET);   // 先设关断电平
 *      GPIO_Init(... GPIO_MODE_OUTPUT_PP ...);        // 再配置输出
 * 9. 有效电平抽象 (实际项目验证): 不同模块高/低有效可能混合, 用统一函数封装:
 *      void io_write(GPIO_TypeDef *port, uint16_t pin, uint8_t active, uint8_t on) {
 *          HAL_GPIO_WritePin(port, pin,
 *              (active ^ on) ? GPIO_PIN_RESET : GPIO_PIN_SET);
 *      }
 * 10. 矩阵键盘扫描 (实际项目验证): 4x4 矩阵用行列扫描 + 消抖,
 *     消抖用"连续两次读到相同键且超过消抖时间"模式:
 *      if (raw != lastRawKey) { lastRawKey = raw; lastRawTick = now; return KEY_NONE; }
 *      if ((now - lastRawTick) < DEBOUNCE_MS) return KEY_NONE;
 * 11. BSRR 直接寄存器 (实际项目验证): 软件 I2C/SPI 性能优化时,
 *     用 port->BSRR = pin (置位) / port->BSRR = pin<<16 (复位) 代替 HAL_GPIO_WritePin,
 *     实测快 5~10 倍。
 */
