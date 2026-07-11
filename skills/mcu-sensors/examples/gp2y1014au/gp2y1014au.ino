/**
 * GP2Y1014AU 粉尘传感器示例 — Arduino
 *
 * 接线:
 *   GP2Y1014AU  →  Arduino
 *     VCC       →  5V
 *     GND       →  GND
 *     AO (VOUT) →  A0  (经 1kΩ+10kΩ 分压板)
 *     ILED      →  D7
 *
 * 串口波特率: 9600
 */

#define COV_RATIO         0.2     /* 灵敏度系数 (μg/m³ per mV) */
#define NO_DUST_VOLTAGE   400     /* 零点电压 (mV) */
#define SYS_VOLTAGE       5000    /* Arduino ADC 参考电压 (mV) */
#define ADC_MAX           1024    /* 10-bit ADC */
#define DIVIDER_RATIO     11      /* 1kΩ+10kΩ 分压板 */

const int iled = 7;   /* LED 控制引脚 */
const int vout = 0;   /* 模拟输入引脚 A0 */

/**
 * 滑动平均滤波 (窗口 10)
 */
int filter(int m)
{
    static int flag_first = 0, buf[10], sum;
    const int max = 10;
    int i;

    if (flag_first == 0) {
        flag_first = 1;
        for (i = 0, sum = 0; i < max; i++) {
            buf[i] = m;
            sum += buf[i];
        }
        return m;
    }

    sum -= buf[0];
    for (i = 0; i < max - 1; i++)
        buf[i] = buf[i + 1];
    buf[9] = m;
    sum += buf[9];

    return sum / max;
}

void setup(void)
{
    pinMode(iled, OUTPUT);
    digitalWrite(iled, LOW);

    Serial.begin(9600);
    Serial.println("GP2Y1014AU Dust Sensor Ready.");
}

void loop(void)
{
    int adcvalue;
    float voltage, density;

    /* 1. 开启 LED */
    digitalWrite(iled, HIGH);
    /* 2. 等待 280μs */
    delayMicroseconds(280);
    /* 3. 采集 ADC */
    adcvalue = analogRead(vout);
    /* 4. 关闭 LED (总脉宽 0.32ms) */
    delayMicroseconds(40);
    digitalWrite(iled, LOW);

    /* 5. 滤波 */
    adcvalue = filter(adcvalue);

    /* 6. 电压换算 (乘分压比 11) */
    voltage = (SYS_VOLTAGE / (float)ADC_MAX) * adcvalue * DIVIDER_RATIO;

    /* 7. 浓度计算 */
    if (voltage >= NO_DUST_VOLTAGE) {
        voltage -= NO_DUST_VOLTAGE;
        density = voltage * COV_RATIO;
    } else {
        density = 0;
    }

    /* 8. 输出 */
    Serial.print("Raw:");
    Serial.print(adcvalue);
    Serial.print(" Vol:");
    Serial.print((int)voltage);
    Serial.print(" mV  PM:");
    Serial.print(density);
    Serial.println(" ug/m3");

    delay(1000);
}
