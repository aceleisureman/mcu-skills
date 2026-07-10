/**
 * @file    driver-template.h
 * @brief   元器件驱动头文件模板 (跨平台)
 *
 * 驱动对外接口的标准形态。使用方法:
 *   1. 全局替换 xxx 为器件名 (如 sht30), XXX 为大写
 *   2. 按器件裁剪 config / data 结构与 API
 *   3. 实现文件参考 templates/driver-template-<接口>.c
 *
 * 设计约定:
 *   - 所有 API 返回统一错误码, 禁止返回裸 bool/int
 *   - 句柄由调用方分配, 驱动不做动态内存分配
 *   - 硬件访问全部通过注入的 HAL 接口, 驱动本体不含平台头文件
 */

#ifndef XXX_DRIVER_H
#define XXX_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 错误码 (统一前缀)
 * ============================================================ */
typedef enum {
    XXX_OK = 0,
    XXX_ERR_INVALID_PARAM,
    XXX_ERR_NOT_INIT,
    XXX_ERR_TIMEOUT,
    XXX_ERR_BUS,            /* 底层总线错误 */
    XXX_ERR_CRC,
    XXX_ERR_NOT_FOUND,      /* 器件不在线 */
} xxx_status_t;

/* ============================================================
 * HAL 注入接口 (由平台适配层实现)
 * ============================================================ */
typedef struct {
    /* 按器件接口类型保留其一, 其余删除; 签名与对应 .c 模板一致 */
    void *bus;                              /* i2c_device_t* / spi_device_t* 等 */
    void (*delay_ms)(uint32_t ms);
    uint32_t (*get_tick_ms)(void);          /* 超时判断用 */
} xxx_hal_t;

/* ============================================================
 * 配置与句柄
 * ============================================================ */
typedef struct {
    uint8_t  addr;          /* I2C 地址等器件参数 */
    /* ... 按器件补充: 量程/采样率/工作模式 ... */
} xxx_config_t;

typedef struct {
    xxx_hal_t    hal;
    xxx_config_t config;
    bool         initialized;
    /* ... 运行时状态: 校准系数/缓存 ... */
} xxx_handle_t;

/* 测量数据 (按器件定义) */
typedef struct {
    float   value;
    int64_t timestamp_ms;
    bool    valid;
} xxx_data_t;

/* ============================================================
 * API (最小集: init / read / deinit; 按需扩展)
 * ============================================================ */

/** 初始化器件: 探测在线 → 复位 → 加载配置/校准 */
xxx_status_t xxx_init(xxx_handle_t *h, const xxx_hal_t *hal, const xxx_config_t *cfg);

/** 读取一次数据 (阻塞, 内部含校验) */
xxx_status_t xxx_read(xxx_handle_t *h, xxx_data_t *out);

/** 进入低功耗 / 唤醒 */
xxx_status_t xxx_sleep(xxx_handle_t *h);
xxx_status_t xxx_wakeup(xxx_handle_t *h);

/** 反初始化 */
void xxx_deinit(xxx_handle_t *h);

#ifdef __cplusplus
}
#endif

#endif /* XXX_DRIVER_H */
