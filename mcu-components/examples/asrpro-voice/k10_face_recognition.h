#ifndef __K10_H
#define __K10_H

#include "stm32f1xx_hal.h"

/* K10 摄像头模块引脚定义 - USART1
 * STM32 PA9  -> K10 RX
 * STM32 PA10 <- K10 TX
 */
#define K10_TX_PORT        GPIOA
#define K10_TX_PIN         GPIO_PIN_9
#define K10_RX_PORT        GPIOA
#define K10_RX_PIN         GPIO_PIN_10
#define K10_BAUD_RATE      115200UL

/* K10 命令定义 */
#define K10_CMD_FACE_ENROLL     0x01    /* 录入人脸 */
#define K10_CMD_FACE_RECOGNIZE  0x02    /* 识别人脸 */
#define K10_CMD_FACE_DELETE     0x03    /* 删除人脸 */
#define K10_CMD_FACE_CLEAR      0x04    /* 清所有人脸 */

/* K10 返回状态 */
#define K10_STATUS_OK           0x00    /* 成功 */
#define K10_STATUS_FAIL         0x01    /* 失败 */
#define K10_STATUS_NO_FACE      0x02    /* 未检测到人脸 */
#define K10_STATUS_TIMEOUT      0x03    /* 超时 */
#define K10_STATUS_BUSY         0x04    /* 模块忙 */

/* K10 包头包尾 */
#define K10_HEADER              0xAA
#define K10_TAIL                0x55

/* 返回数据结构 */
typedef struct {
    uint8_t status;         /* 状态码 */
    uint8_t face_id;        /* 人脸ID */
    uint8_t confidence;     /* 置信度 */
} K10_Result;

/* 函数声明 */
void K10_Init(void);
void K10_FlushRx(void);
uint8_t K10_Ping(void);
uint8_t K10_EnrollFace(uint8_t face_id);
uint8_t K10_RecognizeFace(K10_Result *result);
uint8_t K10_DeleteFace(uint8_t face_id);
uint8_t K10_ClearAll(void);
uint8_t K10_SendCommand(uint8_t cmd, uint8_t param);
uint8_t K10_WaitResponse(K10_Result *result, uint16_t timeout_ms);

/* 开机 UART 接收自检页: OLED 实时显示 USART1(PA10) 收到的字节数与最后内容。
 * 永不返回, 复位退出。用于判断 PA10 这头到底有没有收到 K210 发来的数据。 */
void K10_RxSelfTest(void);

#endif
