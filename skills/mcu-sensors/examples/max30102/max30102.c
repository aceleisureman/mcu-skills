#include "max30102.h"
#include <string.h>
#include <stdlib.h>

static HAL_StatusTypeDef MAX30102_WriteReg(MAX30102_HandleTypeDef *hmax, uint8_t reg, uint8_t data)
{
    return HAL_I2C_Mem_Write(hmax->hi2c, hmax->device_addr, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, MAX30102_I2C_TIMEOUT);
}

static HAL_StatusTypeDef MAX30102_ReadReg(MAX30102_HandleTypeDef *hmax, uint8_t reg, uint8_t *data)
{
    return HAL_I2C_Mem_Read(hmax->hi2c, hmax->device_addr, reg, I2C_MEMADD_SIZE_8BIT, data, 1, MAX30102_I2C_TIMEOUT);
}

static HAL_StatusTypeDef MAX30102_ReadMultiReg(MAX30102_HandleTypeDef *hmax, uint8_t reg, uint8_t *data, uint8_t len)
{
    return HAL_I2C_Mem_Read(hmax->hi2c, hmax->device_addr, reg, I2C_MEMADD_SIZE_8BIT, data, len, MAX30102_I2C_TIMEOUT);
}

static uint8_t MAX30102_DetectAddress(I2C_HandleTypeDef *hi2c)
{
    uint8_t part_id = 0;
    
    if (HAL_I2C_Mem_Read(hi2c, MAX30102_ADDR, MAX30102_REG_PART_ID, I2C_MEMADD_SIZE_8BIT, &part_id, 1, 100) == HAL_OK) {
        if (part_id == 0x15) return MAX30102_ADDR;
    }
    
    if (HAL_I2C_Mem_Read(hi2c, MAX30102_ADDR_ALT, MAX30102_REG_PART_ID, I2C_MEMADD_SIZE_8BIT, &part_id, 1, 100) == HAL_OK) {
        if (part_id == 0x15) return MAX30102_ADDR_ALT;
    }
    
    return 0;
}

HAL_StatusTypeDef MAX30102_Reset(MAX30102_HandleTypeDef *hmax)
{
    return MAX30102_WriteReg(hmax, MAX30102_REG_MODE_CONFIG, 0x40);
}

uint8_t MAX30102_ReadPartID(MAX30102_HandleTypeDef *hmax)
{
    uint8_t part_id = 0;
    MAX30102_ReadReg(hmax, MAX30102_REG_PART_ID, &part_id);
    return part_id;
}

HAL_StatusTypeDef MAX30102_Init(MAX30102_HandleTypeDef *hmax, I2C_HandleTypeDef *hi2c)
{
    hmax->hi2c = hi2c;
    hmax->buffer_index = 0;
    hmax->buffer_count = 0;
    hmax->hr_value = 0;
    hmax->spo2_value = 0;
    hmax->hr_valid = 0;
    hmax->spo2_valid = 0;
    hmax->hr_last_valid = 0;
    hmax->hr_ema = 0;
    hmax->hr_invalid_count = 0;
    hmax->detected = 0;
    hmax->init_err = MAX30102_ERR_NONE;
    memset(hmax->red_buffer, 0, sizeof(hmax->red_buffer));
    memset(hmax->ir_buffer, 0, sizeof(hmax->ir_buffer));

    hmax->device_addr = MAX30102_DetectAddress(hi2c);
    if (hmax->device_addr == 0) {
        hmax->init_err = MAX30102_ERR_NO_DEVICE;
        return HAL_ERROR;
    }
    hmax->detected = 1;

    HAL_StatusTypeDef status;

    status = MAX30102_Reset(hmax);
    if (status != HAL_OK) { hmax->init_err = MAX30102_ERR_BUS; return status; }
    HAL_Delay(100);

    uint8_t part_id = MAX30102_ReadPartID(hmax);
    if (part_id != 0x15) { hmax->init_err = MAX30102_ERR_BAD_ID; return HAL_ERROR; }

    status = MAX30102_WriteReg(hmax, MAX30102_REG_INTR_ENABLE1, 0xC0);
    if (status != HAL_OK) { hmax->init_err = MAX30102_ERR_BUS; return status; }

    status = MAX30102_WriteReg(hmax, MAX30102_REG_INTR_ENABLE2, 0x00);
    if (status != HAL_OK) { hmax->init_err = MAX30102_ERR_BUS; return status; }

    status = MAX30102_WriteReg(hmax, MAX30102_REG_FIFO_WR_PTR, 0x00);
    if (status != HAL_OK) { hmax->init_err = MAX30102_ERR_BUS; return status; }

    status = MAX30102_WriteReg(hmax, MAX30102_REG_OVF_COUNTER, 0x00);
    if (status != HAL_OK) { hmax->init_err = MAX30102_ERR_BUS; return status; }

    status = MAX30102_WriteReg(hmax, MAX30102_REG_FIFO_RD_PTR, 0x00);
    if (status != HAL_OK) { hmax->init_err = MAX30102_ERR_BUS; return status; }

    /* FIFO_CONFIG=0x4F: SMP_AVE=4(降噪), rollover 关, A_FULL=15.
     * 与 SPO2_CONFIG 的 100sps 组合 → 有效采样率 100/4 = 25Hz = MAX30102_FS_HZ. */
    status = MAX30102_WriteReg(hmax, MAX30102_REG_FIFO_CONFIG, 0x4F);
    if (status != HAL_OK) { hmax->init_err = MAX30102_ERR_BUS; return status; }

    status = MAX30102_WriteReg(hmax, MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2);
    if (status != HAL_OK) { hmax->init_err = MAX30102_ERR_BUS; return status; }

    /* SPO2_CONFIG=0x27: ADC range 4096(18bit), LED pw 411us, 采样率 100sps.
     * 经 SMP_AVE=4 后有效采样率 25Hz(MAX30102_FS_HZ), 100 点 buffer = 4 秒窗口,
     * 覆盖 3~5 个心率周期, 且 40ms 时间分辨率明显优于原 12.5Hz(80ms)的量化误差.
     * 关键修复: 原 0x23(50sps)+SMP_AVE4 = 12.5Hz, 但 BPM 公式按 100Hz 算, 三处采样率
     * 不一致导致正常心率被间隔门限滤除/数值乱跳. 现全部由 MAX30102_FS_HZ 单点推导. */
    status = MAX30102_WriteReg(hmax, MAX30102_REG_SPO2_CONFIG, 0x27);
    if (status != HAL_OK) { hmax->init_err = MAX30102_ERR_BUS; return status; }

    status = MAX30102_WriteReg(hmax, MAX30102_REG_LED1_PA, 0x32);
    if (status != HAL_OK) { hmax->init_err = MAX30102_ERR_BUS; return status; }

    status = MAX30102_WriteReg(hmax, MAX30102_REG_LED2_PA, 0x32);
    if (status != HAL_OK) { hmax->init_err = MAX30102_ERR_BUS; return status; }

    return HAL_OK;
}

HAL_StatusTypeDef MAX30102_ReadFIFO(MAX30102_HandleTypeDef *hmax)
{
    uint8_t wr_ptr, rd_ptr;

    /* 比较 FIFO 写/读指针得到待读样本数, 一次性排空全部新样本.
     * 原实现每次只读 1 个样本, 主循环相位漂移时会越积越多, rollover 关闭下
     * FIFO 写满后写指针停住 → wr==rd 误判为无数据 → buffer 永久冻结. 整批排空可避免. */
    if (MAX30102_ReadReg(hmax, MAX30102_REG_FIFO_WR_PTR, &wr_ptr) != HAL_OK) return HAL_ERROR;
    if (MAX30102_ReadReg(hmax, MAX30102_REG_FIFO_RD_PTR, &rd_ptr) != HAL_OK) return HAL_ERROR;
    wr_ptr &= 0x1F;
    rd_ptr &= 0x1F;

    uint8_t available = (uint8_t)((wr_ptr - rd_ptr) & 0x1F);
    if (available == 0) {
        return HAL_OK;  /* 无新样本 */
    }

    for (uint8_t s = 0; s < available; s++) {
        uint8_t fifo_data[6];
        if (MAX30102_ReadMultiReg(hmax, MAX30102_REG_FIFO_DATA, fifo_data, 6) != HAL_OK) {
            return HAL_ERROR;
        }

        /* SpO2 模式 FIFO 顺序: RED 在前, IR 在后, 各 3 字节 18bit */
        uint32_t red = ((uint32_t)fifo_data[0] << 16) | ((uint32_t)fifo_data[1] << 8) | fifo_data[2];
        uint32_t ir  = ((uint32_t)fifo_data[3] << 16) | ((uint32_t)fifo_data[4] << 8) | fifo_data[5];
        red &= 0x03FFFF;
        ir  &= 0x03FFFF;

        /* 始终入栈(含手指移开时的低 IR), 这样 buffer 永远反映当前光学状态,
         * 取指/移开判定才能及时跟随; 原 ir>1000 门限会在手指移开时停止更新 → 旧值冻结. */
        hmax->red_buffer[hmax->buffer_index] = red;
        hmax->ir_buffer[hmax->buffer_index] = ir;
        hmax->buffer_index = (uint8_t)((hmax->buffer_index + 1) % MAX30102_BUF_LEN);
        if (hmax->buffer_count < MAX30102_BUF_LEN) hmax->buffer_count++;
    }

    return HAL_OK;
}

int32_t MAX30102_CalculateHeartRate(MAX30102_HandleTypeDef *hmax)
{
    /* 首次约 2 秒数据即可出值(覆盖 2+ 个心率周期); buffer 满(4秒)后稳态精度更高.
     * 所有时间相关阈值均由 MAX30102_FS_HZ 推导, 保证与硬件采样率一致. */
    uint8_t n = hmax->buffer_count;
    if (n < MAX30102_FS_HZ * 2) {
        hmax->hr_valid = 0;
        return 0;
    }

    /* ---- 手指存在判定: 取"最近 ~0.5s"的 IR 直流均值, 而非全窗均值 ----
     * 手指贴稳时 IR 通常数万, 移开后漏光仅几千; 用最近样本可在下一轮即识别移开,
     * 且不清空 buffer, 重新放回手指能快速恢复. */
    uint8_t recent_n = MAX30102_FS_HZ / 2;
    if (recent_n < 1) recent_n = 1;
    if (recent_n > n) recent_n = n;
    uint32_t recent_sum = 0;
    for (uint8_t k = 0; k < recent_n; k++) {
        uint8_t idx = (uint8_t)((hmax->buffer_index + MAX30102_BUF_LEN - recent_n + k) % MAX30102_BUF_LEN);
        recent_sum += hmax->ir_buffer[idx];
    }
    uint32_t recent_mean = recent_sum / recent_n;
    if (recent_mean < MAX30102_FINGER_IR_MIN) {
        /* 手指移开: 立即判无效并清状态(不清 buffer 以便快速恢复) */
        hmax->hr_valid = 0;
        hmax->hr_last_valid = 0;
        hmax->hr_ema = 0;
        hmax->hr_invalid_count = 6;
        return 0;
    }

    /* ---- 去直流: 全窗均值作为基线(粗高通) ---- */
    uint32_t ir_mean = 0;
    for (uint8_t i = 0; i < n; i++) ir_mean += hmax->ir_buffer[i];
    ir_mean /= n;

    /* 按时间顺序(最旧→最新)把环形 buffer 展开为线性数组, 峰检测必须基于时间序 */
    int32_t ir_signal[MAX30102_BUF_LEN];
    uint8_t start = (hmax->buffer_count >= MAX30102_BUF_LEN) ? hmax->buffer_index : 0;
    for (uint8_t i = 0; i < n; i++) {
        ir_signal[i] = (int32_t)hmax->ir_buffer[(start + i) % MAX30102_BUF_LEN] - (int32_t)ir_mean;
    }

    /* 3 点滑动平均去高频毛刺 */
    int32_t ir_smooth[MAX30102_BUF_LEN];
    for (uint8_t i = 0; i < n; i++) {
        int32_t a = (i > 0)     ? ir_signal[i-1] : ir_signal[i];
        int32_t b = ir_signal[i];
        int32_t c = (i < n - 1) ? ir_signal[i+1] : ir_signal[i];
        ir_smooth[i] = (a + b + c) / 3;
    }

    /* 信号幅度: 太小说明手指未贴稳, 不硬算伪峰 */
    int32_t max_val = ir_smooth[0], min_val = ir_smooth[0];
    for (uint8_t i = 1; i < n; i++) {
        if (ir_smooth[i] > max_val) max_val = ir_smooth[i];
        if (ir_smooth[i] < min_val) min_val = ir_smooth[i];
    }
    int32_t amplitude = max_val - min_val;
    if (amplitude < 50) {
        if (hmax->hr_invalid_count < 255) hmax->hr_invalid_count += 2;
        if (hmax->hr_invalid_count >= 6) {
            hmax->hr_valid = 0;
            hmax->hr_last_valid = 0;
            hmax->hr_ema = 0;
        }
        return hmax->hr_value;
    }

    /* 阈值: 正峰的 40% (相对幅度, 抗整体强度波动); 邻域 ±3 极大值判定 */
    int32_t threshold = (max_val * 2) / 5;
    if (threshold < 30) threshold = 30;

    uint8_t peaks[20];
    uint8_t peak_count = 0;
    uint8_t i_end = (n >= 3) ? (uint8_t)(n - 3) : 0;
    for (uint8_t i = 3; i < i_end && peak_count < 20; i++) {
        if (ir_smooth[i] > threshold &&
            ir_smooth[i] >= ir_smooth[i-1] && ir_smooth[i] >= ir_smooth[i+1] &&
            ir_smooth[i] >  ir_smooth[i-2] && ir_smooth[i] >  ir_smooth[i+2] &&
            ir_smooth[i] >  ir_smooth[i-3] && ir_smooth[i] >  ir_smooth[i+3]) {
            /* 不应期: 两峰至少相隔 MAX30102_MIN_INTERVAL(对应 MAX_BPM) */
            if (peak_count == 0 || (i - peaks[peak_count-1]) >= MAX30102_MIN_INTERVAL) {
                peaks[peak_count++] = i;
            }
        }
    }

    if (peak_count >= 2) {
        /* 收集合法峰间隔(在 [MIN,MAX] 内, 即对应 40~200bpm) */
        uint8_t intervals[19];
        uint8_t ic = 0;
        for (uint8_t i = 1; i < peak_count; i++) {
            uint8_t iv = (uint8_t)(peaks[i] - peaks[i-1]);
            if (iv >= MAX30102_MIN_INTERVAL && iv <= MAX30102_MAX_INTERVAL) {
                intervals[ic++] = iv;
            }
        }

        if (ic >= 1) {
            /* 中位数滤波: 对漏检(间隔翻倍)/误检(间隔减半)鲁棒, 只取接近中位数的间隔做平均 */
            for (uint8_t a = 1; a < ic; a++) {
                uint8_t key = intervals[a];
                int8_t b = (int8_t)a - 1;
                while (b >= 0 && intervals[b] > key) { intervals[b+1] = intervals[b]; b--; }
                intervals[b+1] = key;
            }
            uint8_t median = intervals[ic / 2];

            uint32_t total = 0;
            uint8_t cnt = 0;
            for (uint8_t k = 0; k < ic; k++) {
                int32_t d = (int32_t)intervals[k] - (int32_t)median;
                if (d < 0) d = -d;
                if (d * 10 <= (int32_t)median * 3) {   /* 偏离中位数 <=30% 才纳入 */
                    total += intervals[k];
                    cnt++;
                }
            }
            if (cnt == 0) { total = median; cnt = 1; }
            uint32_t avg_interval = total / cnt;

            int32_t hr = (int32_t)((60 * MAX30102_FS_HZ + avg_interval / 2) / avg_interval);
            if (hr >= MAX30102_HR_LO && hr <= MAX30102_HR_HI) {
                /* EMA 平滑显示(替代原"偏差>20 即拒"的硬锁: 硬锁一旦初值错就永久卡死) */
                if (hmax->hr_ema == 0) hmax->hr_ema = hr;
                else hmax->hr_ema = (hmax->hr_ema * 3 + hr + 2) / 4;
                hmax->hr_last_valid = hmax->hr_ema;
                hmax->hr_value = hmax->hr_ema;
                hmax->hr_valid = 1;
                hmax->hr_invalid_count = 0;
                return hmax->hr_value;
            }
        }
    }

    /* 本轮未算出有效心率: 滞回保持, 连续多次无效才真正判无效, 避免单次抖动变 --- */
    if (hmax->hr_invalid_count < 255) hmax->hr_invalid_count++;
    if (hmax->hr_invalid_count >= 3 && hmax->hr_last_valid > 0) {
        hmax->hr_value = hmax->hr_last_valid;   /* 软保持: 仍显示上次有效值 */
    }
    if (hmax->hr_invalid_count >= 6) {
        hmax->hr_valid = 0;   /* 连续 6 次无效才真正置无效(手指移开) */
        hmax->hr_ema = 0;
    }
    return hmax->hr_value;
}
