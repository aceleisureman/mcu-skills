#ifndef __FINGER_H
#define __FINGER_H

#include "stm32f1xx_hal.h"

/* AS608 指纹模块 - USART3
 * STM32 PB10 (USART3_TX) -> 模块 RX
 * STM32 PB11 (USART3_RX) <- 模块 TX
 * 模块出厂默认地址 0xFFFFFFFF, 波特率 57600
 */
#define FINGER_PORT        GPIOB
#define FINGER_TX_PIN      GPIO_PIN_10
#define FINGER_RX_PIN      GPIO_PIN_11
#define FINGER_BAUD_RATE   57600UL
#define FINGER_ADDR        0xFFFFFFFFUL

/* AS608 确认码 */
#define FINGER_OK               0x00U   /* 成功 */
#define FINGER_NO_FINGER        0x02U   /* 无手指 */
#define FINGER_COMM_ERR         0xFFU   /* 通信失败(超时/校验错) */
#define FINGER_NO_MATCH         0x09U   /* 未搜索到指纹 */

/* 协议层: 返回确认码(0x00=成功), 0xFF=通信失败 */
void    Finger_Init(void);
uint8_t Finger_GenImg(void);                                        /* 采集指纹图像 */
uint8_t Finger_GenChar(uint8_t buf_id);                             /* 生成特征到 CharBuffer1/2 */
uint8_t Finger_RegModel(void);                                      /* 合成模板(CharBuffer1+2 -> 模板) */
uint8_t Finger_Store(uint8_t buf_id, uint16_t page_id);             /* 存储模板到指纹库 */
uint8_t Finger_Search(uint8_t buf_id, uint16_t start_page, uint16_t count, uint16_t *page_id); /* 搜索指纹 */
uint8_t Finger_Delete(uint16_t page_id, uint16_t count);            /* 删除模板 */
uint8_t Finger_EmptyLib(void);                                      /* 清空指纹库 */
uint8_t Finger_Ping(void);                                          /* 握手(读系统参数), 0x00=模块在线 */
uint8_t Finger_SetSecLevel(uint8_t level);                          /* 设置安全等级 1~5 (越小越易识别) */

/* 调试辅助 */
uint32_t Finger_GetBaud(void);                                      /* 当前使用的波特率 */
uint8_t  Finger_IsFound(void);                                      /* 初始化时是否探测到模块, 1=是 */

/* 录入指纹(单次采集), 0=成功, 非0=失败 */
uint8_t Fingerprint_Enroll(uint8_t page_id);

#endif
