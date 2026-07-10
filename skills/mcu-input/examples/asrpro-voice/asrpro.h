#ifndef __ASRPRO_H
#define __ASRPRO_H

#include "stm32f10x.h"

/*ASRPro语音指令定义*/
#define ASRCMD_NONE         0x00    //无指令
#define ASRCMD_WAKE         0xA1    //唤醒词 "你好北鼻"
#define ASRCMD_PLAY         0xA2    //播放音乐
#define ASRCMD_PAUSE        0xA3    //暂停
#define ASRCMD_NEXT         0xA4    //下一首
#define ASRCMD_PREV         0xA5    //上一首
#define ASRCMD_VOL_UP       0xA6    //音量增大
#define ASRCMD_VOL_DOWN     0xA7    //音量减小
#define ASRCMD_EQ_VOCAL     0xA8    //臻享人声模式
#define ASRCMD_EQ_DANCE     0xA9    //动感模式
#define ASRCMD_SONG_QHC     0xAA    //播放青花瓷
#define ASRCMD_SONG_ROCK    0xAB    //来点摇滚
#define ASRCMD_LED_ON       0xAC    //打开灯光
#define ASRCMD_LED_OFF      0xAD    //关闭灯光
#define ASRCMD_SHUTDOWN     0xAE    //关机
#define ASRCMD_SONG_QT      0xAF    //播放晴天
#define ASRCMD_SONG_GHSY    0xB0    //播放光辉岁月
#define ASRCMD_LED_BRIGHT   0xB1    //灯光调亮
#define ASRCMD_LED_DIM      0xB2    //灯光调暗
#define ASRCMD_EQ_NORMAL    0xB3    //普通模式

void ASRPro_Init(void);
uint8_t ASRPro_GetCmd(void);
void ASRPro_IRQHandler(void);

#endif
