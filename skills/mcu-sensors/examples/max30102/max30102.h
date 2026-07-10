#ifndef __MAX30102_H
#define __MAX30102_H

#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_i2c.h"

#define MAX30102_ADDR          0xAE
#define MAX30102_ADDR_ALT      0x57
#define MAX30102_I2C_TIMEOUT   100
#define MAX30102_SAMPLE_DELAY_MS     80u
#define MAX30102_EFFECTIVE_SAMPLE_HZ  13u
#define MAX30102_HR_MIN_BPM          40u
#define MAX30102_HR_MAX_BPM          200u
#define MAX30102_MIN_VALID_SAMPLES   32u

#define MAX30102_REG_INTR_STATUS1   0x00
#define MAX30102_REG_INTR_STATUS2   0x01
#define MAX30102_REG_INTR_ENABLE1   0x02
#define MAX30102_REG_INTR_ENABLE2   0x03
#define MAX30102_REG_FIFO_WR_PTR    0x04
#define MAX30102_REG_OVF_COUNTER    0x05
#define MAX30102_REG_FIFO_RD_PTR    0x06
#define MAX30102_REG_FIFO_DATA      0x07
#define MAX30102_REG_FIFO_CONFIG    0x08
#define MAX30102_REG_MODE_CONFIG    0x09
#define MAX30102_REG_SPO2_CONFIG    0x0A
#define MAX30102_REG_LED1_PA        0x0C
#define MAX30102_REG_LED2_PA        0x0D
#define MAX30102_REG_PART_ID        0xFF

#define MAX30102_MODE_HR_ONLY       0x02
#define MAX30102_MODE_SPO2          0x03
#define MAX30102_MODE_MULTI_LED     0x07

/* ---- 采样/算法单一事实来源(SSOT): 所有时间相关常量都由此推导, 避免再次出现
 *      "硬件 12.5Hz / 注释 50Hz / 公式 100Hz" 这种三处不一致的致命 bug ---- */
#define MAX30102_BUF_LEN            100   /* 环形 buffer 长度 */
#define MAX30102_FS_HZ             25    /* 有效采样率 = 100sps / SMP_AVE(4); 窗口 = BUF_LEN/FS = 4s */
#define MAX30102_MIN_BPM           40
#define MAX30102_MAX_BPM           200
/* 峰间隔(样本数)边界由采样率与 BPM 量程推导. MAX 端 +1 样本余量, 避免整数除法
 * 把 40bpm(真值≈37.5 样本)裁掉 --- 老人心动过缓需要能测到. */
#define MAX30102_MIN_INTERVAL      ((MAX30102_FS_HZ * 60) / MAX30102_MAX_BPM)        /* 7  ~214bpm */
#define MAX30102_MAX_INTERVAL      (((MAX30102_FS_HZ * 60) / MAX30102_MIN_BPM) + 1)  /* 38 ~39bpm  */
/* BPM 合理性闸: 在量程两端各放 2bpm 量化容差, 防止 40/200 边界被四舍五入裁掉 */
#define MAX30102_HR_LO             (MAX30102_MIN_BPM - 2)
#define MAX30102_HR_HI             (MAX30102_MAX_BPM + 2)
#define MAX30102_FINGER_IR_MIN     7000  /* IR 直流低于此判定手指未贴稳 */

/* 初始化失败具体原因 */
#define MAX30102_ERR_NONE           0   /* 无错误 */
#define MAX30102_ERR_NO_DEVICE      1   /* I2C 总线未探测到器件(0xAE/0x57 均无应答) */
#define MAX30102_ERR_BAD_ID         2   /* 探测到器件但 Part ID != 0x15 */
#define MAX30102_ERR_BUS            3   /* I2C 读写寄存器失败(总线错误) */

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t device_addr;
    uint32_t red_buffer[MAX30102_BUF_LEN];
    uint32_t ir_buffer[MAX30102_BUF_LEN];
    uint8_t buffer_index;
    uint8_t buffer_count;
    int32_t hr_value;
    int32_t spo2_value;
    uint8_t hr_valid;
    uint8_t spo2_valid;
    int32_t hr_last_valid;      /* 最近一次有效心率, 用于无效时软保持 */
    int32_t hr_ema;            /* 心率指数滑动平均, 用于显示平滑(替代硬锁) */
    uint8_t hr_invalid_count;   /* 连续无效次数, 达到阈值才真正判无效(滞回) */
    uint8_t detected;
    uint8_t init_err;     /* 见 MAX30102_ERR_xxx */
} MAX30102_HandleTypeDef;

HAL_StatusTypeDef MAX30102_Init(MAX30102_HandleTypeDef *hmax, I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef MAX30102_ReadFIFO(MAX30102_HandleTypeDef *hmax);
int32_t MAX30102_CalculateHeartRate(MAX30102_HandleTypeDef *hmax);
HAL_StatusTypeDef MAX30102_Reset(MAX30102_HandleTypeDef *hmax);
uint8_t MAX30102_ReadPartID(MAX30102_HandleTypeDef *hmax);

#endif
