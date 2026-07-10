#ifndef __DS1302_H
#define __DS1302_H

#include "stm32f1xx_hal.h"
#include "config.h"

typedef struct {
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t week;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} DS1302_Time;

typedef struct {
    uint8_t ok;
    uint8_t ram_a5;
    uint8_t ram_3c;
    uint8_t ram_ff;
    uint8_t ram_00;
    uint8_t ram_55;
    uint8_t sec_raw;
} DS1302_DiagResult;

void DS1302_Init(void);
uint8_t DS1302_Check(void);
void DS1302_Diag(DS1302_DiagResult *d);
void DS1302_SetTime(DS1302_Time *time);
void DS1302_GetTime(DS1302_Time *time);
uint8_t DS1302_GetTimeChecked(DS1302_Time *time);
uint8_t DS1302_IsClassTime(DS1302_Time *time);
/* 按用户设置的上课时间段判断, 上午/下午各一段 */
uint8_t DS1302_IsClassTimeByCfg(DS1302_Time *time, uint8_t am_sh, uint8_t am_sm,
                                uint8_t am_eh, uint8_t am_em,
                                uint8_t pm_sh, uint8_t pm_sm,
                                uint8_t pm_eh, uint8_t pm_em);

#endif
