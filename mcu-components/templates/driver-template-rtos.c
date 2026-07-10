/**
 * @file    driver-template-rtos.c
 * @brief   FreeRTOS 环境驱动封装模板
 *
 * 解决多任务环境下驱动使用的三类问题:
 *   1. 总线互斥 —— 多任务共享 I2C/SPI 总线的互斥锁封装
 *   2. 传感器任务 —— 周期采集任务 + 数据快照的标准写法
 *   3. 中断到任务 —— ISR 用信号量/队列通知任务的标准写法
 *
 * ## 使用方法
 *   1. 结合 templates/driver-template-<接口>.c 的裸机驱动使用
 *   2. 所有总线访问经 bus_lock/bus_unlock 包裹
 *   3. 中断服务函数内只做通知, 不做总线操作
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ============================================================
 * 1. 总线互斥封装
 * ============================================================ */

typedef struct {
    SemaphoreHandle_t mutex;   /* 递归锁, 支持驱动内部嵌套调用 */
    uint32_t          timeout_ticks;
} bus_lock_t;

bool bus_lock_init(bus_lock_t *l, uint32_t timeout_ms) {
    l->mutex = xSemaphoreCreateRecursiveMutex();
    l->timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    return l->mutex != NULL;
}

bool bus_lock(bus_lock_t *l)   { return xSemaphoreTakeRecursive(l->mutex, l->timeout_ticks) == pdTRUE; }
void bus_unlock(bus_lock_t *l) { xSemaphoreGiveRecursive(l->mutex); }

/* 用法: 把裸机驱动的 hal->write/read 包一层
static i2c_status_t i2c_write_locked(void *ud, uint8_t addr,
                                     const uint8_t *data, uint16_t len, uint32_t to) {
    if (!bus_lock(&i2c1_lock)) return I2C_ERR_TIMEOUT;
    i2c_status_t st = i2c_write_raw(ud, addr, data, len, to);
    bus_unlock(&i2c1_lock);
    return st;
}
*/

/* ============================================================
 * 2. 周期采集任务 + 数据快照
 * ============================================================ */

typedef struct {
    float    temperature;
    float    humidity;
    uint32_t seq;              /* 序号, 读取方判断是否更新 */
    bool     valid;
} sensor_snapshot_t;

static sensor_snapshot_t s_snapshot;
static SemaphoreHandle_t s_snapshot_mutex;

/* 其它任务安全读取最新数据 (非阻塞语义, 拿不到锁返回 false) */
bool sensor_get_snapshot(sensor_snapshot_t *out) {
    if (xSemaphoreTake(s_snapshot_mutex, pdMS_TO_TICKS(10)) != pdTRUE)
        return false;
    *out = s_snapshot;
    xSemaphoreGive(s_snapshot_mutex);
    return out->valid;
}

/* 周期采集任务: vTaskDelayUntil 保证固定周期不漂移 */
void sensor_task(void *arg) {
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000);

    for (;;) {
        float t, rh;
        bool ok = (sht30_read(&g_sht30, &t, &rh) == 0);   /* 内部已带总线锁 */

        xSemaphoreTake(s_snapshot_mutex, portMAX_DELAY);
        if (ok) { s_snapshot.temperature = t; s_snapshot.humidity = rh; }
        s_snapshot.valid = ok;
        s_snapshot.seq++;
        xSemaphoreGive(s_snapshot_mutex);

        vTaskDelayUntil(&last, period);
    }
}

/* ============================================================
 * 3. 中断 → 任务 通知 (以 GPIO 报警中断为例)
 * ============================================================ */

static TaskHandle_t s_alert_task;

/* ISR: 只通知, 不访问总线! (I2C 读寄存器等放到任务里做) */
void EXTI_ALERT_IRQHandler(void) {
    BaseType_t hpw = pdFALSE;
    exti_clear_flag(ALERT_PIN);
    vTaskNotifyGiveFromISR(s_alert_task, &hpw);
    portYIELD_FROM_ISR(hpw);
}

void alert_task(void *arg) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   /* 等待中断 */
        /* 在任务上下文中安全地读器件状态寄存器 */
        uint8_t status;
        i2c_reg_read(&g_dev, REG_INT_STATUS, &status);
        /* ... 处理报警 ... */
    }
}

/* ============================================================
 * 4. 初始化汇总
 * ============================================================ */

void drivers_rtos_init(void) {
    s_snapshot_mutex = xSemaphoreCreateMutex();
    bus_lock_init(&i2c1_lock, 100);

    xTaskCreate(sensor_task, "sensor", 512, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(alert_task,  "alert",  512, NULL, tskIDLE_PRIORITY + 3, &s_alert_task);
}

/* ============================================================
 * 注意事项
 * ============================================================
 * 1. ISR 中禁止调用非 FromISR API, 禁止做总线读写和 printf。
 * 2. 栈大小: 含 printf/浮点格式化的任务栈 ≥ 1KB, 用
 *    uxTaskGetStackHighWaterMark 实测后收紧。
 * 3. 阻塞型器件 (DS18B20 750ms 转换) 用 vTaskDelay 让出 CPU,
 *    不要用忙等 delay_ms。
 * 4. 互斥锁必须成对释放; 驱动返回错误路径也要 unlock (goto 统一出口)。
 * 5. 优先级翻转: 共享总线的任务用 mutex (自带优先级继承), 不要用二值信号量。
 */
