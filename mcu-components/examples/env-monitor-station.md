# 端到端示例：温湿度环境监测站

综合示例：SHT30 温湿度采集 + SSD1306 OLED 本地显示 + ESP32 WiFi/MQTT 上报，演示多器件驱动如何按本知识库规范组合成完整应用。

**涉及文档**：`references/sensors/temperature.md`（SHT30）、`references/displays/oled.md`（SSD1306）、`references/communication/wifi.md`（MQTT）、`templates/driver-template-i2c.c`、`templates/driver-template-rtos.c`

## 1. 硬件连接

| 器件 | 引脚 | ESP32 | 说明 |
|------|------|-------|------|
| SHT30 | SDA/SCL | GPIO21/GPIO22 | I2C0, 地址 0x44, 4.7kΩ 上拉 |
| SSD1306 | SDA/SCL | GPIO21/GPIO22 | 同总线, 地址 0x3C |
| 状态 LED | — | GPIO2 | 上报成功闪烁 |

两器件共享 I2C 总线 → 多任务访问必须加总线互斥锁（见 driver-template-rtos.c 第 1 节）。

## 2. 软件架构

```
sensor_task (1s)  ──┐                 ┌── display_task (500ms, 读快照刷 OLED)
                    ├─ 数据快照(互斥) ─┤
                    │                 └── mqtt_task (60s, 读快照上报 JSON)
i2c 总线锁 ── sht30 驱动 / ssd1306 驱动 共用
```

## 3. 核心代码

```c
/* main.c —— 基于 ESP-IDF, 驱动接口遵循本库统一规范 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "mqtt_client.h"

static SemaphoreHandle_t s_i2c_lock, s_snap_lock;
static struct { float t, rh; bool valid; } s_snap;

/* --- 采集任务: 周期读 SHT30, 更新快照 --- */
static void sensor_task(void *arg) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        float t, rh;
        xSemaphoreTake(s_i2c_lock, portMAX_DELAY);
        bool ok = (sht30_read_single(&g_sht30, &t, &rh) == SHT30_OK); /* CRC 已校验 */
        xSemaphoreGive(s_i2c_lock);

        xSemaphoreTake(s_snap_lock, portMAX_DELAY);
        if (ok) { s_snap.t = t; s_snap.rh = rh; }
        s_snap.valid = ok;
        xSemaphoreGive(s_snap_lock);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1000));
    }
}

/* --- 显示任务: 读快照刷新 OLED --- */
static void display_task(void *arg) {
    for (;;) {
        char l1[24], l2[24];
        xSemaphoreTake(s_snap_lock, portMAX_DELAY);
        snprintf(l1, sizeof(l1), "T: %5.1f C", s_snap.t);
        snprintf(l2, sizeof(l2), "RH: %4.1f %%%s", s_snap.rh, s_snap.valid ? "" : " !");
        xSemaphoreGive(s_snap_lock);

        xSemaphoreTake(s_i2c_lock, portMAX_DELAY);
        ssd1306_draw_string(&g_oled, 0, 0, l1, FONT_8X16);
        ssd1306_draw_string(&g_oled, 0, 2, l2, FONT_8X16);
        ssd1306_refresh(&g_oled);
        xSemaphoreGive(s_i2c_lock);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* --- MQTT 上报任务: 60s 一次, 失败重连 --- */
static void mqtt_task(void *arg) {
    esp_mqtt_client_handle_t cli = arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60 * 1000));
        char payload[96];
        xSemaphoreTake(s_snap_lock, portMAX_DELAY);
        bool valid = s_snap.valid;
        snprintf(payload, sizeof(payload),
                 "{\"temp\":%.1f,\"humi\":%.1f}", s_snap.t, s_snap.rh);
        xSemaphoreGive(s_snap_lock);
        if (!valid) continue;                          /* 无效数据不上报 */
        int id = esp_mqtt_client_publish(cli, "env/station01/data",
                                         payload, 0, 1, 0);   /* QoS1 */
        if (id >= 0) led_blink_once();
    }
}

void app_main(void) {
    s_i2c_lock = xSemaphoreCreateMutex();
    s_snap_lock = xSemaphoreCreateMutex();

    i2c_bus_init(I2C_NUM_0, 21, 22, 400000);
    ESP_ERROR_CHECK_WITHOUT_ABORT(sht30_init(&g_sht30, &i2c_hal, &(sht30_config_t){.addr = 0x44}));
    ssd1306_init(&g_oled, &i2c_hal, 0x3C);

    wifi_connect_blocking("SSID", "PASS", 15000);      /* 见 wifi.md 重连规范 */
    esp_mqtt_client_config_t mc = {.broker.address.uri = "mqtt://192.168.1.10"};
    esp_mqtt_client_handle_t cli = esp_mqtt_client_init(&mc);
    esp_mqtt_client_start(cli);

    xTaskCreate(sensor_task,  "sensor",  2048, NULL, 5, NULL);
    xTaskCreate(display_task, "display", 2048, NULL, 4, NULL);
    xTaskCreate(mqtt_task,    "mqtt",    4096, cli,  3, NULL);
}
```

## 4. 规范要点回顾

1. **总线互斥**：SHT30 与 OLED 共享 I2C，所有访问包锁 —— 参见 `templates/driver-template-rtos.c`
2. **数据快照**：采集/显示/上报解耦，读写快照独立小锁，锁内不做 I/O
3. **数据有效性**：CRC 校验失败标记 invalid，显示打感叹号、不上报脏数据
4. **固定周期**：`vTaskDelayUntil` 防周期漂移
5. **QoS1 上报 + LED 指示**：可观测、可远程排障

## 5. 验证步骤

- [ ] OLED 每 0.5s 刷新，数值与实际环境相符（对照 temperature.md 精度验证法）
- [ ] `mosquitto_sub -t env/+/data` 每 60s 收到一条 JSON
- [ ] 拔掉 SHT30：显示感叹号、停止上报、总线不死锁，插回自动恢复
- [ ] 断开 WiFi 路由器 5 分钟后恢复，MQTT 自动重连并继续上报
- [ ] 连续运行 24h 无重启（idf.py monitor 观察堆水位）
