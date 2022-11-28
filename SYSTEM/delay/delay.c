#include "delay.h"
#include "sys.h"
////////////////////////////////////////////////////////////////////////////////// 	 
//���ʹ��ucos,����������ͷ�ļ�����.
#include "FreeRTOS.h"					//ucos ʹ��	  

//----------------------------------ADD-------------------------------------
#include "task.h"



//////////////////////////////////////////////////////////////////////////////////  
//������ֻ��ѧϰʹ�ã�δ��������ɣ��������������κ���;
//ALIENTEK STM32H7������
//ʹ��SysTick����ͨ����ģʽ���ӳٽ��й���(֧��ucosii)
//����delay_us,delay_ms
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//��������:2017/6/8
//�汾��V1.0
//��Ȩ���У�����ؾ���
//Copyright(C) ������������ӿƼ����޹�˾ 2014-2024
//All rights reserved
//********************************************************************************
//�޸�˵��
////////////////////////////////////////////////////////////////////////////////// 

static u32 fac_us=0;							//us��ʱ������	
static u16 fac_ms=0;							//ms��ʱ������,��os��,����ÿ�����ĵ�ms��




//-------------------------------------ADD---------------------------------------------
typedef BaseType_t (*TaskHookFunction_t)( void * );
extern void xPortSysTickHandler(void);
extern BaseType_t xTaskGetSchedulerState( void );
#define taskSCHEDULER_NOT_STARTED	( ( BaseType_t ) 1 )

void SysTick_Handler(void)
{ 
	if(xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED)//??????
	{
		xPortSysTickHandler();
	}
}
			   				

void delay_init(u16 SYSCLK)
{
	u32 reload;
	HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
	fac_us=SYSCLK; 
	reload=SYSCLK; 
	reload*=1000000/configTICK_RATE_HZ; //?? configTICK_RATE_HZ ??????
	fac_ms=1000/configTICK_RATE_HZ; //?? OS ?????????
	SysTick->CTRL|=SysTick_CTRL_TICKINT_Msk;//?? SYSTICK ??
	SysTick->LOAD=reload; //? 1/configTICK_RATE_HZ ???
	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk; //?? SYSTICK
}

void delay_us(u32 nus)
{
	u32 ticks;
	u32 told,tnow,tcnt=0;
	u32 reload=SysTick->LOAD; //LOAD ?? 
	ticks=nus*fac_us; //??????
	told=SysTick->VAL; //?????????
	while(1)
	{
		tnow=SysTick->VAL;
		if(tnow!=told)
		{
			//?????? SYSTICK ?????????????. 
			if(tnow<told)tcnt+=told-tnow;
			else tcnt+=reload-tnow+told; 
			told=tnow;
			if(tcnt>=ticks)break; //????/????????,???.
		} 
	}; 
} 

void delay_ms(u32 nms)
{
	if(xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED)//??????
	{
		if(nms>=fac_ms) //??????? OS ???????
		{ 
			vTaskDelay(nms/fac_ms); //FreeRTOS ??
		}
		nms%=fac_ms; //OS ?????????????,
	}
	delay_us((u32)(nms*1000)); //??????
}

void delay_xms(u32 nms)
{
	u32 i;
	for(i=0;i<nms;i++) delay_us(1000);
}
			 



































