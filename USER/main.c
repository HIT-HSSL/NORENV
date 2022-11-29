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
#include "usmart.h"
#include "w25qxx.h"
#include "lfs_brigde.h"
#include "spiffs_brigde.h"
#include "nfvfs.h"

void board_init(void)
{
    Cache_Enable();
    HAL_Init();
    Stm32_Clock_Init(160, 5, 2, 4);
    delay_init(400);
    uart_init(115200);
    usmart_dev.init(200);
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
}


void fs_registration(void) 
{
    register_nfvfs("littlefs", &lfs_ops, NULL);
    register_nfvfs("spiffs", &spiffs_ops, NULL);
}

extern u8 usmart_sys_cmd_exe(u8 *str);
void show_help()
{
    usmart_sys_cmd_exe("help");    
}

int main(void)
{
    board_init();
    fs_registration();
    show_help();
    
    while (1) {
        LED0_Toggle; // DS0иак╦
        delay_ms(2000);
    }
}
