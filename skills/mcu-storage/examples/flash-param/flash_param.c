#include "flash_param.h"
#include <string.h>

/* STM32F103C8T6: 64KB flash, 末页 1KB 起始 0x0800FC00 */
#define FLASH_PARAM_PAGE_ADDR  ((uint32_t)0x0800FC00U)

static uint16_t CalcCRC(const Threshold *t, const ClassTime *c)
{
    uint16_t sum = 0;
    const uint8_t *p;
    uint8_t i;

    p = (const uint8_t *)t;
    for (i = 0; i < sizeof(Threshold); i++) sum = (uint16_t)(sum + p[i]);
    p = (const uint8_t *)c;
    for (i = 0; i < sizeof(ClassTime); i++) sum = (uint16_t)(sum + p[i]);
    return sum;
}

uint8_t FlashParam_Load(Threshold *thresh, ClassTime *class_time)
{
    const FlashParam *fp = (const FlashParam *)FLASH_PARAM_PAGE_ADDR;

    if (fp->magic != FLASH_PARAM_MAGIC) return 0;
    if (fp->crc != CalcCRC(&fp->thresh, &fp->class_time)) return 0;

    memcpy(thresh, &fp->thresh, sizeof(Threshold));
    memcpy(class_time, &fp->class_time, sizeof(ClassTime));
    return 1;
}

uint8_t FlashParam_Save(const Threshold *thresh, const ClassTime *class_time)
{
    FlashParam fp;
    FLASH_EraseInitTypeDef erase;
    uint32_t page_err = 0;
    HAL_StatusTypeDef status;

    fp.magic = FLASH_PARAM_MAGIC;
    fp.crc = CalcCRC(thresh, class_time);
    memcpy(&fp.thresh, thresh, sizeof(Threshold));
    memcpy(&fp.class_time, class_time, sizeof(ClassTime));

    HAL_FLASH_Unlock();

    /* 擦除末页 */
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = FLASH_PARAM_PAGE_ADDR;
    erase.NbPages = 1;
    status = HAL_FLASHEx_Erase(&erase, &page_err);
    if (status != HAL_OK) {
        HAL_FLASH_Lock();
        return 1;
    }

    /* 按半字写入 (结构体以 uint16_t 对齐, 长度为偶数) */
    const uint16_t *p = (const uint16_t *)&fp;
    uint32_t n = (sizeof(FlashParam) + 1U) / 2U;
    for (uint32_t i = 0; i < n; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                                   FLASH_PARAM_PAGE_ADDR + i * 2U, p[i]);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return 1;
        }
    }

    HAL_FLASH_Lock();
    return 0;
}
