/**
 * @file    driver-template-spi.c
 * @brief   SPI 设备驱动模板 (跨平台)
 *
 * 本模板提供 SPI 设备驱动的标准化框架，包含：
 *   - SPI HAL 抽象层定义 (平台无关接口)
 *   - SPI 读写函数原型
 *   - 设备初始化模板 (CS 控制、时钟配置)
 *   - 寄存器读 / 写模板
 *   - DMA 模式说明
 *   - 使用说明注释
 *
 * ## 使用方法
 *   1. 实现 spi_hal_t 中的平台适配函数
 *   2. 用 spi_device_init() 初始化设备句柄, 指定 CS 引脚和时钟模式
 *   3. 调用 spi_reg_read / spi_reg_write / spi_transfer 进行操作
 *   4. 修改寄存器命令定义以匹配目标芯片
 *
 * ## SPI 时钟模式
 *   Mode 0: CPOL=0, CPHA=0  (空闲低, 上升沿采样) -- 最常用
 *   Mode 1: CPOL=0, CPHA=1  (空闲低, 下降沿采样)
 *   Mode 2: CPOL=1, CPHA=0  (空闲高, 下降沿采样)
 *   Mode 3: CPOL=1, CPHA=1  (空闲高, 上升沿采样)
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================
 * 1. SPI HAL 抽象层定义
 * ============================================================ */

/** SPI 状态码 */
typedef enum {
    SPI_OK = 0,
    SPI_ERR_TIMEOUT,
    SPI_ERR_TRANSFER,
    SPI_ERR_INVALID_PARAM,
    SPI_ERR_BUSY,
    SPI_ERR_DMA,
} spi_status_t;

/** SPI 时钟模式 */
typedef enum {
    SPI_MODE_0 = 0,  /* CPOL=0, CPHA=0 */
    SPI_MODE_1,      /* CPOL=0, CPHA=1 */
    SPI_MODE_2,      /* CPOL=1, CPHA=0 */
    SPI_MODE_3,      /* CPOL=1, CPHA=1 */
} spi_mode_t;

/** SPI 位序 */
typedef enum {
    SPI_MSB_FIRST = 0,
    SPI_LSB_FIRST,
} spi_bit_order_t;

/**
 * SPI HAL 接口 - 平台适配层
 */
typedef struct {
    /** 初始化 SPI 总线 */
    spi_status_t (*init)(void *user_data, uint32_t clock_hz, spi_mode_t mode,
                         spi_bit_order_t bit_order);
    /** 反初始化 */
    spi_status_t (*deinit)(void *user_data);
    /**
     * 全双工传输 (同步阻塞)
     * @param tx_buf  发送缓冲 (NULL 表示发送 0xFF dummy)
     * @param rx_buf  接收缓冲 (NULL 表示忽略接收)
     * @param len     传输长度
     * @param timeout_ms 超时
     */
    spi_status_t (*transfer)(void *user_data,
                              const uint8_t *tx_buf, uint8_t *rx_buf,
                              uint16_t len, uint32_t timeout_ms);
    /**
     * DMA 全双工传输 (异步)
     * @return 传输启动状态, 完成后通过 callback 通知
     */
    spi_status_t (*transfer_dma)(void *user_data,
                                  const uint8_t *tx_buf, uint8_t *rx_buf,
                                  uint16_t len);
    /** 等待 DMA 传输完成 (阻塞) */
    spi_status_t (*dma_wait_complete)(void *user_data, uint32_t timeout_ms);
    /** CS 片选拉低 */
    void (*cs_select)(void *user_data);
    /** CS 片选拉高 */
    void (*cs_deselect)(void *user_data);
    /** 毫秒延时 */
    void (*delay_ms)(uint32_t ms);
    /** 用户数据指针 (SPI 端口实例 / CS 引脚信息等) */
    void *user_data;
} spi_hal_t;

/* ============================================================
 * 2. SPI 设备句柄
 * ============================================================ */

#define SPI_DEFAULT_TIMEOUT_MS  100

typedef struct {
    spi_hal_t       *hal;            /* HAL 接口 */
    uint32_t         clock_hz;       /* 时钟频率 */
    spi_mode_t       mode;           /* 时钟模式 */
    spi_bit_order_t  bit_order;      /* 位序 */
    bool             initialized;
} spi_device_t;

/* ============================================================
 * 3. 设备初始化模板
 * ============================================================ */

/**
 * 初始化 SPI 设备
 * @param dev       设备句柄
 * @param hal       HAL 接口
 * @param clock_hz  时钟频率 (Hz), 如 1000000 = 1MHz
 * @param mode      SPI 时钟模式
 * @param bit_order 位序
 */
spi_status_t spi_device_init(spi_device_t *dev, spi_hal_t *hal,
                              uint32_t clock_hz, spi_mode_t mode,
                              spi_bit_order_t bit_order)
{
    if (!dev || !hal)
        return SPI_ERR_INVALID_PARAM;

    dev->hal        = hal;
    dev->clock_hz   = clock_hz;
    dev->mode       = mode;
    dev->bit_order  = bit_order;
    dev->initialized = false;

    spi_status_t st = hal->init(hal->user_data, clock_hz, mode, bit_order);
    if (st != SPI_OK)
        return st;

    /* 初始状态 CS 拉高 (空闲) */
    hal->cs_deselect(hal->user_data);

    dev->initialized = true;
    return SPI_OK;
}

void spi_device_deinit(spi_device_t *dev)
{
    if (!dev || !dev->hal)
        return;
    dev->hal->cs_deselect(dev->hal->user_data);
    dev->hal->deinit(dev->hal->user_data);
    dev->initialized = false;
}

/* ============================================================
 * 4. SPI 读写函数原型与实现
 * ============================================================ */

/**
 * 全双工传输 (自动管理 CS)
 * CS 拉低 -> 传输 -> CS 拉高
 */
spi_status_t spi_transfer(spi_device_t *dev,
                           const uint8_t *tx_buf, uint8_t *rx_buf,
                           uint16_t len)
{
    if (!dev || !dev->hal || len == 0)
        return SPI_ERR_INVALID_PARAM;

    dev->hal->cs_select(dev->hal->user_data);
    spi_status_t st = dev->hal->transfer(dev->hal->user_data,
                                          tx_buf, rx_buf, len,
                                          SPI_DEFAULT_TIMEOUT_MS);
    dev->hal->cs_deselect(dev->hal->user_data);
    return st;
}

/**
 * 只写 (丢弃接收数据)
 */
spi_status_t spi_write(spi_device_t *dev, const uint8_t *data, uint16_t len)
{
    return spi_transfer(dev, data, NULL, len);
}

/**
 * 只读 (发送 dummy 0xFF)
 */
spi_status_t spi_read(spi_device_t *dev, uint8_t *data, uint16_t len)
{
    return spi_transfer(dev, NULL, data, len);
}

/* ============================================================
 * 5. 寄存器读 / 写模板
 * ============================================================ */

/*
 * 常见 SPI 寄存器操作格式:
 *   写: [W_CMD | REG] [DATA...]
 *   读: [R_CMD | REG] [DUMMY...] [DATA...]
 *
 * 以 W25Q64 Flash 为例:
 *   读命令:  0x03 + 24bit 地址 + 数据
 *   写使能:  0x06
 *   写状态:  0x01 + 状态字节
 *
 * 以 ADC 芯片 (如 MCP3208) 为例:
 *   读: [START_BIT] [CFG] [DONTCARE] -> [NULL] [MSB] [LSB]
 *
 * 以下为通用寄存器读写, 适配大多数 SPI 传感器/存储器
 */

#define SPI_REG_WRITE_CMD   0x00   /* 写命令掩码 (按芯片手册修改) */
#define SPI_REG_READ_CMD    0x80   /* 读命令掩码 (最高位=1 表示读) */

/**
 * 写单个寄存器
 * 帧: [CS↓] [REG | WRITE_CMD] [VALUE] [CS↑]
 */
spi_status_t spi_reg_write(spi_device_t *dev, uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { (uint8_t)(reg | SPI_REG_WRITE_CMD), value };
    return spi_write(dev, tx, 2);
}

/**
 * 读单个寄存器
 * 帧: [CS↓] [REG | READ_CMD] [DUMMY] [CS↑]
 * 返回第二个字节 (部分芯片需要更多 dummy, 按手册调整)
 */
spi_status_t spi_reg_read(spi_device_t *dev, uint8_t reg, uint8_t *value)
{
    uint8_t tx[2] = { (uint8_t)(reg | SPI_REG_READ_CMD), 0xFF };
    uint8_t rx[2];
    spi_status_t st = spi_transfer(dev, tx, rx, 2);
    if (st != SPI_OK)
        return st;
    *value = rx[1];
    return SPI_OK;
}

/**
 * 批量写寄存器
 */
spi_status_t spi_reg_write_burst(spi_device_t *dev, uint8_t reg,
                                  const uint8_t *data, uint16_t len)
{
    if (!dev || !dev->hal || !data || len == 0)
        return SPI_ERR_INVALID_PARAM;

    dev->hal->cs_select(dev->hal->user_data);

    /* 先发寄存器地址 */
    uint8_t cmd = (uint8_t)(reg | SPI_REG_WRITE_CMD);
    spi_status_t st = dev->hal->transfer(dev->hal->user_data, &cmd, NULL, 1,
                                          SPI_DEFAULT_TIMEOUT_MS);
    if (st != SPI_OK) {
        dev->hal->cs_deselect(dev->hal->user_data);
        return st;
    }

    /* 再发数据 */
    st = dev->hal->transfer(dev->hal->user_data, data, NULL, len,
                             SPI_DEFAULT_TIMEOUT_MS);
    dev->hal->cs_deselect(dev->hal->user_data);
    return st;
}

/**
 * 批量读寄存器
 */
spi_status_t spi_reg_read_burst(spi_device_t *dev, uint8_t reg,
                                 uint8_t *buf, uint16_t len)
{
    if (!dev || !dev->hal || !buf || len == 0)
        return SPI_ERR_INVALID_PARAM;

    dev->hal->cs_select(dev->hal->user_data);

    /* 发送读命令 */
    uint8_t cmd = (uint8_t)(reg | SPI_REG_READ_CMD);
    spi_status_t st = dev->hal->transfer(dev->hal->user_data, &cmd, NULL, 1,
                                          SPI_DEFAULT_TIMEOUT_MS);
    if (st != SPI_OK) {
        dev->hal->cs_deselect(dev->hal->user_data);
        return st;
    }

    /* 读取数据 (发送 dummy) */
    st = dev->hal->transfer(dev->hal->user_data, NULL, buf, len,
                             SPI_DEFAULT_TIMEOUT_MS);
    dev->hal->cs_deselect(dev->hal->user_data);
    return st;
}

/* ============================================================
 * 6. DMA 模式
 * ============================================================ */

/**
 * DMA 传输 (异步, 适用于大数据量如 Flash 读取、显示屏刷新)
 *
 * 使用流程:
 *   1. spi_transfer_dma() 启动传输
 *   2. spi_dma_wait() 阻塞等待完成 (或注册回调)
 *   3. 处理接收数据
 *
 * 注意:
 *   - DMA 传输期间 CPU 可执行其他任务
 *   - tx_buf 和 rx_buf 必须在 DMA 完成前保持有效 (不可为栈上局部变量)
 *   - 部分 MCU 的 DMA 要求缓冲区地址对齐
 *   - DMA 传输大小受 DMA 通道最大传输次数限制 (如 STM32 为 65535)
 */
spi_status_t spi_transfer_dma(spi_device_t *dev,
                               const uint8_t *tx_buf, uint8_t *rx_buf,
                               uint16_t len)
{
    if (!dev || !dev->hal || len == 0)
        return SPI_ERR_INVALID_PARAM;

    dev->hal->cs_select(dev->hal->user_data);
    spi_status_t st = dev->hal->transfer_dma(dev->hal->user_data,
                                              tx_buf, rx_buf, len);
    if (st != SPI_OK) {
        dev->hal->cs_deselect(dev->hal->user_data);
        return st;
    }
    return SPI_OK;
}

/** 等待 DMA 完成 */
spi_status_t spi_dma_wait(spi_device_t *dev, uint32_t timeout_ms)
{
    if (!dev || !dev->hal)
        return SPI_ERR_INVALID_PARAM;

    spi_status_t st = dev->hal->dma_wait_complete(dev->hal->user_data, timeout_ms);
    dev->hal->cs_deselect(dev->hal->user_data);
    return st;
}

/* ============================================================
 * 7. 完整使用示例 (伪代码)
 * ============================================================ */

/*
// --- 平台适配: STM32 HAL ---
static spi_status_t stm32_spi_init(void *ud, uint32_t clk, spi_mode_t mode,
                                    spi_bit_order_t order) {
    SPI_HandleTypeDef *hspi = (SPI_HandleTypeDef*)ud;
    hspi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; // 按clk计算
    hspi->Init.CLKPolarity = (mode >> 1) & 1;
    hspi->Init.CLKPhase = mode & 1;
    hspi->Init.FirstBit = order ? SPI_FIRSTBIT_LSB : SPI_FIRSTBIT_MSB;
    HAL_SPI_Init(hspi);
    return SPI_OK;
}
static spi_status_t stm32_spi_transfer(void *ud, const uint8_t *tx,
        uint8_t *rx, uint16_t len, uint32_t timeout) {
    SPI_HandleTypeDef *hspi = (SPI_HandleTypeDef*)ud;
    uint8_t dummy_tx = 0xFF;
    const uint8_t *p_tx = tx ? tx : &dummy_tx;
    if (rx) {
        if (HAL_SPI_TransmitReceive(hspi, (uint8_t*)p_tx, rx, len, timeout) == HAL_OK)
            return SPI_OK;
    } else {
        if (HAL_SPI_Transmit(hspi, (uint8_t*)p_tx, len, timeout) == HAL_OK)
            return SPI_OK;
    }
    return SPI_ERR_TRANSFER;
}
static void stm32_cs_select(void *ud) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
}
static void stm32_cs_deselect(void *ud) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
}
static spi_status_t stm32_spi_dma(void *ud, const uint8_t *tx,
        uint8_t *rx, uint16_t len) {
    SPI_HandleTypeDef *hspi = (SPI_HandleTypeDef*)ud;
    if (HAL_SPI_TransmitReceive_DMA(hspi, (uint8_t*)tx, rx, len) == HAL_OK)
        return SPI_OK;
    return SPI_ERR_DMA;
}
static spi_status_t stm32_dma_wait(void *ud, uint32_t timeout) {
    SPI_HandleTypeDef *hspi = (SPI_HandleTypeDef*)ud;
    uint32_t start = HAL_GetTick();
    while (HAL_SPI_GetState(hspi) != HAL_SPI_STATE_READY) {
        if (HAL_GetTick() - start > timeout) return SPI_ERR_TIMEOUT;
    }
    return SPI_OK;
}

// --- 主函数 ---
int main(void) {
    spi_hal_t hal = {
        .init          = stm32_spi_init,
        .deinit        = NULL,
        .transfer      = stm32_spi_transfer,
        .transfer_dma  = stm32_spi_dma,
        .dma_wait_complete = stm32_dma_wait,
        .cs_select     = stm32_cs_select,
        .cs_deselect   = stm32_cs_deselect,
        .delay_ms      = HAL_Delay,
        .user_data     = &hspi1,
    };
    spi_device_t dev;
    spi_device_init(&dev, &hal, 1000000, SPI_MODE_0, SPI_MSB_FIRST);

    // 读写寄存器
    uint8_t id;
    spi_reg_read(&dev, 0x0F, &id);   // 读取芯片 ID
    spi_reg_write(&dev, 0x20, 0x0F); // 写配置寄存器

    // DMA 批量读取
    static uint8_t rx_buf[256];       // DMA 必须用静态/全局缓冲区
    spi_transfer_dma(&dev, NULL, rx_buf, 256);
    spi_dma_wait(&dev, 100);
}
*/

/* ============================================================
 * 注意事项
 * ============================================================
 * 1. CS 控制: 每次 SPI 传输前后必须正确控制 CS (片选),
 *    多从机共享同一 SPI 总线时, 每个从机使用独立 CS 引脚。
 * 2. 时钟模式: 不同器件支持不同的 CPOL/CPHA, 必须按数据手册配置,
 *    错误的模式会导致数据移位或无法通信。
 * 3. 时钟速度: 最大时钟频率取决于器件 (如 W25Q64 最高 104MHz),
 *    PCB 走线较长时需降低时钟。
 * 4. DMA 限制:
 *    - 缓冲区必须在 DMA 可访问的内存区域 (STM32 需注意 D-Cache 一致性)
 *    - 传输长度不超过 DMA 通道最大值
 *    - 传输期间不可修改缓冲区内容
 * 5. 多设备共享总线: 切换设备前确保上一设备的 CS 已拉高,
 *    且重新配置时钟模式和频率 (如果不同设备配置不同)。
 * 6. 虚假读取: SPI 全双工特性决定每次传输都同时收发,
 *    只读时需要发送 dummy 字节 (通常 0x00 或 0xFF)。
 */
