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

#include "eHNFFS_brigde.h"
#include "delay.h"
#include "eHNFFS.h"
#include "eHNFFS_high.h"
#include "nfvfs.h"
#include "w25qxx.h"

eHNFFS_t eHNFFS;

int W25Qxx_readeHNFFS(const struct eHNFFS_config *c, eHNFFS_size_t sector,
                      eHNFFS_off_t off, void *buffer, eHNFFS_size_t size)
{
    if (sector >= W25Q256_NUM_GRAN)
    {
        return eHNFFS_ERR_IO;
    }

    W25QXX_Read(buffer, sector * W25Q256_ERASE_GRAN + off, size);
    return eHNFFS_ERR_OK;
}

int W25Qxx_writeeHNFFS(const struct eHNFFS_config *c, eHNFFS_size_t sector,
                       eHNFFS_off_t off, void *buffer, eHNFFS_size_t size)
{
    if (sector >= W25Q256_NUM_GRAN) // error
    {
        return eHNFFS_ERR_IO;
    }

    // W25QXX_Write(buffer, sector * W25Q256_ERASE_GRAN + off, size);
    W25QXX_Write_NoCheck(buffer, sector * W25Q256_ERASE_GRAN + off, size);

    return eHNFFS_ERR_OK;
}

int W25Qxx_eraseeHNFFS(const struct eHNFFS_config *c, eHNFFS_size_t sector)
{
    if (sector >= W25Q256_NUM_GRAN) // error
    {
        return eHNFFS_ERR_IO;
    }

    W25QXX_Erase_Sector(sector);
    return eHNFFS_ERR_OK;
}

int W25Qxx_synceHNFFS(const struct eHNFFS_config *c)
{
    return eHNFFS_ERR_OK;
}

const struct eHNFFS_config eHNFFS_cfg = {
    .read = W25Qxx_readeHNFFS,
    .prog = W25Qxx_writeeHNFFS,
    .erase = W25Qxx_eraseeHNFFS,
    .sync = W25Qxx_synceHNFFS,

    .read_size = 1,
    .prog_size = 1,
    .sector_size = 4096,
    .sector_count = 8192,
    // .cache_size = 2048,
    .cache_size = 512,
    .region_cnt = 128,
    .name_max = 255,
    .file_max = eHNFFS_FILE_MAX_SIZE,
    .root_dir_name = "1:",
};

int eHNFFS_mount_wrp()
{
    int err = -1;
    int tries = 0;

    err = eHNFFS_mount(&eHNFFS, &eHNFFS_cfg);
    while (err)
    {
        err = eHNFFS_format(&eHNFFS, &eHNFFS_cfg);
        printf("mount fail is %d\r\n", err);
        // delay_ms(1000);
        tries++;
        if (tries >= 2)
        {
            return -1;
        }
    }
    return eHNFFS_ERR_OK;
}

int eHNFFS_unmount_wrp()
{
    return eHNFFS_unmount(&eHNFFS);
}

int eHNFFS_fssize_wrp()
{
    return eHNFFS_fs_size(&eHNFFS);
}

int eHNFFS_open_wrp(char *path, int flags, int mode, struct nfvfs_context *context)
{
    eHNFFS_file_ram_t *file;
    eHNFFS_dir_ram_t *dir;
    int fentry = *(int *)context->in_data;
    int err = eHNFFS_ERR_OK;

    if (S_IFREG(mode))
    {
        err = eHNFFS_file_open(&eHNFFS, &file, path, flags);
        context->out_data = file;
    }
    else
    {
        err = eHNFFS_dir_open(&eHNFFS, &dir, path);
        context->out_data = dir;
    }

    if (err < 0)
    {
        return err;
    }

    return fentry;
}

int eHNFFS_close_wrp(int fd)
{
    int err = eHNFFS_ERR_OK;
    struct nfvfs_fentry *entry = ftable_get_entry(fd);
    if (entry == NULL)
    {
        return -1;
    }
    if (S_IFREG(entry->mode))
    {
        err = eHNFFS_file_close(&eHNFFS, (eHNFFS_file_ram_t *)entry->f);
        if (err < 0)
        {
            printf("eHNFFS_file_close error is %d\r\n", err);
        }
    }
    else
    {
        err = eHNFFS_dir_close(&eHNFFS, (eHNFFS_dir_ram_t *)entry->f);
        if (err < 0)
        {
            printf("eHNFFS_dir_close error is %d\r\n", err);
        }
    }
    // vPortFree(entry->f);
    entry->f = NULL;
    return eHNFFS_ERR_OK;
}

int eHNFFS_read_wrp(int fd, void *buf, uint32_t size)
{
    struct nfvfs_fentry *entry = ftable_get_entry(fd);
    if (entry == NULL)
    {
        return -1;
    }
    if (S_IFREG(entry->mode))
    {
        return eHNFFS_file_read(&eHNFFS, (eHNFFS_file_ram_t *)entry->f,
                                (void *)buf, size);
    }
    else
    {
        return -1;
    }
}

int eHNFFS_write_wrp(int fd, void *buf, uint32_t size)
{
    struct nfvfs_fentry *entry = ftable_get_entry(fd);
    if (entry == NULL)
    {
        return -1;
    }
    if (S_IFREG(entry->mode))
    {
        return eHNFFS_file_write(&eHNFFS, (eHNFFS_file_ram_t *)entry->f,
                                 (void *)buf, size);
    }
    else
    {
        return -1;
    }
}

int eHNFFS_lseek_wrp(int fd, uint32_t offset, int whence)
{
    struct nfvfs_fentry *entry = ftable_get_entry(fd);
    int eHNFFS_whence;

    if (entry == NULL)
    {
        return -1;
    }

    switch (whence)
    {
    case NFVFS_SEEK_CUR:
        eHNFFS_whence = eHNFFS_SEEK_CUR;
        break;
    case NFVFS_SEEK_SET:
        eHNFFS_whence = eHNFFS_SEEK_SET;
        break;
    case NFVFS_SEEK_END:
        eHNFFS_whence = eHNFFS_SEEK_END;
        break;
    default:
        break;
    }

    if (S_IFREG(entry->mode))
    {
        return eHNFFS_file_seek(&eHNFFS, (eHNFFS_file_ram_t *)entry->f,
                                offset, eHNFFS_whence);
    }
    else
    {
        return -1;
    }
}

int eHNFFS_readdir_wrp(int fd, struct nfvfs_dentry *buf)
{
    struct nfvfs_fentry *entry = ftable_get_entry(fd);
    eHNFFS_info_ram_t my_info;

    int err = eHNFFS_dir_read(&eHNFFS, (eHNFFS_dir_ram_t *)entry->f, &my_info);
    if (err < 0)
    {
        printf("read dir error type is %d\r\n", err);
        return -1;
    }

    if (my_info.type == 0)
        buf->type = (int)NFVFS_TYPE_END;
    else if (my_info.type == eHNFFS_DATA_REG)
        buf->type = (int)NFVFS_TYPE_REG;
    else if (my_info.type == eHNFFS_DATA_DIR)
        buf->type = (int)NFVFS_TYPE_DIR;
    else
    {
        printf("err in dir read function!\r\n");
        return -1;
    }

    strcpy(buf->name, my_info.name);
    return err;
}

int eHNFFS_delete_wrp(int fd, char *path, int mode)
{
    int err = eHNFFS_ERR_OK;

    struct nfvfs_fentry *entry = (struct nfvfs_fentry *)ftable_get_entry(fd);
    if (entry == NULL)
    {
        return -1;
    }

    if (S_IFREG(mode))
    {
        err = eHNFFS_file_delete(&eHNFFS, (eHNFFS_file_ram_t *)entry->f);
        if (err != 0)
        {
            printf("Delete file error, err is %d\r\n", err);
        }
        return err;
    }
    else
    {
        err = eHNFFS_dir_delete(&eHNFFS, (eHNFFS_dir_ram_t *)entry->f);
        if (err != 0)
        {
            printf("Delete dir err, err is %d\r\n", err);
        }
    }

    return err;
}

int eHNFFS_fsync_wrp(int fd)
{
    int err = eHNFFS_ERR_OK;

    struct nfvfs_fentry *entry = ftable_get_entry(fd);
    if (entry == NULL)
    {
        return -1;
    }

    err = eHNFFS_file_sync(&eHNFFS, (eHNFFS_file_ram_t *)entry->f);
    if (err < 0)
    {
        printf("file sync error is %d\r\n", err);
    }
    return err;
}

struct nfvfs_operations eHNFFS_ops = {
    .mount = eHNFFS_mount_wrp,
    .unmount = eHNFFS_unmount_wrp,
    .fssize = eHNFFS_fssize_wrp,
    .open = eHNFFS_open_wrp,
    .close = eHNFFS_close_wrp,
    .read = eHNFFS_read_wrp,
    .write = eHNFFS_write_wrp,
    .lseek = eHNFFS_lseek_wrp,
    .readdir = eHNFFS_readdir_wrp,
    .remove = eHNFFS_delete_wrp,
    .fsync = eHNFFS_fsync_wrp,
};
