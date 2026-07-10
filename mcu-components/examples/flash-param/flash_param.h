#ifndef __FLASH_PARAM_H
#define __FLASH_PARAM_H

#include "stm32f1xx_hal.h"
#include "app.h"

/* 掉电保存: 使用 flash 最后一页 (0x0800FC00, 1KB) 存储阈值+上课时间.
 * 校验 magic 标记区分首次上电(未写入)与已保存数据. */

#define FLASH_PARAM_MAGIC  0xA55AU

typedef struct {
    uint16_t magic;                 /* 校验标记, == FLASH_PARAM_MAGIC 表示有效 */
    uint16_t crc;                   /* 简单校验和 */
    Threshold thresh;               /* 阈值参数 */
    ClassTime class_time;           /* 上课时间段 */
} FlashParam;

/* 上电加载: 若 flash 无有效数据则保持 app 中默认值, 返回 1=加载成功 0=无有效数据 */
uint8_t FlashParam_Load(Threshold *thresh, ClassTime *class_time);

/* 保存阈值和上课时间到 flash (擦除+写入整页), 返回 0=成功 */
uint8_t FlashParam_Save(const Threshold *thresh, const ClassTime *class_time);

#endif
