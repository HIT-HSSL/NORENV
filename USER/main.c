#include "delay.h"
#include "key.h"
#include "led.h"
#include "lfs.h"
#include "ltdc.h"
#include "malloc.h"
#include "mpu.h"
#include "pcf8574.h"
#include "sdram.h"
#include "sys.h"
#include "usart.h"
// #include "usmart.h"
#include "w25qxx.h"
#include "nand.h"
#include "ftl.h"
#include "lfs_brigde.h"
#include "spiffs_brigde.h"
#include "jesfs_brigde.h"
#include "eHNFFS_brigde.h"
#include "ff.h"
#include "exfuns.h"

#include "nfvfs.h"
#include "benchmark.h"
#include "FreeRTOS.h"
#include "task.h"

#include "eHNFFS.h"

#define START_TASK_PRIO 1
#define START_STK_SIZE 128
TaskHandle_t StartTask_Handler;
void start_task(void *pvParameters);

#define FS_TASK_PRIO 2
#define FS_STK_SIZE 8192
TaskHandle_t FsTask_Handler;
void fs_task(void *p_arg);

#define LED_TASK_PRIO 10
#define LED_STK_SIZE 50
TaskHandle_t LedTask_Handler;
void led_task(void *p_arg);

void start_task(void *pvParameters)
{
	taskENTER_CRITICAL();

	xTaskCreate((TaskFunction_t)fs_task,
				(const char *)"fs_task",
				(uint16_t)FS_STK_SIZE,
				(void *)NULL,
				(UBaseType_t)FS_TASK_PRIO,
				(TaskHandle_t *)&FsTask_Handler);

	xTaskCreate((TaskFunction_t)led_task,
				(const char *)"led_task",
				(uint16_t)LED_STK_SIZE,
				(void *)NULL,
				(UBaseType_t)LED_TASK_PRIO,
				(TaskHandle_t *)&LedTask_Handler);

	vTaskDelete(StartTask_Handler);
	taskEXIT_CRITICAL();
}

void led_task(void *pvParameters)
{
	while (1)
	{
		LED0_Toggle;
		delay_ms(200);
	}
}

extern eHNFFS_t eHNFFS;
extern const struct eHNFFS_config eHNFFS_cfg;

void fs_task(void *p_arg)
{
	printf("erase begin\r\n");
	uint32_t start = (uint32_t)xTaskGetTickCount();
	W25QXX_Erase_Chip();
	uint32_t end = (uint32_t)xTaskGetTickCount();
	printf("erase end\r\n");
	// printf("Erase chip time is %u\r\n", end - start);

	// int heap_size1 = xPortGetFreeHeapSize();
	// printf("\r\n\r\n\r\n");

	file_prog_test("eHNFFS", 2000);

	// random_write_test("spiffs");
	// random_read_test("spiffs");

	// start = (uint32_t)xTaskGetTickCount();
	// int size = 1;
	// for (int i = 0; i < 13; i++)
	// {
	// 	raw_sctor_test(1000, size);
	// 	size = size * 2;
	// }
	// end = (uint32_t)xTaskGetTickCount();
	// printf("Total sector test time is %u\r\n", end - start);

	// busybox_test("eHNFFS");
	// new_operation_test("littlefs");

	// int heap_size2 = xPortGetMinimumEverFreeHeapSize();
	// int stack_size = (int32_t)uxTaskGetStackHighWaterMark(NULL) * 4;
	// printf("\r\n\r\n\r\n");
	// printf("heap and stack usage information is: %d, %d, %d\r\n", heap_size1, heap_size2, stack_size);

	vTaskDelete(FsTask_Handler);
}

void board_init(void)
{
	Cache_Enable();
	HAL_Init();
	Stm32_Clock_Init(160, 5, 2, 4);
	delay_init(400);
	uart_init(115200);
	// usmart_dev.init(200);
	LED_Init();
	KEY_Init();
	SDRAM_Init();
	W25QXX_Init();
	my_mem_init(SRAMIN);
	my_mem_init(SRAMEX);
	my_mem_init(SRAM12);
	my_mem_init(SRAM4);
	my_mem_init(SRAMDTCM);
	my_mem_init(SRAMITCM);

	exfuns_init();
}

void fs_registration(void)
{
	register_nfvfs("littlefs", &lfs_ops, NULL);
	register_nfvfs("spiffs", &spiffs_ops, NULL);
	register_nfvfs("eHNFFS", &eHNFFS_ops, NULL);
	// register_nfvfs("jesfs", &jesfs_ops, NULL);
}

// extern u8 usmart_sys_cmd_exe(u8 *str);
// void show_help()
//{
//	usmart_sys_cmd_exe("help");
// }

int main(void)
{
	board_init();
	fs_registration();

	xTaskCreate(
		(TaskFunction_t)start_task,
		(const char *)"start_task",
		(uint16_t)START_STK_SIZE,
		(void *)NULL,
		(UBaseType_t)START_TASK_PRIO,
		(TaskHandle_t *)&StartTask_Handler);

	vTaskStartScheduler();

	while (1)
	{
		printf("It's me?\r\n");
	}
}
