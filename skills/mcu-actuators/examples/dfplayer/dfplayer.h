#ifndef __DFPLAYER_H
#define __DFPLAYER_H

#include "stm32f10x.h"

/*DFPlayer均衡器模式*/
#define EQ_NORMAL   0
#define EQ_POP      1
#define EQ_ROCK     2
#define EQ_JAZZ     3
#define EQ_CLASSIC  4
#define EQ_BASS     5

/*DFPlayer播放状态*/
#define DFPLAYER_STATUS_STOP    0
#define DFPLAYER_STATUS_PLAY    1
#define DFPLAYER_STATUS_PAUSE   2

void DFPlayer_Init(void);
void DFPlayer_Play(void);
void DFPlayer_Pause(void);
void DFPlayer_Stop(void);
void DFPlayer_Next(void);
void DFPlayer_Previous(void);
void DFPlayer_SetVolume(uint8_t vol);
void DFPlayer_VolumeUp(void);
void DFPlayer_VolumeDown(void);
void DFPlayer_SetEQ(uint8_t eq);
void DFPlayer_PlayFolder(uint8_t folder, uint8_t file);
void DFPlayer_PlayTrack(uint16_t track);
uint8_t DFPlayer_GetVolume(void);

#endif
