#ifndef __FINGERPRINT_H
#define __FINGERPRINT_H
#include "sys.h"

/* =========================================================================
 * AS608 / FPM10A 应答码定义
 * ========================================================================= */
#define ACK_SUCCESS         0x00    /* 操作成功                 */
#define ACK_FAIL            0x01    /* 操作失败                 */
#define ACK_NO_FINGER       0x02    /* 未检测到手指             */
#define ACK_GET_IMG_ERR     0x03    /* 采集图像失败             */
#define ACK_IMG_MESSY       0x06    /* 图像太乱                 */
#define ACK_IMG_FEW_POINT   0x07    /* 特征点太少               */
#define ACK_NO_MATCH        0x08    /* 指纹不匹配               */
#define ACK_NO_FOUND        0x09    /* 未找到匹配指纹           */
#define ACK_COMBINE_ERR     0x0A    /* 特征合并失败             */
#define ACK_ADDR_OVER       0x0B    /* 地址超出范围             */
#define ACK_READ_ERR        0x0C    /* 读取模板库失败           */
#define ACK_UPLOAD_ERR      0x0D    /* 上传特征失败             */
#define ACK_RECV_ERR        0x0E    /* 接收数据包失败           */
#define ACK_UPLOAD_IMG_ERR  0x0F    /* 上传图像失败             */

/* =========================================================================
 * 模块参数
 * ========================================================================= */
#define AS680_MAX_TEMPLATES  80     /* 最大模板数量             */

/* =========================================================================
 * 全局变量
 * ========================================================================= */
extern u8 FPM10A_RECEIVE_BUFFER[32];

/* =========================================================================
 * 函数声明
 * ========================================================================= */
void Fingerprint_Init(void);
u8   Fingerprint_HandShake(void);
u8   Fingerprint_AutoBaud(void);
u8   Fingerprint_GetTemplateCount(u16 *count);

/* 底层驱动 */
u8 FPM10A_Get_Image(void);
u8 FPM10A_Img_To_Buffer1(void);
u8 FPM10A_Img_To_Buffer2(void);
u8 FPM10A_Reg_Model(void);
u8 FPM10A_Save_Finger(u16 finger_id);
u8 FPM10A_Search_Finger(void);
u8 FPM10A_Delete_All(void);

/* 兼容旧接口（main.c 无需修改） */
u8 Fingerprint_GetImage(void);
u8 Fingerprint_GenChar(u8 BufferID);
u8 Fingerprint_Match(void);
u8 Fingerprint_RegModel(void);
u8 Fingerprint_StoreChar(u8 BufferID, u16 PageID);
u8 Fingerprint_DeleteChar(u16 PageID, u16 Count);
u8 Fingerprint_Empty(void);
u8 Fingerprint_Search(u8 BufferID, u16 StartPage, u16 PageNum,
                      u16 *PageID, u16 *Score);

#endif
