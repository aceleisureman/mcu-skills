#include "ds1302.h"

#define DS1302_SEC_REG      0x80
#define DS1302_MIN_REG      0x82
#define DS1302_HOUR_REG     0x84
#define DS1302_DAY_REG      0x86
#define DS1302_MONTH_REG    0x88
#define DS1302_WEEK_REG     0x8A
#define DS1302_YEAR_REG     0x8C
#define DS1302_WP_REG       0x8E

static void DS1302_DelayUs(uint32_t us)
{
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    uint32_t told = SysTick->VAL;
    uint32_t tcnt = 0;
    uint32_t guard = ticks * 8U + 100000U;

    while (tcnt < ticks) {
        uint32_t tnow = SysTick->VAL;
        if (tnow != told) {
            if (tnow < told) {
                tcnt += told - tnow;
            } else {
                tcnt += SysTick->LOAD - tnow + told;
            }
            told = tnow;
        }
        if (--guard == 0U) break;
    }
}

static void SCLK_H(void) { HAL_GPIO_WritePin(DS1302_CLK_PORT, DS1302_CLK_PIN, GPIO_PIN_SET); }
static void SCLK_L(void) { HAL_GPIO_WritePin(DS1302_CLK_PORT, DS1302_CLK_PIN, GPIO_PIN_RESET); }
static void CE_H(void)   { HAL_GPIO_WritePin(DS1302_RST_PORT, DS1302_RST_PIN, GPIO_PIN_SET); }
static void CE_L(void)   { HAL_GPIO_WritePin(DS1302_RST_PORT, DS1302_RST_PIN, GPIO_PIN_RESET); }
static void DATA_H(void) { HAL_GPIO_WritePin(DS1302_DAT_PORT, DS1302_DAT_PIN, GPIO_PIN_SET); }
static void DATA_L(void) { HAL_GPIO_WritePin(DS1302_DAT_PORT, DS1302_DAT_PIN, GPIO_PIN_RESET); }

static void DS1302_GPIO_InitPins(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = DS1302_CLK_PIN;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(DS1302_CLK_PORT, &gpio);
    SCLK_L();

    gpio.Pin = DS1302_RST_PIN;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(DS1302_RST_PORT, &gpio);
    CE_L();
}

static void DS1302_DATAOUT_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = DS1302_DAT_PIN;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(DS1302_DAT_PORT, &gpio);
    DATA_L();
}

static void DS1302_DATAINPUT_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = DS1302_DAT_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DS1302_DAT_PORT, &gpio);
}

static uint8_t DS1302_DATA_READ(void)
{
    return HAL_GPIO_ReadPin(DS1302_DAT_PORT, DS1302_DAT_PIN) ? 1U : 0U;
}

static uint8_t BCD2DEC(uint8_t bcd) { return (uint8_t)((bcd >> 4) * 10U + (bcd & 0x0FU)); }
static uint8_t DEC2BCD(uint8_t dec) { return (uint8_t)(((dec / 10U) << 4) | (dec % 10U)); }

static uint8_t DS1302_BCDValid(uint8_t bcd, uint8_t mask, uint8_t max)
{
    uint8_t v = bcd & mask;
    if ((v & 0x0FU) > 9U) return 0;
    if (((v >> 4) & 0x0FU) > 9U) return 0;
    return BCD2DEC(v) <= max;
}

static void DS1302_write_onebyte(uint8_t data)
{
    uint8_t count = 0;

    SCLK_L();
    DS1302_DATAOUT_init();

    for (count = 0; count < 8; count++) {
        SCLK_L();
        if (data & 0x01U) {
            DATA_H();
        } else {
            DATA_L();
        }
        SCLK_H();
        data >>= 1;
    }
}

static void DS1302_wirte_rig(uint8_t address, uint8_t data)
{
    CE_L();
    SCLK_L();
    DS1302_DelayUs(1);
    CE_H();
    DS1302_DelayUs(3);
    DS1302_write_onebyte(address);
    DS1302_write_onebyte(data);
    CE_L();
    SCLK_L();
    DS1302_DelayUs(3);
}

static uint8_t DS1302_read_rig(uint8_t address)
{
    uint8_t count = 0;
    uint8_t return_data = 0x00;

    CE_L();
    SCLK_L();
    DS1302_DelayUs(3);
    CE_H();
    DS1302_DelayUs(3);
    DS1302_write_onebyte(address);
    DS1302_DATAINPUT_init();
    DS1302_DelayUs(3);

    for (count = 0; count < 8; count++) {
        DS1302_DelayUs(3);
        return_data >>= 1;
        SCLK_H();
        DS1302_DelayUs(5);
        SCLK_L();
        DS1302_DelayUs(30);
        if (DS1302_DATA_READ()) {
            return_data |= 0x80U;
        }
    }

    DS1302_DelayUs(2);
    CE_L();
    DS1302_DATAOUT_init();
    DATA_L();
    return return_data;
}

static uint8_t DS1302_TimeValid(const DS1302_Time *time)
{
    if (time->sec > 59U || time->min > 59U || time->hour > 23U) return 0;
    if (time->month < 1U || time->month > 12U) return 0;
    if (time->day < 1U || time->day > 31U) return 0;
    if (time->week < 1U || time->week > 7U) return 0;
    return 1;
}

void DS1302_Init(void)
{
    DS1302_GPIO_InitPins();
    DS1302_DATAOUT_init();
    CE_L();
    SCLK_L();
    DATA_L();

    DS1302_wirte_rig(DS1302_WP_REG, 0x00);
    DS1302_wirte_rig(DS1302_SEC_REG, DS1302_read_rig(DS1302_SEC_REG | 0x01U) & 0x7FU);
    DS1302_wirte_rig(DS1302_HOUR_REG, DS1302_read_rig(DS1302_HOUR_REG | 0x01U) & 0x3FU);
    DS1302_wirte_rig(DS1302_WP_REG, 0x80);
}

uint8_t DS1302_Check(void)
{
    uint8_t v1;
    uint8_t v2;

    DS1302_wirte_rig(DS1302_WP_REG, 0x00);
    DS1302_wirte_rig(0xC0, 0xA5);
    v1 = DS1302_read_rig(0xC1);
    DS1302_wirte_rig(0xC0, 0x3C);
    v2 = DS1302_read_rig(0xC1);
    DS1302_wirte_rig(DS1302_WP_REG, 0x80);

    return (v1 == 0xA5U) && (v2 == 0x3CU);
}

void DS1302_Diag(DS1302_DiagResult *d)
{
    DS1302_wirte_rig(DS1302_WP_REG, 0x00);
    DS1302_wirte_rig(0xC0, 0xA5);
    d->ram_a5 = DS1302_read_rig(0xC1);
    DS1302_wirte_rig(0xC2, 0x3C);
    d->ram_3c = DS1302_read_rig(0xC3);
    DS1302_wirte_rig(0xC4, 0xFF);
    d->ram_ff = DS1302_read_rig(0xC5);
    DS1302_wirte_rig(0xC6, 0x00);
    d->ram_00 = DS1302_read_rig(0xC7);
    DS1302_wirte_rig(0xC8, 0x55);
    d->ram_55 = DS1302_read_rig(0xC9);
    d->sec_raw = DS1302_read_rig(0x81);
    d->ok = (d->ram_a5 == 0xA5U) && (d->ram_3c == 0x3CU) && (d->ram_ff == 0xFFU);
    DS1302_wirte_rig(DS1302_WP_REG, 0x80);
}

uint8_t DS1302_GetTimeChecked(DS1302_Time *time)
{
    uint8_t sec = DS1302_read_rig(0x81);
    uint8_t min = DS1302_read_rig(0x83);
    uint8_t hour = DS1302_read_rig(0x85);
    uint8_t day = DS1302_read_rig(0x87);
    uint8_t month = DS1302_read_rig(0x89);
    uint8_t week = DS1302_read_rig(0x8B);
    uint8_t year = DS1302_read_rig(0x8D);

    if (!DS1302_BCDValid(sec, 0x7F, 59)) return 0;
    if (!DS1302_BCDValid(min, 0x7F, 59)) return 0;
    if (!DS1302_BCDValid(hour, 0x3F, 23)) return 0;
    if (!DS1302_BCDValid(day, 0x3F, 31)) return 0;
    if (!DS1302_BCDValid(month, 0x1F, 12)) return 0;
    if (!DS1302_BCDValid(week, 0x0F, 7)) return 0;
    if (!DS1302_BCDValid(year, 0xFF, 99)) return 0;

    time->sec = BCD2DEC(sec & 0x7F);
    time->min = BCD2DEC(min & 0x7F);
    time->hour = BCD2DEC(hour & 0x3F);
    time->day = BCD2DEC(day & 0x3F);
    time->month = BCD2DEC(month & 0x1F);
    time->week = BCD2DEC(week & 0x0F);
    time->year = BCD2DEC(year);

    return DS1302_TimeValid(time);
}

void DS1302_SetTime(DS1302_Time *time)
{
    uint8_t week = (time->week >= 1U && time->week <= 7U) ? time->week : 1U;

    DS1302_wirte_rig(DS1302_WP_REG, 0x00);
    DS1302_wirte_rig(DS1302_SEC_REG, DEC2BCD(time->sec) & 0x7FU);
    DS1302_wirte_rig(DS1302_MIN_REG, DEC2BCD(time->min));
    DS1302_wirte_rig(DS1302_HOUR_REG, DEC2BCD(time->hour) & 0x3FU);
    DS1302_wirte_rig(DS1302_DAY_REG, DEC2BCD(time->day));
    DS1302_wirte_rig(DS1302_MONTH_REG, DEC2BCD(time->month));
    DS1302_wirte_rig(DS1302_WEEK_REG, DEC2BCD(week));
    DS1302_wirte_rig(DS1302_YEAR_REG, DEC2BCD(time->year));
    DS1302_wirte_rig(DS1302_WP_REG, 0x80);
}

void DS1302_GetTime(DS1302_Time *time)
{
    if (!DS1302_GetTimeChecked(time)) {
        time->sec = 0;
        time->min = 0;
        time->hour = 0;
        time->week = 0;
        time->day = 0;
        time->month = 0;
        time->year = 0;
    }
}

uint8_t DS1302_IsClassTime(DS1302_Time *time)
{
    uint16_t t = (uint16_t)time->hour * 60U + time->min;

    if (t >= 480U && t <= 525U) return 1;
    if (t >= 530U && t <= 575U) return 1;
    if (t >= 585U && t <= 630U) return 1;
    if (t >= 635U && t <= 680U) return 1;
    if (t >= 780U && t <= 825U) return 1;
    if (t >= 830U && t <= 875U) return 1;
    if (t >= 885U && t <= 930U) return 1;
    if (t >= 935U && t <= 980U) return 1;

    return 0;
}

uint8_t DS1302_IsClassTimeByCfg(DS1302_Time *time, uint8_t am_sh, uint8_t am_sm,
                                uint8_t am_eh, uint8_t am_em,
                                uint8_t pm_sh, uint8_t pm_sm,
                                uint8_t pm_eh, uint8_t pm_em)
{
    uint16_t t   = (uint16_t)time->hour * 60U + time->min;
    uint16_t ams = (uint16_t)am_sh * 60U + am_sm;
    uint16_t ame = (uint16_t)am_eh * 60U + am_em;
    uint16_t pms = (uint16_t)pm_sh * 60U + pm_sm;
    uint16_t pme = (uint16_t)pm_eh * 60U + pm_em;

    if (t >= ams && t <= ame) return 1;
    if (t >= pms && t <= pme) return 1;
    return 0;
}
