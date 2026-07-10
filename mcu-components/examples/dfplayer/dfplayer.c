#include "stm32f10x.h"                  // Device header
#include "DFPlayer.h"
#include "Delay.h"

/*DFPlayer当前音量*/
static uint8_t DFPlayer_Volume = 15;

/**
  * 函    数：USART2发送一个字节
  * 参    数：Byte 要发送的字节
  * 返 回 值：无
  */
static void DFPlayer_SendByte(uint8_t Byte)
{
	USART_SendData(USART2, Byte);
	while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
}

/**
  * 函    数：向DFPlayer发送命令
  * 参    数：cmd  命令字节
  *           para 参数（16位）
  * 返 回 值：无
  * 说    明：DFPlayer Mini通信协议：
  *           起始 0x7E, 版本 0xFF, 长度 0x06, 命令, 反馈 0x00,
  *           参数高 paraH, 参数低 paraL, 校验高, 校验低, 结束 0xEF
  */
static void DFPlayer_SendCmd(uint8_t cmd, uint16_t para)
{
	uint8_t paraH = (uint8_t)(para >> 8);
	uint8_t paraL = (uint8_t)(para & 0xFF);
	
	/*计算校验和*/
	int16_t checksum = 0 - (0xFF + 0x06 + cmd + 0x00 + paraH + paraL);
	uint8_t checkH = (uint8_t)(checksum >> 8);
	uint8_t checkL = (uint8_t)(checksum & 0xFF);
	
	/*发送10字节命令帧*/
	DFPlayer_SendByte(0x7E);    //起始
	DFPlayer_SendByte(0xFF);    //版本
	DFPlayer_SendByte(0x06);    //长度
	DFPlayer_SendByte(cmd);     //命令
	DFPlayer_SendByte(0x00);    //反馈：不需要
	DFPlayer_SendByte(paraH);   //参数高字节
	DFPlayer_SendByte(paraL);   //参数低字节
	DFPlayer_SendByte(checkH);  //校验高字节
	DFPlayer_SendByte(checkL);  //校验低字节
	DFPlayer_SendByte(0xEF);    //结束
	
	Delay_ms(30);               //等待DFPlayer处理
}

/**
  * 函    数：DFPlayer Mini初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：使用USART2（PA2-TX, PA3-RX），波特率9600
  */
void DFPlayer_Init(void)
{
	/*开启时钟*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	
	/*PA2-USART2_TX*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/*PA3-USART2_RX*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/*USART初始化*/
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART2, &USART_InitStructure);
	
	/*使能USART2*/
	USART_Cmd(USART2, ENABLE);
	
	Delay_ms(200);  //等待DFPlayer上电稳定
	
	/*设置播放源为TF卡*/
	DFPlayer_SendCmd(0x09, 0x0002);
	Delay_ms(200);
	
	/*设置默认音量（0~30）*/
	DFPlayer_Volume = 15;
	DFPlayer_SendCmd(0x06, DFPlayer_Volume);
	
	/*设置默认EQ为Normal*/
	DFPlayer_SendCmd(0x07, EQ_NORMAL);
}

/**
  * 函    数：播放（恢复播放）
  * 参    数：无
  * 返 回 值：无
  */
void DFPlayer_Play(void)
{
	DFPlayer_SendCmd(0x0D, 0x0000);
}

/**
  * 函    数：暂停播放
  * 参    数：无
  * 返 回 值：无
  */
void DFPlayer_Pause(void)
{
	DFPlayer_SendCmd(0x0E, 0x0000);
}

/**
  * 函    数：停止播放
  * 参    数：无
  * 返 回 值：无
  */
void DFPlayer_Stop(void)
{
	DFPlayer_SendCmd(0x16, 0x0000);
}

/**
  * 函    数：下一首
  * 参    数：无
  * 返 回 值：无
  */
void DFPlayer_Next(void)
{
	DFPlayer_SendCmd(0x01, 0x0000);
}

/**
  * 函    数：上一首
  * 参    数：无
  * 返 回 值：无
  */
void DFPlayer_Previous(void)
{
	DFPlayer_SendCmd(0x02, 0x0000);
}

/**
  * 函    数：设置音量
  * 参    数：vol 音量值，范围0~30
  * 返 回 值：无
  */
void DFPlayer_SetVolume(uint8_t vol)
{
	if (vol > 30) vol = 30;
	DFPlayer_Volume = vol;
	DFPlayer_SendCmd(0x06, vol);
}

/**
  * 函    数：音量增大
  * 参    数：无
  * 返 回 值：无
  */
void DFPlayer_VolumeUp(void)
{
	if (DFPlayer_Volume < 30)
	{
		DFPlayer_Volume += 3;
		if (DFPlayer_Volume > 30) DFPlayer_Volume = 30;
	}
	DFPlayer_SendCmd(0x06, DFPlayer_Volume);
}

/**
  * 函    数：音量减小
  * 参    数：无
  * 返 回 值：无
  */
void DFPlayer_VolumeDown(void)
{
	if (DFPlayer_Volume >= 3)
	{
		DFPlayer_Volume -= 3;
	}
	else
	{
		DFPlayer_Volume = 0;
	}
	DFPlayer_SendCmd(0x06, DFPlayer_Volume);
}

/**
  * 函    数：设置均衡器
  * 参    数：eq 均衡器模式，0~5（Normal/Pop/Rock/Jazz/Classic/Bass）
  * 返 回 值：无
  */
void DFPlayer_SetEQ(uint8_t eq)
{
	if (eq > 5) eq = 0;
	DFPlayer_SendCmd(0x07, eq);
}

/**
  * 函    数：播放指定文件夹中的指定文件
  * 参    数：folder 文件夹编号（01~99）
  *           file   文件编号（001~255）
  * 返 回 值：无
  * 说    明：TF卡需按文件夹/文件编号组织，如 /01/001.mp3
  */
void DFPlayer_PlayFolder(uint8_t folder, uint8_t file)
{
	uint16_t para = ((uint16_t)folder << 8) | file;
	DFPlayer_SendCmd(0x0F, para);
}

/**
  * 函    数：播放指定曲目编号
  * 参    数：track 曲目编号（1~3000）
  * 返 回 值：无
  */
void DFPlayer_PlayTrack(uint16_t track)
{
	DFPlayer_SendCmd(0x03, track);
}

/**
  * 函    数：获取当前音量
  * 参    数：无
  * 返 回 值：当前音量值
  */
uint8_t DFPlayer_GetVolume(void)
{
	return DFPlayer_Volume;
}
