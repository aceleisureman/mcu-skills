/**
 * @file    driver-template-uart.c
 * @brief   UART 驱动模板 (跨平台)
 *
 * 本模板提供 UART 驱动的标准化框架，包含：
 *   - UART HAL 抽象层定义 (平台无关接口)
 *   - 发送 / 接收函数原型
 *   - 环形缓冲区实现
 *   - AT 指令解析框架 (状态机)
 *   - 超时处理
 *
 * ## 使用方法
 *   1. 实现 uart_hal_t 中的平台适配函数
 *   2. 用 uart_init() 初始化, 指定波特率/数据位/校验/停止位
 *   3. 注册 RX 回调或使用环形缓冲区接收数据
 *   4. 调用 uart_send() 发送数据
 *   5. AT 指令场景使用 at_command_send() 同步发送并等待响应
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* ============================================================
 * 1. UART HAL 抽象层定义
 * ============================================================ */

typedef enum {
    UART_OK = 0,
    UART_ERR_TIMEOUT,
    UART_ERR_BUSY,
    UART_ERR_PARAM,
    UART_ERR_OVERFLOW,
    UART_ERR_NO_DATA,
} uart_status_t;

typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_EVEN,
    UART_PARITY_ODD,
} uart_parity_t;

typedef enum {
    UART_STOP_1 = 0,
    UART_STOP_1_5,
    UART_STOP_2,
} uart_stop_bits_t;

typedef enum {
    UART_DATA_7 = 7,
    UART_DATA_8 = 8,
    UART_DATA_9 = 9,
} uart_data_bits_t;

/**
 * UART HAL 接口
 */
typedef struct {
    /** 初始化 UART */
    uart_status_t (*init)(void *user_data, uint32_t baudrate,
                          uart_data_bits_t data_bits, uart_parity_t parity,
                          uart_stop_bits_t stop_bits);
    /** 反初始化 */
    uart_status_t (*deinit)(void *user_data);
    /** 发送数据 (阻塞) */
    uart_status_t (*send)(void *user_data, const uint8_t *data, uint16_t len,
                          uint32_t timeout_ms);
    /** 发送数据 (中断/DMA异步) */
    uart_status_t (*send_async)(void *user_data, const uint8_t *data, uint16_t len);
    /** 等待异步发送完成 */
    uart_status_t (*send_wait)(void *user_data, uint32_t timeout_ms);
    /** 读取接收数据 (从硬件 FIFO) */
    int (*recv_byte)(void *user_data, uint8_t *byte, uint32_t timeout_ms);
    /** 开启接收中断 */
    uart_status_t (*start_rx_it)(void *user_data);
    /** 关闭接收中断 */
    uart_status_t (*stop_rx_it)(void *user_data);
    /** 毫秒延时 */
    void (*delay_ms)(uint32_t ms);
    /** 获取系统毫秒时间戳 */
    uint32_t (*get_tick_ms)(void);
    /** 用户数据 */
    void *user_data;
} uart_hal_t;

/* ============================================================
 * 2. 环形缓冲区实现
 * ============================================================ */

#define UART_RINGBUF_SIZE  256   /* 必须为 2 的幂 */

typedef struct {
    uint8_t  buffer[UART_RINGBUF_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} ringbuf_t;

static inline void ringbuf_init(ringbuf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

static inline bool ringbuf_is_empty(const ringbuf_t *rb)
{
    return rb->head == rb->tail;
}

static inline bool ringbuf_is_full(const ringbuf_t *rb)
{
    return ((rb->head + 1) & (UART_RINGBUF_SIZE - 1)) == rb->tail;
}

/** 写入一个字节 (在中断中调用) */
static inline bool ringbuf_push(ringbuf_t *rb, uint8_t byte)
{
    uint16_t next = (rb->head + 1) & (UART_RINGBUF_SIZE - 1);
    if (next == rb->tail)
        return false;  /* 满 */
    rb->buffer[rb->head] = byte;
    rb->head = next;
    return true;
}

/** 读取一个字节 */
static inline bool ringbuf_pop(ringbuf_t *rb, uint8_t *byte)
{
    if (rb->head == rb->tail)
        return false;  /* 空 */
    *byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) & (UART_RINGBUF_SIZE - 1);
    return true;
}

/** 可读数据长度 */
static inline uint16_t ringbuf_available(const ringbuf_t *rb)
{
    return (uint16_t)((rb->head - rb->tail) & (UART_RINGBUF_SIZE - 1));
}

/* ============================================================
 * 3. UART 设备句柄
 * ============================================================ */

#define UART_DEFAULT_TIMEOUT_MS  1000

typedef struct {
    uart_hal_t  *hal;
    uint32_t     baudrate;
    ringbuf_t    rx_ringbuf;
    bool         initialized;
} uart_device_t;

/**
 * UART RX 中断回调 (在 MCU UART RX ISR 中调用)
 * 将接收到的字节推入环形缓冲区
 */
void uart_rx_isr_callback(uart_device_t *dev, uint8_t byte)
{
    if (!dev)
        return;
    ringbuf_push(&dev->rx_ringbuf, byte);
}

/* ============================================================
 * 4. 初始化 / 发送 / 接收
 * ============================================================ */

uart_status_t uart_init(uart_device_t *dev, uart_hal_t *hal,
                         uint32_t baudrate)
{
    if (!dev || !hal)
        return UART_ERR_PARAM;

    dev->hal      = hal;
    dev->baudrate = baudrate;
    ringbuf_init(&dev->rx_ringbuf);
    dev->initialized = false;

    uart_status_t st = hal->init(hal->user_data, baudrate,
                                  UART_DATA_8, UART_PARITY_NONE, UART_STOP_1);
    if (st != UART_OK)
        return st;

    /* 开启接收中断 */
    st = hal->start_rx_it(hal->user_data);
    if (st != UART_OK)
        return st;

    dev->initialized = true;
    return UART_OK;
}

void uart_deinit(uart_device_t *dev)
{
    if (!dev || !dev->hal)
        return;
    dev->hal->stop_rx_it(dev->hal->user_data);
    dev->hal->deinit(dev->hal->user_data);
    dev->initialized = false;
}

/** 发送数据 (阻塞) */
uart_status_t uart_send(uart_device_t *dev, const uint8_t *data, uint16_t len)
{
    if (!dev || !dev->hal || !data || len == 0)
        return UART_ERR_PARAM;
    return dev->hal->send(dev->hal->user_data, data, len, UART_DEFAULT_TIMEOUT_MS);
}

/** 发送字符串 */
uart_status_t uart_send_string(uart_device_t *dev, const char *str)
{
    return uart_send(dev, (const uint8_t *)str, (uint16_t)strlen(str));
}

/**
 * 读取接收数据 (从环形缓冲区)
 * @return 实际读取的字节数
 */
uint16_t uart_recv(uart_device_t *dev, uint8_t *buf, uint16_t max_len,
                    uint32_t timeout_ms)
{
    if (!dev || !buf || max_len == 0)
        return 0;

    uint32_t start = dev->hal->get_tick_ms();
    uint16_t count = 0;

    while (count < max_len) {
        if (!ringbuf_pop(&dev->rx_ringbuf, &buf[count])) {
            if (dev->hal->get_tick_ms() - start >= timeout_ms)
                break;
            continue;
        }
        count++;
    }
    return count;
}

/** 检查是否有可读数据 */
bool uart_has_data(uart_device_t *dev)
{
    return !ringbuf_is_empty(&dev->rx_ringbuf);
}

/* ============================================================
 * 5. AT 指令解析框架 (状态机)
 * ============================================================ */

#define AT_RESP_MAX_LEN   512
#define AT_CMD_MAX_LEN    128
#define AT_LINE_END       "\r\n"

typedef enum {
    AT_STATE_IDLE = 0,
    AT_STATE_WAITING_RESP,
    AT_STATE_DONE_OK,
    AT_STATE_DONE_ERROR,
    AT_STATE_TIMEOUT,
} at_state_t;

typedef struct {
    uart_device_t *uart;
    char    resp_buf[AT_RESP_MAX_LEN];
    uint16_t resp_len;
    at_state_t state;
    uint32_t start_tick;
    uint32_t timeout_ms;
} at_engine_t;

/**
 * 发送 AT 指令并等待响应
 * @param engine   AT 引擎
 * @param cmd      AT 指令 (如 "AT", "AT+CWJAP=\"ssid\",\"pass\"")
 * @param resp     响应缓冲区
 * @param resp_max 响应缓冲区大小
 * @param timeout_ms 超时
 * @return UART_OK / UART_ERR_TIMEOUT
 *
 * 响应中查找 "OK\r\n" 或 "ERROR\r\n" 判定结果
 */
uart_status_t at_command_send(at_engine_t *engine, const char *cmd,
                               char *resp, uint16_t resp_max,
                               uint32_t timeout_ms)
{
    if (!engine || !cmd)
        return UART_ERR_PARAM;

    /* 清空接收缓冲区 */
    ringbuf_init(&engine->uart->rx_ringbuf);

    /* 发送指令 (附带 \r\n) */
    char tx_buf[AT_CMD_MAX_LEN];
    int n = snprintf(tx_buf, sizeof(tx_buf), "%s%s", cmd, AT_LINE_END);
    uart_status_t st = uart_send(engine->uart, (uint8_t *)tx_buf, (uint16_t)n);
    if (st != UART_OK)
        return st;

    /* 等待响应 */
    engine->resp_len = 0;
    engine->timeout_ms = timeout_ms;
    engine->start_tick = engine->uart->hal->get_tick_ms();
    engine->state = AT_STATE_WAITING_RESP;

    while (engine->state == AT_STATE_WAITING_RESP) {
        uint8_t byte;
        if (ringbuf_pop(&engine->uart->rx_ringbuf, &byte)) {
            if (engine->resp_len < AT_RESP_MAX_LEN - 1)
                engine->resp_buf[engine->resp_len++] = (char)byte;
            engine->resp_buf[engine->resp_len] = '\0';

            /* 检查响应结束标志 */
            if (engine->resp_len >= 4) {
                if (strstr(engine->resp_buf, "\r\nOK\r\n"))
                    engine->state = AT_STATE_DONE_OK;
                else if (strstr(engine->resp_buf, "\r\nERROR\r\n"))
                    engine->state = AT_STATE_DONE_ERROR;
            }
        }

        /* 超时检查 */
        if (engine->uart->hal->get_tick_ms() - engine->start_tick >= timeout_ms) {
            engine->state = AT_STATE_TIMEOUT;
            break;
        }
    }

    /* 输出响应 */
    if (resp && resp_max > 0) {
        uint16_t copy_len = engine->resp_len < resp_max - 1
                            ? engine->resp_len : (uint16_t)(resp_max - 1);
        memcpy(resp, engine->resp_buf, copy_len);
        resp[copy_len] = '\0';
    }

    return (engine->state == AT_STATE_DONE_OK) ? UART_OK
           : (engine->state == AT_STATE_TIMEOUT) ? UART_ERR_TIMEOUT
           : UART_ERR_BUSY;
}

/**
 * 等待非请求响应 (URC, 如 "+IPD" 接收数据, "WIFI CONNECTED" 等)
 * @param buf    缓冲区
 * @param max    缓冲区大小
 * @param timeout_ms 超时
 * @return 实际接收长度, 0 表示超时
 */
uint16_t at_wait_urc(at_engine_t *engine, char *buf, uint16_t max,
                      uint32_t timeout_ms)
{
    if (!engine || !buf || max == 0)
        return 0;

    uint32_t start = engine->uart->hal->get_tick_ms();
    uint16_t len = 0;

    while (engine->uart->hal->get_tick_ms() - start < timeout_ms) {
        uint8_t byte;
        if (ringbuf_pop(&engine->uart->rx_ringbuf, &byte)) {
            if (len < max - 1)
                buf[len++] = (char)byte;
            /* 行结束检测 */
            if (len >= 2 && buf[len-2] == '\r' && buf[len-1] == '\n')
                break;
        }
    }
    buf[len] = '\0';
    return len;
}

/* ============================================================
 * 6. 超时处理辅助
 * ============================================================ */

/**
 * 带超时的等待特定字符串
 * @return true 匹配成功, false 超时
 */
bool uart_wait_for(uart_device_t *dev, const char *pattern,
                    uint32_t timeout_ms)
{
    if (!dev || !pattern)
        return false;

    uint32_t start = dev->hal->get_tick_ms();
    uint16_t plen = (uint16_t)strlen(pattern);
    char match_buf[64];

    if (plen >= sizeof(match_buf))
        return false;

    uint16_t idx = 0;

    while (dev->hal->get_tick_ms() - start < timeout_ms) {
        uint8_t byte;
        if (ringbuf_pop(&dev->rx_ringbuf, &byte)) {
            match_buf[idx++] = (char)byte;
            if (idx > plen)
                memmove(match_buf, match_buf + 1, idx - 1);
            if (idx >= plen) {
                match_buf[plen] = '\0';
                if (strncmp(match_buf, pattern, plen) == 0)
                    return true;
            }
        }
    }
    return false;
}

/* ============================================================
 * 注意事项
 * ============================================================
 * 1. 波特率误差: MCU 时钟精度不够时, 高波特率 (>115200) 容易出现误码,
 *    误差应 < 2.5%, 使用高精度晶振或内部时钟校准。
 * 2. 环形缓冲区: 大小必须为 2 的幂以使用位运算取模, 提高效率。
 *    高波特率场景需增大缓冲区 (如 4096)。
 * 3. DMA 接收: 建议使用 DMA + 空闲中断 (IDLE) 实现不定长接收,
 *    比逐字节中断效率更高。
 * 4. AT 指令: 不同模块 (ESP8266/BC95/SIM800) 的 AT 响应格式略有差异,
 *    需按具体模块调整匹配逻辑 (如 "+OK" vs "OK")。
 * 5. 线程安全: 环形缓冲区的 head/tail 在中断和主循环中交叉访问,
 *    使用 volatile 修饰; 若多核 (ESP32) 需加自旋锁。
 * 6. 流控: 高波特率大数据量建议启用硬件流控 (RTS/CTS),
 *    避免缓冲区溢出丢数据。
 * 7. printf 重定向: 调试阶段可重定向 printf 到 UART,
 *    注意 printf 可能在中断中不可重入。
 * 8. 错误恢复 (实际项目验证): UART 溢出错误 (ORE) 会导致接收停止,
 *    必须在 ErrorCallback 中清除 ORE 标志后重新启用接收:
 *      void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
 *          __HAL_UART_CLEAR_OREFLAG(huart);
 *          HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
 *      }
 * 9. 自适应波特率 (实际项目验证): 通信模块默认波特率可能不确定,
 *    遍历常见波特率 [115200, 9600, 74880, 57600, 38400] 发送 AT 确认:
 *      for each baud: 设置波特率 → 发送 "AT" → 等待 "OK" → 命中则使用该波特率。
 * 10. wait_token 流式匹配 (实际项目验证): 不要等全部数据接收完再匹配,
 *     逐字节流式匹配 token, 匹配成功立即返回, 减少等待时间:
 *      if (c == token[match]) { if (++match == len) return 1; }
 *      else { match = (c == token[0]) ? 1 : 0; }
 * 11. 二进制安全匹配: AT+CIPSEND 透传时 MQTT CONNACK 含 0x00 字节,
 *     strstr 会被 0x00 截断, 必须用 memcmp 的二进制安全查找。
 */
