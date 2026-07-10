/**
 * @file    driver-template-pwm.c
 * @brief   PWM 驱动模板 (跨平台)
 *
 * 本模板提供 PWM 驱动的标准化框架，包含：
 *   - PWM HAL 抽象层定义 (平台无关接口)
 *   - 频率 / 占空比设置
 *   - 多通道 PWM 同步
 *   - 呼吸灯效果示例
 *   - 舵机控制 PWM 示例 (50Hz)
 *
 * ## 使用方法
 *   1. 实现 pwm_hal_t 中的平台适配函数
 *   2. 用 pwm_init() 初始化, 指定频率和分辨率
 *   3. pwm_set_duty() 设置占空比
 *   4. 呼吸灯: breathing_led_task()
 *   5. 舵机: servo_set_angle()
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================
 * 1. PWM HAL 抽象层定义
 * ============================================================ */

typedef enum {
    PWM_OK = 0,
    PWM_ERR_PARAM,
    PWM_ERR_TIMEOUT,
    PWM_ERR_UNSUPPORTED,
} pwm_status_t;

/**
 * PWM HAL 接口
 */
typedef struct {
    /** 初始化 PWM, 配置频率 */
    pwm_status_t (*init)(void *user_data, uint32_t frequency_hz);
    /** 反初始化 */
    pwm_status_t (*deinit)(void *user_data);
    /** 启动指定通道 */
    pwm_status_t (*start)(void *user_data, uint8_t channel);
    /** 停止指定通道 */
    pwm_status_t (*stop)(void *user_data, uint8_t channel);
    /**
     * 设置占空比
     * @param channel  通道号
     * @param duty     占空比, 0~max_duty
     */
    pwm_status_t (*set_duty)(void *user_data, uint8_t channel, uint32_t duty);
    /**
     * 设置频率 (运行时修改)
     * @param frequency_hz 新频率
     */
    pwm_status_t (*set_frequency)(void *user_data, uint32_t frequency_hz);
    /** 获取最大占空比值 (分辨率) */
    uint32_t (*get_max_duty)(void *user_data);
    /** 毫秒延时 */
    void (*delay_ms)(uint32_t ms);
    /** 获取系统毫秒时间戳 */
    uint32_t (*get_tick_ms)(void);
    /** 用户数据 */
    void *user_data;
} pwm_hal_t;

/* ============================================================
 * 2. PWM 设备句柄
 * ============================================================ */

typedef struct {
    pwm_hal_t   *hal;
    uint32_t     frequency_hz;
    uint32_t     max_duty;       /* 最大占空比 (分辨率) */
    bool         initialized;
} pwm_device_t;

/* ============================================================
 * 3. 初始化与基础操作
 * ============================================================ */

pwm_status_t pwm_init(pwm_device_t *dev, pwm_hal_t *hal,
                      uint32_t frequency_hz)
{
    if (!dev || !hal || frequency_hz == 0)
        return PWM_ERR_PARAM;

    dev->hal          = hal;
    dev->frequency_hz = frequency_hz;

    pwm_status_t st = hal->init(hal->user_data, frequency_hz);
    if (st != PWM_OK)
        return st;

    dev->max_duty    = hal->get_max_duty(hal->user_data);
    dev->initialized = true;
    return PWM_OK;
}

void pwm_deinit(pwm_device_t *dev)
{
    if (!dev || !dev->hal)
        return;
    dev->hal->deinit(dev->hal->user_data);
    dev->initialized = false;
}

/**
 * 设置占空比 (百分比)
 * @param percent  0~100
 */
pwm_status_t pwm_set_duty_percent(pwm_device_t *dev, uint8_t channel,
                                   float percent)
{
    if (!dev || !dev->hal)
        return PWM_ERR_PARAM;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    uint32_t duty = (uint32_t)(percent * dev->max_duty / 100.0f);
    return dev->hal->set_duty(dev->hal->user_data, channel, duty);
}

/**
 * 设置占空比 (原始值)
 */
pwm_status_t pwm_set_duty(pwm_device_t *dev, uint8_t channel, uint32_t duty)
{
    if (!dev || !dev->hal)
        return PWM_ERR_PARAM;
    if (duty > dev->max_duty)
        duty = dev->max_duty;
    return dev->hal->set_duty(dev->hal->user_data, channel, duty);
}

/**
 * 运行时修改频率
 */
pwm_status_t pwm_set_frequency(pwm_device_t *dev, uint32_t frequency_hz)
{
    if (!dev || !dev->hal || frequency_hz == 0)
        return PWM_ERR_PARAM;
    pwm_status_t st = dev->hal->set_frequency(dev->hal->user_data, frequency_hz);
    if (st == PWM_OK)
        dev->frequency_hz = frequency_hz;
    return st;
}

/* ============================================================
 * 4. 多通道 PWM 同步
 * ============================================================ */

/**
 * 多通道同步设置 (如三相无刷电机驱动)
 * @param channels   通道数组
 * @param duties     对应占空比数组
 * @param count      通道数
 *
 * 注意: 真正的硬件同步更新需要在所有通道配置完成后统一使能,
 *       部分平台 (STM32) 需使用 preload + CCR update 事件。
 */
pwm_status_t pwm_set_duty_multi(pwm_device_t *dev,
                                 const uint8_t *channels,
                                 const uint32_t *duties, uint8_t count)
{
    if (!dev || !dev->hal || !channels || !duties || count == 0)
        return PWM_ERR_PARAM;

    for (uint8_t i = 0; i < count; i++) {
        uint32_t d = duties[i] > dev->max_duty ? dev->max_duty : duties[i];
        pwm_status_t st = dev->hal->set_duty(dev->hal->user_data,
                                              channels[i], d);
        if (st != PWM_OK)
            return st;
    }
    return PWM_OK;
}

/* ============================================================
 * 5. 呼吸灯效果示例
 * ============================================================ */

/**
 * 呼吸灯单步更新 (在定时器/主循环中周期调用, 建议 10ms 间隔)
 * @param dev       PWM 设备
 * @param channel   PWM 通道
 * @param step      当前亮度步进指针 (函数内部更新)
 * @param direction 方向指针: 1=渐亮, -1=渐暗 (函数内部更新)
 * @param step_size 每步变化量 (建议 1~5, 越大越快)
 */
void breathing_led_step(pwm_device_t *dev, uint8_t channel,
                         int16_t *step, int8_t *direction,
                         uint16_t step_size)
{
    if (!dev || !step || !direction)
        return;

    *step += (*direction) * step_size;

    /* 边界翻转 */
    if (*step >= (int16_t)dev->max_duty) {
        *step = (int16_t)dev->max_duty;
        *direction = -1;
    } else if (*step <= 0) {
        *step = 0;
        *direction = 1;
    }

    pwm_set_duty(dev, channel, (uint32_t)*step);
}

/**
 * 呼吸灯完整周期 (阻塞, 适用于简单演示)
 * @param dev       PWM 设备
 * @param channel   PWM 通道
 * @param period_ms 一个完整呼吸周期 (毫秒)
 */
void breathing_led_cycle(pwm_device_t *dev, uint8_t channel,
                          uint32_t period_ms)
{
    if (!dev || !dev->hal)
        return;

    uint32_t steps = 200;
    uint32_t delay = period_ms / (steps * 2);
    if (delay == 0) delay = 1;

    /* 渐亮 */
    for (uint32_t i = 0; i <= steps; i++) {
        uint32_t duty = dev->max_duty * i / steps;
        pwm_set_duty(dev, channel, duty);
        dev->hal->delay_ms(delay);
    }
    /* 渐暗 */
    for (uint32_t i = steps; i > 0; i--) {
        uint32_t duty = dev->max_duty * i / steps;
        pwm_set_duty(dev, channel, duty);
        dev->hal->delay_ms(delay);
    }
}

/* ============================================================
 * 6. 舵机控制 PWM 示例 (50Hz, 周期 20ms)
 * ============================================================ */

/*
 * 标准舵机 (SG90, MG996R 等) PWM 参数:
 *   频率: 50 Hz (周期 20ms)
 *   正脉冲宽度: 0.5ms ~ 2.5ms (对应 0~180 度)
 *   0度:   0.5ms  -> 占空比 2.5%
 *   90度:  1.5ms  -> 占空比 7.5%
 *   180度: 2.5ms  -> 占空比 12.5%
 */

#define SERVO_FREQ_HZ       50
#define SERVO_MIN_PULSE_US  500     /* 0 度对应脉宽 */
#define SERVO_MAX_PULSE_US  2500    /* 180 度对应脉宽 */
#define SERVO_PERIOD_US     20000   /* 50Hz -> 20ms = 20000us */

/**
 * 设置舵机角度
 * @param dev     PWM 设备 (需已初始化为 50Hz)
 * @param channel PWM 通道
 * @param angle   角度 (0~180)
 */
pwm_status_t servo_set_angle(pwm_device_t *dev, uint8_t channel,
                              float angle)
{
    if (!dev || !dev->hal)
        return PWM_ERR_PARAM;
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    /* 角度 -> 脉宽 (us) */
    uint32_t pulse_us = SERVO_MIN_PULSE_US
                      + (uint32_t)(angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) / 180.0f);

    /* 脉宽 -> 占空比原始值 */
    uint32_t duty = dev->max_duty * pulse_us / SERVO_PERIOD_US;

    return dev->hal->set_duty(dev->hal->user_data, channel, duty);
}

/**
 * 初始化舵机 PWM (50Hz)
 */
pwm_status_t servo_init(pwm_device_t *dev, pwm_hal_t *hal)
{
    return pwm_init(dev, hal, SERVO_FREQ_HZ);
}

/* ============================================================
 * 注意事项
 * ============================================================
 * 1. 频率与分辨率: PWM 频率和分辨率互相制约,
 *    freq = timer_clk / (prescaler * period), period 越大分辨率越高但频率越低。
 *    例如 STM32 72MHz: prescaler=71, period=19999 -> 50Hz, 分辨率 20000。
 * 2. 死区时间: 互补 PWM (如电机驱动 H 桥) 必须设置死区时间,
 *    防止上下桥臂同时导通短路, 典型 500ns~1us。
 * 3. 舵机供电: 舵机瞬时电流可达 500mA~2A, 不可从 MCU 3.3V 供电,
 *    需独立 5V/6V 电源 + 大电容滤波, 共地连接。
 * 4. PWM 与 LED: LED 调光频率建议 > 200Hz (避免可见闪烁),
 *    但太高 (>20kHz) 会导致分辨率不足, 建议 1kHz~10kHz。
 * 5. 无源蜂鸣器: 频率决定音高, 占空比固定 50%,
 *    中音 A4 = 440Hz, C5 = 523Hz。
 * 6. 电机调速: 直流电机 PWM 频率建议 10kHz~20kHz (超出人耳听觉),
 *    太低会有可闻噪声, 太高开关损耗增大。
 * 7. 通道同步: 多通道需要同步时 (如 RGB LED、三相电机),
 *    使用同一定时器的不同通道, 并开启 preload 确保同时更新。
 */
