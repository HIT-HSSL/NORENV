#include "sys.h"
#include "delay.h"
#include "usart.h" 
#include "led.h"
#include "key.h"
#include "ltdc.h"
#include "sdram.h"
#include "usmart.h"
#include "pcf8574.h"
#include "mpu.h"
#include "malloc.h"
#include "w25qxx.h"
#include "lfs.h"

lfs_t lfs; 
lfs_file_t file;
/*
 * @brief littlefs read interface
 * @param [in] c lfs_config数据结构
 * @param [in] block 要读的块
 * @param [in] off 在当前块的偏移
 * @param [out] buffer 读取到的数据
 * @param [in] size 要读取的字节数
 * @return 0 成功 <0 错误
 * @note littlefs 一定不会存在跨越块存储的情况
 */
int W25Qxx_readLittlefs(const struct lfs_config *c, lfs_block_t block,
				lfs_off_t off, void *buffer, lfs_size_t size)
{
	
	if(block >= W25Q256_NUM_GRAN) //error
	{
		return LFS_ERR_IO;
	}
	
	W25QXX_Read(buffer,block*W25Q256_ERASE_GRAN + off, size);
	
	return LFS_ERR_OK;
				
}



/*
 * @brief littlefs write interface
 * @param [in] c lfs_config数据结构
 * @param [in] block 要读的块
 * @param [in] off 在当前块的偏移
 * @param [out] buffer 读取到的数据
 * @param [in] size 要读取的字节数
 * @return 0 成功 <0 错误
 * @note littlefs 一定不会存在跨越块存储的情况
 */
int W25Qxx_writeLittlefs(const struct lfs_config *c, lfs_block_t block,
				lfs_off_t off,void *buffer, lfs_size_t size)
{
	
	if(block >= W25Q256_NUM_GRAN) //error
	{
		return LFS_ERR_IO;
	}
	
	W25QXX_Write_NoCheck(buffer,block*W25Q256_ERASE_GRAN + off, size);
	
	return LFS_ERR_OK;
				
}



/*
 * @brief littlefs 擦除一个块
 * @param [in] c lfs_config数据结构
 * @param [in] block 要擦出的块
 * @return 0 成功 <0 错误
 */
int W25Qxx_eraseLittlefs(const struct lfs_config *c, lfs_block_t block)
{
	
	if(block >= W25Q256_NUM_GRAN) //error
	{
		return LFS_ERR_IO;
	}
	
	//擦除扇区
	W25QXX_Erase_Sector(block);
	return  LFS_ERR_OK;

}

int W25Qxx_syncLittlefs(const struct lfs_config *c )
{
	return  LFS_ERR_OK;

}

const struct lfs_config cfg = {
		// block device operations
		.read  = W25Qxx_readLittlefs,
		.prog  = W25Qxx_writeLittlefs,
		.erase = W25Qxx_eraseLittlefs,
		.sync  = W25Qxx_syncLittlefs,

		// block device configuration
		.read_size = 256,
		.prog_size = 256,
		.block_size = W25Q256_ERASE_GRAN,
		.block_count = W25Q256_NUM_GRAN,
		.cache_size = 512,
		.lookahead_size = 512,
		.block_cycles = 500,	
};


int main(void)
{
	int err = -1;
	Cache_Enable(); //?? L1-Cache
	HAL_Init(); //??? HAL ?
	Stm32_Clock_Init(160,5,2,4); //????,400Mhz 
	delay_init(400); //?????
	uart_init(115200); //?????
	
	usmart_dev.init(200); 		    		//初始化USMART	
	LED_Init();								//初始化LED
	KEY_Init();								//初始化按键
	SDRAM_Init();                   		//初始化SDRAM
	W25QXX_Init();					//初始化W25Q256
	my_mem_init(SRAMIN);			//初始化内部内存池(AXI)
	my_mem_init(SRAMEX);			//初始化外部内存池(SDRAM)
	my_mem_init(SRAM12);			//初始化SRAM12内存池(SRAM1+SRAM2)
	my_mem_init(SRAM4);				//初始化SRAM4内存池(SRAM4)
	my_mem_init(SRAMDTCM);			//初始化DTCM内存池(DTCM)
	my_mem_init(SRAMITCM);			//初始化ITCM内存池(ITCM)


	

//	while(W25QXX_ReadID()!=W25Q256)								//检测不到W25Q256
//	{
//		printf("W25Q256 Check Failed!\r\n");
//		delay_ms(500);
//		printf("Please Check!        \r\n");
//		delay_ms(500);
//		LED0_Toggle;//DS0闪烁
//	}
	
	while(1)
	{
//		printf("begin\r\n"); 
		LED0_Toggle;//DS0闪烁
		
		err = lfs_mount(&lfs, &cfg);	 
		
		//-----------------------------------------
		//想知道lfs_block_t、LFS_BLOCK_NULL、LFS_BLOCK_INLINE都是啥
		//(lfs_gstate_t){0}啥意思
		
		while( err )
		{
			lfs_format(&lfs, &cfg);
			err = lfs_mount(&lfs, &cfg);
			printf("mount fail is %d\r\n", err);
			delay_ms(1000);
		}
			
		uint32_t boot_count = 0;
		lfs_file_open(&lfs, &file, "boot_count", LFS_O_RDWR | LFS_O_CREAT);
		lfs_file_read(&lfs, &file, &boot_count, sizeof(boot_count));
		
		// update boot count
		boot_count += 1;
		lfs_file_rewind(&lfs, &file);  // seek the file to begin
		lfs_file_write(&lfs, &file, &boot_count, sizeof(boot_count));
		// remember the storage is not updated until the file is closed successfully
		lfs_file_close(&lfs, &file);

		// release any resources we were using
		lfs_unmount(&lfs);

		// print the boot count
		printf("boot_count: %d\r\n", boot_count);

		delay_ms(2000);
//		printf("delay over\r\n"); 
	}
	
//	while(1)
//	{
//		// 有了mount这些玩意才不能跑的
//		printf("?????why why why\r\n"); 
//		LED0_Toggle;
//		delay_ms(200);
//	}
}
