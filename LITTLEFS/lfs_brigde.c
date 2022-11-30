/**
 * Copyright (C) 2022 Deadpool, Hao Huang
 * 
 * This file is part of NORENV.
 * 
 * NORENV is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * NORENV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with NORENV.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "lfs_brigde.h"
#include "delay.h"
#include "lfs.h"
#include "nfvfs.h"
#include "w25qxx.h"

lfs_t lfs;

int W25Qxx_readlfs(const struct lfs_config *c, lfs_block_t block,
                        lfs_off_t off, void *buffer, lfs_size_t size)
{
    if (block >= W25Q256_NUM_GRAN) // error
    {
        return LFS_ERR_IO;
    }

    W25QXX_Read(buffer, block * W25Q256_ERASE_GRAN + off, size);

    return LFS_ERR_OK;
}

int W25Qxx_writelfs(const struct lfs_config *c, lfs_block_t block,
                         lfs_off_t off, void *buffer, lfs_size_t size)
{
    if (block >= W25Q256_NUM_GRAN) // error
    {
        return LFS_ERR_IO;
    }

    W25QXX_Write(buffer, block * W25Q256_ERASE_GRAN + off, size);

    return LFS_ERR_OK;
}

int W25Qxx_eraselfs(const struct lfs_config *c, lfs_block_t block)
{
    if (block >= W25Q256_NUM_GRAN) // error
    {
        return LFS_ERR_IO;
    }

    W25QXX_Erase_Sector(block);
    return LFS_ERR_OK;
}

int W25Qxx_synclfs(const struct lfs_config *c)
{
    return LFS_ERR_OK;
}

const struct lfs_config lfs_cfg = {
    // block device operations
    .read = W25Qxx_readlfs,
    .prog = W25Qxx_writelfs,
    .erase = W25Qxx_eraselfs,
    .sync = W25Qxx_synclfs,

    // block device configuration
    .read_size = 256,
    .prog_size = 256,
    .block_size = W25Q256_ERASE_GRAN,
    .block_count = W25Q256_NUM_GRAN,
    .cache_size = 512,
    .lookahead_size = 512,
    .block_cycles = 500,
};

int lfs_mount_wrp()
{
    int err = -1;
    int tries = 0;

    err = lfs_mount(&lfs, &lfs_cfg);
    while (err) {
        lfs_format(&lfs, &lfs_cfg);
        err = lfs_mount(&lfs, &lfs_cfg);
        printf("mount fail is %d\r\n", err);
        delay_ms(1000);
        tries++;
        if (tries >= 2) {
            return -1;
        }
    }

    return 0;
}

int lfs_unmount_wrp()
{
    return lfs_unmount(&lfs);
}

int lfs_open_wrp(const char *path, int flags, int mode, struct nfvfs_context *context)
{
    struct lfs_file *file;
    struct lfs_dir *dir;
    int fentry = *(int *)context->in_data;
    int lfs_flags = 0;
    int ret = 0;

    lfs_flags |= (IF_O_CREAT(flags) ? LFS_O_CREAT : 0);
    lfs_flags |= (IF_O_EXCL(flags) ? LFS_O_EXCL : 0);
    lfs_flags |= (IF_O_TRUNC(flags) ? LFS_O_TRUNC : 0);
    lfs_flags |= (IF_O_APPEND(flags) ? LFS_O_APPEND : 0);
    lfs_flags |= (IF_O_RDONLY(flags) ? LFS_O_RDONLY : 0);
    lfs_flags |= (IF_O_WRONLY(flags) ? LFS_O_WRONLY : 0);
    lfs_flags |= (IF_O_RDWR(flags) ? LFS_O_RDWR : 0);

    if (S_IFREG(mode)) {
        file = (lfs_file_t *)pvPortMalloc(sizeof(lfs_file_t));
        ret = lfs_file_open(&lfs, file, path, lfs_flags);
        context->out_data = file;
    } else {
        dir = (lfs_dir_t *)pvPortMalloc(sizeof(lfs_dir_t));
        ret = lfs_dir_open(&lfs, dir, path);
        context->out_data = dir;
    }

    if (ret < 0) {
        return -1;
    }

    return fentry;
}

int lfs_close_wrp(int fd)
{
    struct nfvfs_fentry *entry = ftable_get_entry(fd);
    if (entry == NULL) {
        return -1;
    }
    if (S_IFREG(entry->mode)) {
        lfs_file_close(&lfs, (lfs_file_t *)entry->f);
    } else {
        lfs_dir_close(&lfs, (lfs_dir_t *)entry->f);
    }
    vPortFree(entry->f);
    return 0;
}

int lfs_read_wrp(int fd, void *buf, uint32_t size)
{
    struct nfvfs_fentry *entry = ftable_get_entry(fd);
    if (entry == NULL) {
        return -1;
    }
    if (S_IFREG(entry->mode)) {
        return lfs_file_read(&lfs, (lfs_file_t *)entry->f, (void *)buf, size);
    } else {
        return -1;
    }
}

int lfs_write_wrp(int fd, void *buf, uint32_t size)
{
    struct nfvfs_fentry *entry = ftable_get_entry(fd);
    if (entry == NULL) {
        return -1;
    }
    if (S_IFREG(entry->mode)) {
        return lfs_file_write(&lfs, (lfs_file_t *)entry->f, buf, size);
    } else {
        return -1;
    }
}

int lfs_lseek_wrp(int fd, uint32_t offset, int whence)
{
    struct nfvfs_fentry *entry = ftable_get_entry(fd);
    int lfs_whence;

    if (entry == NULL) {
        return -1;
    }

    switch (whence) {
    case NFVFS_SEEK_CUR:
        lfs_whence = LFS_SEEK_CUR;
        break;
    case NFVFS_SEEK_SET:
        lfs_whence = LFS_SEEK_SET;
        break;
    case NFVFS_SEEK_END:
        lfs_whence = LFS_SEEK_END;
        break;
    default:
        break;
    }

    if (S_IFREG(entry->mode)) {
        return lfs_file_seek(&lfs, (lfs_file_t *)entry->f, offset, lfs_whence);
    } else {
        return -1;
    }
}

struct nfvfs_operations lfs_ops = {
    .mount = lfs_mount_wrp,
    .unmount = lfs_unmount_wrp,
    .open = lfs_open_wrp,
    .close = lfs_close_wrp,
    .read = lfs_read_wrp,
    .write = lfs_write_wrp,
    .lseek = lfs_lseek_wrp,
};
