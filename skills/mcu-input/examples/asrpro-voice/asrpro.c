#include "stm32f10x.h"                  // Device header
#include "ASRPro.h"

/*ASRPro串口接收缓冲*/
static uint8_t ASRPro_RxData;
static uint8_t ASRPro_RxFlag;

/**
  * 函    数：ASRPro语音模块初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：使用USART1（PA10-RX）接收ASRPro发送的语音指令
  *           波特率9600，中断接收
  */
void ASRPro_Init(void)
{
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	/*GPIO初始化 PA10-USART1_RX*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/*PA9-USART1_TX (可选，用于向ASRPro发送反馈)*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/*USART初始化*/
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART1, &USART_InitStructure);
	
	/*开启USART1接收中断*/
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	
	/*NVIC配置*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init(&NVIC_InitStructure);
	
	/*使能USART1*/
	USART_Cmd(USART1, ENABLE);
}

/**
  * 函    数：获取ASRPro语音指令
  * 参    数：无
  * 返 回 值：语音指令码，0x00表示无指令
  * 说    明：读取后自动清除标志位
  */
uint8_t ASRPro_GetCmd(void)
{
	if (ASRPro_RxFlag == 1)
	{
		ASRPro_RxFlag = 0;
		return ASRPro_RxData;
	}
	return ASRCMD_NONE;
}

/**
  * 函    数：USART1中断处理（供stm32f10x_it.c调用）
  * 参    数：无
  * 返 回 值：无
  */
void ASRPro_IRQHandler(void)
{
	if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
	{
		ASRPro_RxData = USART_ReceiveData(USART1);
		ASRPro_RxFlag = 1;
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}
}
