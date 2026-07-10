/**
 * @file    driver-template-adc.c
 * @brief   ADC 驱动模板 (跨平台)
 *
 * 本模板提供 ADC 驱动的标准化框架，包含：
 *   - ADC HAL 抽象层定义 (平台无关接口)
 *   - 单次采样模板
 *   - 多次采样平均滤波
 *   - DMA 连续采集模板
 *   - 电压转换公式
 *
 * ## 使用方法
 *   1. 实现 adc_hal_t 中的平台适配函数
 *   2. 用 adc_init() 初始化, 指定参考电压和分辨率
 *   3. 单次采样: adc_sample_single()
 *   4. 多次平均: adc_sample_averaged()
 *   5. DMA 连续采集: adc_start_dma() / adc_stop_dma()
 *   6. 电压转换: adc_to_voltage()
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================
 * 1. ADC HAL 抽象层定义
 * ============================================================ */

typedef enum {
    ADC_OK = 0,
    ADC_ERR_TIMEOUT,
    ADC_ERR_PARAM,
    ADC_ERR_BUSY,
    ADC_ERR_DMA,
    ADC_ERR_NOT_READY,
} adc_status_t;

/** ADC 分辨率 */
typedef enum {
    ADC_RES_8BIT  = 8,
    ADC_RES_10BIT = 10,
    ADC_RES_12BIT = 12,
    ADC_RES_14BIT = 14,
    ADC_RES_16BIT = 16,
} adc_resolution_t;

/**
 * ADC HAL 接口
 */
typedef struct {
    /** 初始化 ADC */
    adc_status_t (*init)(void *user_data, adc_resolution_t resolution);
    /** 反初始化 */
    adc_status_t (*deinit)(void *user_data);
    /**
     * 配置 ADC 通道
     * @param channel  通道号
     * @param sampling_time  采样时间 (cycles, 按平台支持)
     */
    adc_status_t (*config_channel)(void *user_data, uint8_t channel,
                                    uint32_t sampling_time);
    /**
     * 启动单次转换并读取结果 (阻塞)
     * @param channel  通道号
     * @param value    转换结果
     * @param timeout_ms 超时
     */
    adc_status_t (*read_channel)(void *user_data, uint8_t channel,
                                  uint16_t *value, uint32_t timeout_ms);
    /**
     * 启动 DMA 连续采集
     * @param channel  通道号
     * @param buf      DMA 缓冲区 (必须为静态/全局)
     * @param len      缓冲区长度
     */
    adc_status_t (*start_dma)(void *user_data, uint8_t channel,
                               uint16_t *buf, uint16_t len);
    /** 停止 DMA */
    adc_status_t (*stop_dma)(void *user_data);
    /** 获取 DMA 已完成转换数量 */
    uint16_t (*dma_get_count)(void *user_data);
    /** 毫秒延时 */
    void (*delay_ms)(uint32_t ms);
    /** 用户数据 */
    void *user_data;
} adc_hal_t;

/* ============================================================
 * 2. ADC 设备句柄
 * ============================================================ */

typedef struct {
    adc_hal_t       *hal;
    adc_resolution_t resolution;
    float            vref;          /* 参考电压 (V) */
    uint16_t         max_value;     /* 最大转换值 (2^resolution - 1) */
    bool             initialized;
} adc_device_t;

/* ============================================================
 * 3. 初始化
 * ============================================================ */

adc_status_t adc_init(adc_device_t *dev, adc_hal_t *hal,
                      adc_resolution_t resolution, float vref)
{
    if (!dev || !hal || vref <= 0)
        return ADC_ERR_PARAM;

    dev->hal         = hal;
    dev->resolution  = resolution;
    dev->vref        = vref;
    dev->max_value   = (uint16_t)((1U << resolution) - 1);
    dev->initialized = false;

    adc_status_t st = hal->init(hal->user_data, resolution);
    if (st != ADC_OK)
        return st;

    dev->initialized = true;
    return ADC_OK;
}

void adc_deinit(adc_device_t *dev)
{
    if (!dev || !dev->hal)
        return;
    dev->hal->deinit(dev->hal->user_data);
    dev->initialized = false;
}

/* ============================================================
 * 4. 单次采样模板
 * ============================================================ */

/**
 * 单次采样
 * @param dev      ADC 设备
 * @param channel  通道号
 * @param value    原始 ADC 值
 * @return ADC_OK / 错误码
 */
adc_status_t adc_sample_single(adc_device_t *dev, uint8_t channel,
                                uint16_t *value)
{
    if (!dev || !dev->hal || !value)
        return ADC_ERR_PARAM;
    return dev->hal->read_channel(dev->hal->user_data, channel, value, 100);
}

/* ============================================================
 * 5. 多次采样平均滤波
 * ============================================================ */

/**
 * 多次采样取平均 (最简单的滤波)
 * @param dev      ADC 设备
 * @param channel  通道号
 * @param samples  采样次数 (建议 8/16/32)
 * @param result   平均后的原始值
 */
adc_status_t adc_sample_averaged(adc_device_t *dev, uint8_t channel,
                                  uint8_t samples, uint16_t *result)
{
    if (!dev || !dev->hal || !result || samples == 0)
        return ADC_ERR_PARAM;

    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        uint16_t val;
        adc_status_t st = dev->hal->read_channel(dev->hal->user_data,
                                                  channel, &val, 100);
        if (st != ADC_OK)
            return st;
        sum += val;
    }
    *result = (uint16_t)(sum / samples);
    return ADC_OK;
}

/**
 * 滑动平均滤波 (适用于 DMA 连续采集场景)
 * 维护一个窗口, 去除最大最小值后取平均
 */
#define MEDIAN_FILTER_SIZE  16

adc_status_t adc_sample_median_avg(adc_device_t *dev, uint8_t channel,
                                    uint16_t *result)
{
    if (!dev || !dev->hal || !result)
        return ADC_ERR_PARAM;

    uint16_t buf[MEDIAN_FILTER_SIZE];
    for (int i = 0; i < MEDIAN_FILTER_SIZE; i++) {
        adc_status_t st = dev->hal->read_channel(dev->hal->user_data,
                                                  channel, &buf[i], 100);
        if (st != ADC_OK)
            return st;
    }

    /* 冒泡排序 (样本量小, 可接受) */
    for (int i = 0; i < MEDIAN_FILTER_SIZE - 1; i++)
        for (int j = 0; j < MEDIAN_FILTER_SIZE - 1 - i; j++)
            if (buf[j] > buf[j + 1]) {
                uint16_t tmp = buf[j];
                buf[j] = buf[j + 1];
                buf[j + 1] = tmp;
            }

    /* 去掉最大最小各 2 个, 取中间平均 */
    uint32_t sum = 0;
    for (int i = 2; i < MEDIAN_FILTER_SIZE - 2; i++)
        sum += buf[i];
    *result = (uint16_t)(sum / (MEDIAN_FILTER_SIZE - 4));
    return ADC_OK;
}

/* ============================================================
 * 6. DMA 连续采集模板
 * ============================================================ */

/**
 * 启动 DMA 连续采集
 * @param dev      ADC 设备
 * @param channel  通道号
 * @param buf      DMA 缓冲区 (必须为静态/全局变量, 非栈上)
 * @param len      缓冲区长度
 *
 * DMA 连续采集适用于:
 *   - 高频信号采样 (如音频、振动)
 *   - 多通道扫描采集
 *   - 需要无 CPU 干预的连续数据流
 *
 * 使用流程:
 *   1. adc_start_dma() 启动
 *   2. 定期检查 dma_get_count() 获取已采集数量
 *   3. 从 buf 中读取数据
 *   4. adc_stop_dma() 停止
 */
adc_status_t adc_start_dma(adc_device_t *dev, uint8_t channel,
                           uint16_t *buf, uint16_t len)
{
    if (!dev || !dev->hal || !buf || len == 0)
        return ADC_ERR_PARAM;
    return dev->hal->start_dma(dev->hal->user_data, channel, buf, len);
}

adc_status_t adc_stop_dma(adc_device_t *dev)
{
    if (!dev || !dev->hal)
        return ADC_ERR_PARAM;
    return dev->hal->stop_dma(dev->hal->user_data);
}

uint16_t adc_dma_get_count(adc_device_t *dev)
{
    if (!dev || !dev->hal)
        return 0;
    return dev->hal->dma_get_count(dev->hal->user_data);
}

/* ============================================================
 * 7. 电压转换公式
 * ============================================================ */

/**
 * 原始 ADC 值转电压
 * @param dev    ADC 设备
 * @param raw    原始值
 * @return 电压 (V)
 */
float adc_to_voltage(adc_device_t *dev, uint16_t raw)
{
    if (!dev || dev->max_value == 0)
        return 0.0f;
    return (float)raw * dev->vref / (float)dev->max_value;
}

/**
 * 分压电路电压转换
 * 适用于: V_meas = V_adc * (R1 + R2) / R2
 * @param dev       ADC 设备
 * @param raw       原始值
 * @param r_upper   上分压电阻 (欧姆)
 * @param r_lower   下分压电阻 (欧姆)
 * @return 实际被测电压 (V)
 */
float adc_to_voltage_divider(adc_device_t *dev, uint16_t raw,
                              float r_upper, float r_lower)
{
    float v_adc = adc_to_voltage(dev, raw);
    return v_adc * (r_upper + r_lower) / r_lower;
}

/**
 * NTC 热敏电阻温度转换 (B 值法)
 * @param raw       原始 ADC 值
 * @param r_ref     参考电阻 (欧姆)
 * @param r_nominal 标称电阻 (欧姆, 通常 25C 时的阻值)
 * @param b_value   B 值 (如 3950)
 * @return 温度 (摄氏度)
 *
 * 电路: VCC -- R_ref -- ADC -- NTC -- GND
 */
float adc_ntc_to_temperature(adc_device_t *dev, uint16_t raw,
                              float r_ref, float r_nominal, float b_value)
{
    if (raw == 0)
        return -999.0f;

    float v_adc = adc_to_voltage(dev, raw);
    /* R_ntc = R_ref * V_adc / (VCC - V_adc) */
    float r_ntc = r_ref * v_adc / (dev->vref - v_adc);

    /* B 值法: 1/T = 1/T0 + (1/B) * ln(R/R0) */
    const float T0 = 298.15f;  /* 25C in Kelvin */
    float t_kelvin = 1.0f / (1.0f / T0 + (1.0f / b_value)
                        * logf(r_ntc / r_nominal));
    return t_kelvin - 273.15f;
}

/* ============================================================
 * 注意事项
 * ============================================================
 * 1. 参考电压: 必须稳定, 建议使用独立 LDO 供电的 VREF,
 *    不要直接用噪声大的开关电源。内部参考电压精度有限 (±1%),
 *    高精度场景需外部精密基准 (如 TL431)。
 * 2. 采样时间: 输入阻抗高时需延长采样时间, 否则采样电容充不满,
 *    读数偏小。规则: 采样时间 > (R_src + R_adc) * C_adc * 7.6
 *    典型: R_src=10k, C_adc=5pF -> 采样时间 > 0.4us
 * 3. 输入阻抗: MCU ADC 输入阻抗通常 50k~100k,
 *    信号源内阻 > 10k 时建议加运放跟随器。
 * 4. 抗混叠: ADC 输入前加 RC 低通滤波 (R=1k, C=10nF -> fc=16kHz),
 *    截止频率 < 采样率/2 (奈奎斯特准则)。
 * 5. DMA 双缓冲: 连续采集建议使用双缓冲 (Half/Full 中断),
 *    避免数据被覆盖。STM32 使用 HAL_ADC_ConvHalfCpltCallback。
 * 6. 自热效应: NTC/PT100 等传感器通过电流会产生自热,
 *    保持电流 < 0.1mA, 采样间隔不要太短。
 * 7. 校准: 出厂前用已知电压源校准 ADC 增益和偏移,
 *    存储校准系数到 EEPROM/Flash。
 */
