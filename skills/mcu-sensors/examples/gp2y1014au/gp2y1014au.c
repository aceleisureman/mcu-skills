/**
 * GP2Y1014AU 粉尘传感器驱动 — STM32F103 标准库
 *
 * 接线:
 *   GP2Y1014AU VCC  → 5V
 *   GP2Y1014AU GND  → GND
 *   GP2Y1014AU VOUT → PA0 (ADC1_CH0, 经 1kΩ+10kΩ 分压)
 *   GP2Y1014AU ILED → PA2 (GPIO 推挽输出)
 *
 * 采样时序 (datasheet):
 *   ILED 高 → 等 280μs → ADC 采样 → 等 40μs → ILED 低 → 等 9680μs
 *   总周期 10ms
 */

#include "gp2y1014au.h"

/* ====== 配置参数 ====== */
#define NO_DUST_VOLTAGE    400      /* 零点电压 (mV)，需实测校准 */
#define COV_RATIO          0.20f    /* 灵敏度系数 (μg/m³ per mV) */
#define DIVIDER_RATIO      11       /* 1kΩ+10kΩ 分压板，还原乘 11 */
#define VREF_MV            3300     /* ADC 参考电压 (mV) */
#define ADC_MAX            4096     /* 12-bit ADC */
#define FILTER_SIZE        10       /* 滑动平均窗口 */

/* ====== 滤波状态 ====== */
static uint16_t s_buf[FILTER_SIZE];
static uint32_t s_sum;
static uint8_t  s_idx;
static uint8_t  s_init;

/**
 * 滑动平均滤波
 * 首次调用时用当前值填满缓冲区，避免启动时数值从 0 缓慢爬升
 */
uint16_t gp2y_filter(uint16_t new_val)
{
    if (!s_init) {
        uint8_t i;
        for (i = 0; i < FILTER_SIZE; i++) s_buf[i] = new_val;
        s_sum  = (uint32_t)new_val * FILTER_SIZE;
        s_init = 1;
        return new_val;
    }
    s_sum -= s_buf[s_idx];
    s_buf[s_idx] = new_val;
    s_sum += new_val;
    s_idx = (s_idx + 1) % FILTER_SIZE;
    return s_sum / FILTER_SIZE;
}

/**
 * 初始化 ADC1 通道 0 (PA0)
 */
void gp2y_adc_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef  ADC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);   /* 72MHz/6 = 12MHz (≤14MHz) */

    /* PA0 模拟输入 */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);

    /* 校准（必须，否则读数偏低） */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}

/**
 * 读取 ADC 值
 */
uint16_t gp2y_adc_read(uint8_t channel)
{
    ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_55Cycles5);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC1);
}

/**
 * 初始化 ILED 控制引脚 PA2
 */
void gp2y_iled_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_ResetBits(GPIOA, GPIO_Pin_2);   /* 默认关闭 LED */
}

/**
 * 读取粉尘浓度
 * @param raw       输出: 原始 ADC 值
 * @param voltage   输出: 电压 (mV)
 * @param density   输出: 浓度 (μg/m³)
 */
void gp2y_read(uint16_t *raw, uint16_t *voltage, float *density)
{
    uint16_t adc_val, filtered;

    /* 1. 开启内部 LED */
    GPIO_SetBits(GPIOA, GPIO_Pin_2);

    /* 2. 等待 280μs (datasheet 采样窗口) */
    delay_us(280);

    /* 3. 采集 ADC */
    adc_val = gp2y_adc_read(ADC_Channel_0);

    /* 4. 补足脉宽至 0.32ms，然后关闭 LED */
    delay_us(40);
    GPIO_ResetBits(GPIOA, GPIO_Pin_2);

    /* 5. 恢复等待 (总周期 10ms) */
    delay_us(9680);

    /* 6. 滤波 */
    filtered = gp2y_filter(adc_val);

    /* 7. 电压换算 (乘分压比 11 还原真实电压) */
    uint16_t v_mv = (uint16_t)(
        ((uint32_t)filtered * VREF_MV) / ADC_MAX * DIVIDER_RATIO
    );

    /* 8. 浓度计算 */
    float dust;
    if (v_mv > NO_DUST_VOLTAGE)
        dust = (float)(v_mv - NO_DUST_VOLTAGE) * COV_RATIO;
    else
        dust = 0;

    if (raw)     *raw     = adc_val;
    if (voltage) *voltage = v_mv;
    if (density) *density = dust;
}
