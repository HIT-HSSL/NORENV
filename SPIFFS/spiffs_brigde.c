#include "spiffs_brigde.h"
#include "nfvfs.h"
#include "spiffs.h"
#include "w25qxx.h"
#include "delay.h"

#define LOG_PAGE_SIZE       256
#define W25Q256_SECTOR_SIZE 4096
#define W25Q256_SIZE        (W25Q256_SECTOR_SIZE * W25Q256_NUM_GRAN)

spiffs fs;

static uint8_t spiffs_work_buf[LOG_PAGE_SIZE * 2];
static uint8_t spiffs_fds[32 * 4];
static uint8_t spiffs_cache_buf[(LOG_PAGE_SIZE + 32) * 4];

int W25Qxx_readspiffs(u32_t addr, u32_t size, u8_t *dst)
{
    if (addr >= W25Q256_SIZE) {
        return SPIFFS_ERR_IO;
    }

    W25QXX_Read(dst, addr, size);

    return SPIFFS_OK;
}

int W25Qxx_writespiffs(u32_t addr, u32_t size, u8_t *src)
{
    if (addr >= W25Q256_SIZE) {
        return SPIFFS_ERR_IO;
    }

    W25QXX_Write(src, addr, size);

    return SPIFFS_OK;
}

int W25Qxx_erasespiffs(u32_t addr, u32_t size)
{
    if (addr >= W25Q256_SIZE) // error
    {
        return SPIFFS_ERR_IO;
    }

    W25QXX_Erase_Sector(addr);
    return SPIFFS_OK;
}

int spiffs_mount_wrp()
{
    int err, tries = 0;
    spiffs_config cfg;
    cfg.phys_size = W25Q256_SIZE;              // use all spi flash
    cfg.phys_addr = 0;                         // start spiffs at start of spi flash
    cfg.phys_erase_block = W25Q256_ERASE_GRAN; // according to datasheet
    cfg.log_block_size = 65536;                // let us not complicate things
    cfg.log_page_size = LOG_PAGE_SIZE;         // as we said

    cfg.hal_read_f = W25Qxx_readspiffs;
    cfg.hal_write_f = W25Qxx_writespiffs;
    cfg.hal_erase_f = W25Qxx_erasespiffs;
    
    /* Do not config USE_MAGIC */
    err = SPIFFS_mount(&fs,
                       &cfg,
                       spiffs_work_buf,
                       spiffs_fds,
                       sizeof(spiffs_fds),
                       spiffs_cache_buf,
                       sizeof(spiffs_cache_buf),
                       NULL);
    while (err) {
        printf("try clean and remount %d\r\n", err);
        SPIFFS_format(&fs);
        err = SPIFFS_mount(&fs,
                           &cfg,
                           spiffs_work_buf,
                           spiffs_fds,
                           sizeof(spiffs_fds),
                           spiffs_cache_buf,
                           sizeof(spiffs_cache_buf),
                           NULL);
        delay_ms(1000);
        tries++;
        if (tries >= 2) {
            printf("mount fail is %d\r\n", err);
            return -1;
        }
    }

    printf("mount res: %i\r\n", err);

    return err;
}

int spiffs_unmount_wrp()
{
    SPIFFS_unmount(&fs);
    return 0;
}

int spiffs_open_wrp(const char *path, int flags, int mode, struct nfvfs_context *context)
{
    int fentry = *(int *)context->in_data;
    int spiffs_flags = 0;
    int ret = 0;
    spiffs_file *fd;
    spiffs_DIR *dir;

    spiffs_flags |= (IF_O_CREAT(flags) ? SPIFFS_O_CREAT : 0);
    spiffs_flags |= (IF_O_EXCL(flags) ? SPIFFS_O_EXCL : 0);
    spiffs_flags |= (IF_O_TRUNC(flags) ? SPIFFS_O_TRUNC : 0);
    spiffs_flags |= (IF_O_APPEND(flags) ? SPIFFS_O_APPEND : 0);
    spiffs_flags |= (IF_O_RDONLY(flags) ? SPIFFS_O_RDONLY : 0);
    spiffs_flags |= (IF_O_WRONLY(flags) ? SPIFFS_O_WRONLY : 0);
    spiffs_flags |= (IF_O_RDWR(flags) ? SPIFFS_RDWR : 0);

    if (S_IFREG(mode)) {
        fd = (spiffs_file *)pvPortMalloc(sizeof(spiffs_file));
        ret = SPIFFS_open(&fs, path, spiffs_flags, 0);
        if (ret < 0) {
            return ret;
        }
        *fd = ret;
        context->out_data = fd;
    } else {
        dir = (spiffs_DIR *)pvPortMalloc(sizeof(spiffs_DIR));
        SPIFFS_opendir(&fs, path, dir);
        context->out_data = dir;
    }

    return fentry;
}

int spiffs_close_wrp(int fd)
{
    int ret = 0;
    struct nfvfs_fentry *entry = (struct nfvfs_fentry *)ftable_get_entry(fd);
    if (entry == NULL) {
        return -1;
    }

    if (S_IFREG(entry->mode)) {
        ret = SPIFFS_close(&fs, *(spiffs_file *)entry->f);
        vPortFree(entry->f);
    } else {
        SPIFFS_closedir((spiffs_DIR *)entry->f);
        vPortFree(entry->f);
    }

    return ret;
}

int spiffs_read_wrp(int fd, void *buf, uint32_t size)
{
    int ret = 0;
    struct nfvfs_fentry *entry = (struct nfvfs_fentry *)ftable_get_entry(fd);
    if (entry == NULL) {
        return -1;
    }

    if (S_IFREG(entry->mode)) {
        ret = SPIFFS_read(&fs, *(spiffs_file *)entry->f, buf, size);
    } else {
        ret = -1;
    }

    return ret;
}

int spiffs_write_wrp(int fd, void *buf, uint32_t size)
{
    int ret = 0;
    struct nfvfs_fentry *entry = (struct nfvfs_fentry *)ftable_get_entry(fd);
    if (entry == NULL) {
        return -1;
    }

    if (S_IFREG(entry->mode)) {
        ret = SPIFFS_write(&fs, *(spiffs_file *)entry->f, buf, size);
    } else {
        ret = -1;
    }

    return ret;
}

int spiffs_lseek_wrp(int fd, uint32_t offset, int whence)
{
    int ret = 0;
    struct nfvfs_fentry *entry = (struct nfvfs_fentry *)ftable_get_entry(fd);
    int spiffs_whence = 0;
    
    if (entry == NULL) {
        return -1;
    }

    switch (whence)
    {
    case NFVFS_SEEK_CUR:
        spiffs_whence = SPIFFS_SEEK_CUR;    
        break;
    case NFVFS_SEEK_SET:
        spiffs_whence = SPIFFS_SEEK_SET;
        break;
    case NFVFS_SEEK_END:
        spiffs_whence = SPIFFS_SEEK_END;
        break;
    default:
        break;
    }

    if (S_IFREG(entry->mode)) {
        ret = SPIFFS_lseek(&fs, *(spiffs_file *)entry->f, offset, spiffs_whence);
    } else {
        ret = -1;
    }

    return ret;
}

struct nfvfs_operations spiffs_ops = {
    .mount = spiffs_mount_wrp,
    .unmount = spiffs_unmount_wrp,
    .open = spiffs_open_wrp,
    .close = spiffs_close_wrp,
    .read = spiffs_read_wrp,
    .write = spiffs_write_wrp,
    .lseek = spiffs_lseek_wrp,
};
