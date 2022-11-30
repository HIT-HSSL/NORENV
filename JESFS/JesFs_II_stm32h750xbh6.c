#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "jesfs.h"
#include "jesfs_int.h"
#include "spi.h"
#include "w25qxx.h"
#include "delay.h"

#ifdef __STM32H750xx_H

int16_t sflash_spi_init(void)
{
    /* We've done this in board_init */
    return 0;
}

void sflash_spi_close(void)
{
    return;
}

void sflash_spi_read(uint8_t *buf, uint16_t len)
{
    SPI2_ReadBytes(buf, len);
}

void sflash_spi_write(const uint8_t *buf, uint16_t len)
{
    SPI2_WriteBytes(buf, len);
}

void sflash_select(void)
{
    W25QXX_CS(0);
}

void sflash_deselect(void)
{
    W25QXX_CS(1);
}

void sflash_wait_usec(uint32_t usec)
{
    delay_us(usec);
}

#endif
