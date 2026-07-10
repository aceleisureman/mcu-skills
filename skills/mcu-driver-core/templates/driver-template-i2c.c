/**
 * @file    driver-template-i2c.c
 * @brief   I2C 设备驱动模板 (跨平台)
 *
 * 本模板提供 I2C 设备驱动的标准化框架，包含：
 *   - I2C HAL 抽象层定义 (平台无关接口)
 *   - I2C 读写函数原型
 *   - 设备初始化模板
 *   - 寄存器读/写/批量读模板
 *   - CRC-8 校验示例 (多项式 0x31, 适配 SHTxx/BMPxx 等)
 *   - 使用说明注释
 *
 * ## 使用方法
 *   1. 实现 i2c_hal_t 中的平台适配函数 (STM32 HAL / ESP-IDF / Arduino Wire)
 *   2. 用 i2c_device_init() 初始化设备句柄
 *   3. 调用 i2c_reg_read / i2c_reg_write / i2c_reg_read_burst 进行寄存器操作
 *   4. 修改 DEVICE_I2C_ADDR 和寄存器定义以匹配目标芯片
 *
 * ## 平台适配示例
 *   STM32:  使用 HAL_I2C_Master_Transmit / HAL_I2C_Master_Receive
 *   ESP32:  使用 i2c_master_write_to_device / i2c_master_read_from_device
 *   Arduino: 使用 Wire.beginTransmission / Wire.requestFrom
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================
 * 1. I2C HAL 抽象层定义
 * ============================================================ */

/** I2C 状态码 */
typedef enum {
    I2C_OK = 0,
    I2C_ERR_TIMEOUT,
    I2C_ERR_NACK,
    I2C_ERR_BUS_BUSY,
    I2C_ERR_INVALID_PARAM,
    I2C_ERR_CRC,
    I2C_ERR_NOT_FOUND,
} i2c_status_t;

/**
 * I2C HAL 接口 - 平台适配层
 * 每个平台需要实现以下函数指针，通过 i2c_hal_t 注入
 */
typedef struct {
    /** 初始化 I2C 总线 */
    i2c_status_t (*init)(void *user_data);
    /** 反初始化 / 释放总线 */
    i2c_status_t (*deinit)(void *user_data);
    /**
     * 向从机写入数据
     * @param addr      7 位从机地址 (不含 R/W 位)
     * @param data      待写入数据
     * @param len       数据长度
     * @param timeout_ms 超时时间
     */
    i2c_status_t (*write)(void *user_data, uint8_t addr,
                          const uint8_t *data, uint16_t len, uint32_t timeout_ms);
    /**
     * 从从机读取数据
     * @param addr      7 位从机地址
     * @param data      读取缓冲区
     * @param len       期望读取长度
     * @param timeout_ms 超时时间
     */
    i2c_status_t (*read)(void *user_data, uint8_t addr,
                         uint8_t *data, uint16_t len, uint32_t timeout_ms);
    /** 毫秒延时 */
    void (*delay_ms)(uint32_t ms);
    /** 用户自定义数据指针 (存放端口/总线实例等) */
    void *user_data;
} i2c_hal_t;

/* ============================================================
 * 2. I2C 设备句柄
 * ============================================================ */

#define DEVICE_I2C_ADDR         0x44    /* 替换为目标器件地址 */
#define I2C_DEFAULT_TIMEOUT_MS  100

typedef struct {
    i2c_hal_t   *hal;           /* HAL 接口指针 */
    uint8_t      dev_addr;      /* 从机地址 */
    bool         initialized;   /* 初始化标志 */
} i2c_device_t;

/* ============================================================
 * 3. I2C 读写函数原型
 * ============================================================ */

/**
 * 向从机发送写请求 (含寄存器地址前缀)
 * 帧格式: [START] [Addr+W] [RegAddr] [Data0] ... [DataN] [STOP]
 */
static inline i2c_status_t i2c_write_reg_raw(i2c_device_t *dev,
                                              uint8_t reg,
                                              const uint8_t *data,
                                              uint16_t len)
{
    if (!dev || !dev->hal || !data || len == 0)
        return I2C_ERR_INVALID_PARAM;

    /* 拼接寄存器地址 + 数据 */
    uint8_t buf[1 + 64];  /* 1 字节 reg + 最多 64 字节数据, 按需调整 */
    if (len > sizeof(buf) - 1)
        return I2C_ERR_INVALID_PARAM;

    buf[0] = reg;
    for (uint16_t i = 0; i < len; i++)
        buf[1 + i] = data[i];

    return dev->hal->write(dev->hal->user_data, dev->dev_addr,
                           buf, len + 1, I2C_DEFAULT_TIMEOUT_MS);
}

/**
 * 从从机读取数据 (先写寄存器地址，再 restart 读)
 * 帧格式: [START] [Addr+W] [RegAddr] [RESTART] [Addr+R] [Data0]...[DataN] [STOP]
 */
static inline i2c_status_t i2c_read_reg_raw(i2c_device_t *dev,
                                             uint8_t reg,
                                             uint8_t *data,
                                             uint16_t len)
{
    if (!dev || !dev->hal || !data || len == 0)
        return I2C_ERR_INVALID_PARAM;

    /* 先写入寄存器地址 */
    i2c_status_t st = dev->hal->write(dev->hal->user_data, dev->dev_addr,
                                      &reg, 1, I2C_DEFAULT_TIMEOUT_MS);
    if (st != I2C_OK)
        return st;

    /* 再读取数据 */
    return dev->hal->read(dev->hal->user_data, dev->dev_addr,
                          data, len, I2C_DEFAULT_TIMEOUT_MS);
}

/* ============================================================
 * 4. 设备初始化模板
 * ============================================================ */

/**
 * 初始化 I2C 设备
 * @param dev       设备句柄
 * @param hal       HAL 接口 (由调用方提供)
 * @param dev_addr  从机地址 (7 bit)
 * @return I2C_OK / 错误码
 */
i2c_status_t i2c_device_init(i2c_device_t *dev, i2c_hal_t *hal, uint8_t dev_addr)
{
    if (!dev || !hal)
        return I2C_ERR_INVALID_PARAM;

    dev->hal    = hal;
    dev->dev_addr = dev_addr;
    dev->initialized = false;

    /* 初始化总线 */
    i2c_status_t st = hal->init(hal->user_data);
    if (st != I2C_OK)
        return st;

    /* 可选: 探测设备是否在线 */
    uint8_t dummy;
    st = hal->read(hal->user_data, dev_addr, &dummy, 0, I2C_DEFAULT_TIMEOUT_MS);
    /* 部分平台读 0 字节会 NACK, 可改为写 0 字节探测 */
    /* st = hal->write(hal->user_data, dev_addr, &dummy, 0, I2C_DEFAULT_TIMEOUT_MS); */

    dev->initialized = true;
    return I2C_OK;
}

/** 反初始化 */
void i2c_device_deinit(i2c_device_t *dev)
{
    if (!dev || !dev->hal)
        return;
    dev->hal->deinit(dev->hal->user_data);
    dev->initialized = false;
}

/* ============================================================
 * 5. 寄存器读 / 写 / 批量读模板
 * ============================================================ */

/** 读单个寄存器 */
i2c_status_t i2c_reg_read(i2c_device_t *dev, uint8_t reg, uint8_t *val)
{
    return i2c_read_reg_raw(dev, reg, val, 1);
}

/** 读 16 位寄存器 (大端) */
i2c_status_t i2c_reg_read16(i2c_device_t *dev, uint8_t reg, uint16_t *val)
{
    uint8_t buf[2];
    i2c_status_t st = i2c_read_reg_raw(dev, reg, buf, 2);
    if (st != I2C_OK)
        return st;
    *val = (uint16_t)((buf[0] << 8) | buf[1]);
    return I2C_OK;
}

/** 写单个寄存器 */
i2c_status_t i2c_reg_write(i2c_device_t *dev, uint8_t reg, uint8_t val)
{
    return i2c_write_reg_raw(dev, reg, &val, 1);
}

/** 写 16 位寄存器 (大端) */
i2c_status_t i2c_reg_write16(i2c_device_t *dev, uint8_t reg, uint16_t val)
{
    uint8_t buf[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };
    return i2c_write_reg_raw(dev, reg, buf, 2);
}

/** 批量读取寄存器 */
i2c_status_t i2c_reg_read_burst(i2c_device_t *dev, uint8_t reg,
                                 uint8_t *buf, uint16_t len)
{
    return i2c_read_reg_raw(dev, reg, buf, len);
}

/** 批量写入寄存器 */
i2c_status_t i2c_reg_write_burst(i2c_device_t *dev, uint8_t reg,
                                  const uint8_t *buf, uint16_t len)
{
    return i2c_write_reg_raw(dev, reg, buf, len);
}

/* ============================================================
 * 6. CRC-8 校验 (多项式 0x31, 初始值 0xFF)
 *    适用于 SHT30/SHT40/BMP280/BME280 等传感器
 * ============================================================ */

/**
 * @brief CRC-8 计算 (Polynomial = 0x31, Init = 0xFF)
 * @param data 数据指针
 * @param len  数据长度
 * @return CRC 校验值
 */
uint8_t i2c_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ 0x31);
            else
                crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* ============================================================
 * 7. CRC-8 校验变体 (Dallas/Maxim 多项式 0x8C / 0x31 反向查表)
 *    适用于 DS28E18 等, 若器件使用不同多项式请按手册替换
 * ============================================================ */

/**
 * @brief CRC-8 (Dallas/Maxim, Polynomial 0x8C 反向)
 *        适用于 DS18B20 等器件 (与 1-Wire 配合)
 */
uint8_t i2c_crc8_dallas(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    while (len--) {
        uint8_t inbyte = *data++;
        for (uint8_t i = 0; i < 8; i++) {
            uint8_t mix = (uint8_t)((crc ^ inbyte) & 0x01);
            crc >>= 1;
            if (mix)
                crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}

/* ============================================================
 * 8. I2C 总线扫描 (调试用)
 * ============================================================ */

/**
 * @brief 扫描 I2C 总线上的所有在线设备
 * @param hal       HAL 接口
 * @param found     回调函数, 每发现一个设备调用
 * @return 发现的设备数量
 */
uint8_t i2c_scan(i2c_hal_t *hal, void (*found_cb)(uint8_t addr))
{
    uint8_t count = 0;
    uint8_t dummy;

    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        /* 尝试读取 0 字节或写入 0 字节, 不产生 NACK 即认为设备在线 */
        if (hal->write(hal->user_data, addr, &dummy, 0, 10) == I2C_OK) {
            count++;
            if (found_cb)
                found_cb(addr);
        }
    }
    return count;
}

/* ============================================================
 * 9. 带重试的寄存器读取 (提高鲁棒性)
 * ============================================================ */

/**
 * @brief 带重试机制的寄存器读取
 * @param dev        设备句柄
 * @param reg        寄存器地址
 * @param buf        读取缓冲区
 * @param len        读取长度
 * @param retries    重试次数
 * @param verify_crc 是否进行 CRC 校验 (buf 最后一个字节为 CRC)
 * @return I2C_OK / 错误码
 */
i2c_status_t i2c_reg_read_with_retry(i2c_device_t *dev, uint8_t reg,
                                      uint8_t *buf, uint16_t len,
                                      uint8_t retries, bool verify_crc)
{
    i2c_status_t st;
    for (uint8_t i = 0; i <= retries; i++) {
        st = i2c_read_reg_raw(dev, reg, buf, len);
        if (st != I2C_OK) {
            dev->hal->delay_ms(2);
            continue;
        }
        if (verify_crc && len >= 3) {
            /* 假设最后 1 字节为前 (len-1) 字节的 CRC */
            uint8_t crc = i2c_crc8(buf, (uint8_t)(len - 1));
            if (crc != buf[len - 1])
                continue;  /* CRC 错误, 重试 */
        }
        return I2C_OK;
    }
    return st;
}

/* ============================================================
 * 10. 完整使用示例 (伪代码)
 * ============================================================ */

/*
// --- 平台适配: STM32 HAL ---
static i2c_status_t stm32_i2c_init(void *ud) {
    MX_I2C1_Init();
    return I2C_OK;
}
static i2c_status_t stm32_i2c_write(void *ud, uint8_t addr,
        const uint8_t *data, uint16_t len, uint32_t timeout) {
    if (HAL_I2C_Master_Transmit((I2C_HandleTypeDef*)ud, addr << 1,
            (uint8_t*)data, len, timeout) == HAL_OK)
        return I2C_OK;
    return I2C_ERR_TIMEOUT;
}
static i2c_status_t stm32_i2c_read(void *ud, uint8_t addr,
        uint8_t *data, uint16_t len, uint32_t timeout) {
    if (HAL_I2C_Master_Receive((I2C_HandleTypeDef*)ud, addr << 1,
            data, len, timeout) == HAL_OK)
        return I2C_OK;
    return I2C_ERR_TIMEOUT;
}

// --- 主函数 ---
int main(void) {
    i2c_hal_t hal = {
        .init     = stm32_i2c_init,
        .deinit   = NULL,
        .write    = stm32_i2c_write,
        .read     = stm32_i2c_read,
        .delay_ms = HAL_Delay,
        .user_data = &hi2c1,
    };
    i2c_device_t dev;
    i2c_device_init(&dev, &hal, 0x44);

    // 读寄存器
    uint8_t id;
    i2c_reg_read(&dev, 0xD0, &id);  // BME280 chip ID

    // 写寄存器
    i2c_reg_write(&dev, 0xF4, 0x27);  // BME280 ctrl_meas

    // 带重试 + CRC 读取 (SHT30)
    uint8_t data[6];
    i2c_reg_read_with_retry(&dev, 0x00, data, 6, 3, false);

    // 扫描总线
    i2c_scan(&hal, NULL);
}
*/

/* ============================================================
 * 注意事项
 * ============================================================
 * 1. I2C 地址: 数据手册通常给出 7 位地址, 部分平台 (如 STM32 HAL)
 *    需要左移 1 位 (addr << 1) 后传入, 请在 write/read 适配中处理。
 * 2. 上拉电阻: SCL/SDA 必须接 4.7k~10k 欧上拉, 部分 MCU 内部上拉
 *    阻值较大 (20k~50k), 仅适用于低速短距离。
 * 3. 时钟 stretching: 部分从机需要时钟拉伸, 确保主控支持。
 * 4. 多设备共享总线: 每个设备地址必须唯一, 地址冲突时用 I2C 多路复用器。
 * 5. 长线传输: 总线电容 > 400pF 时需使用 I2C 中继器 (如 PCA9515)。
 * 6. 线程安全: 多线程共享同一 I2C 总线时, 需要在外层加互斥锁。
 * 7. 上电安全: GPIO 配置为输出前先设电平到关断态, 避免上电瞬间误触发
 *    (实际项目: 先 HAL_GPIO_WritePin 设关断电平, 再 HAL_GPIO_Init 配置输出)。
 * 8. DWT 微秒延时: 软件 I2C 推荐用 DWT CYCCNT 实现精确微秒延时:
 *      CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
 *      DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
 *      void delay_us(uint32_t us) {
 *          uint32_t start = DWT->CYCCNT;
 *          uint32_t ticks = us * (SystemCoreClock / 1000000U);
 *          while ((DWT->CYCCNT - start) < ticks);
 *      }
 * 9. BSRR 直接寄存器: 软件 I2C 性能优化时, 用 port->BSRR 代替 HAL_GPIO_WritePin
 *    (实际项目测得快 5~10 倍, 适合 OLED 高刷屏场景)。
 * 10. 地址探测: 初始化时读取芯片 ID 寄存器验证, 支持多地址器件自动探测
 *     (实际项目: MAX30102 尝试 0x57 和 0xAE 两个地址读 PartID=0x15)。
 */
