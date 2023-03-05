/**
 * The high level operations of eHNFFS
 */
#include "eHNFFS_high.h"
#include "eHNFFS_middle.h"
#include "eHNFFS_file.h"

/**
 * -------------------------------------------------------------------------------------------------------
 * --------------------------------------    FS level operations    --------------------------------------
 * -------------------------------------------------------------------------------------------------------
 */

/**
 *  Before first mounting nor flash, we should initialize some basic informatino.
 */
int eHNFFS_format(eHNFFS_t *eHNFFS, const struct eHNFFS_config *cfg)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_TRACE("eHNFFS_format(%p, %p {.context=%p, "
                 ".read=%p, .prog=%p, .erase=%p, .sync=%p, "
                 ".read_size=%" PRIu32 ", .prog_size=%" PRIu32 ", "
                 ".block_size=%" PRIu32 ", .block_count=%" PRIu32 ", "
                 ".block_cycles=%" PRIu32 ", .cache_size=%" PRIu32 ", "
                 ".lookahead_size=%" PRIu32 ", .read_buffer=%p, "
                 ".prog_buffer=%p, .lookahead_buffer=%p, "
                 ".name_max=%" PRIu32 ", .file_max=%" PRIu32 ", "
                 ".attr_max=%" PRIu32 "})",
                 (void *)eHNFFS, (void *)cfg, cfg->context,
                 (void *)(uintptr_t)cfg->read, (void *)(uintptr_t)cfg->prog,
                 (void *)(uintptr_t)cfg->erase, (void *)(uintptr_t)cfg->sync,
                 cfg->read_size, cfg->prog_size, cfg->block_size, cfg->block_count,
                 cfg->block_cycles, cfg->cache_size, cfg->lookahead_size,
                 cfg->read_buffer, cfg->prog_buffer, cfg->lookahead_buffer,
                 cfg->name_max, cfg->file_max, cfg->attr_max);

    err = eHNFFS_rawformat(eHNFFS, cfg);

    eHNFFS_TRACE("eHNFFS_format -> %d", err);
    return err;
}

/**
 * Mount of eHNFFS, so we can use basic io functions it offers.
 */
int eHNFFS_mount(eHNFFS_t *eHNFFS, const struct eHNFFS_config *cfg)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_TRACE("eHNFFS_mount(%p, %p {.context=%p, "
                 ".read=%p, .prog=%p, .erase=%p, .sync=%p, "
                 ".read_size=%" PRIu32 ", .prog_size=%" PRIu32 ", "
                 ".block_size=%" PRIu32 ", .block_count=%" PRIu32 ", "
                 ".block_cycles=%" PRIu32 ", .cache_size=%" PRIu32 ", "
                 ".lookahead_size=%" PRIu32 ", .read_buffer=%p, "
                 ".prog_buffer=%p, .lookahead_buffer=%p, "
                 ".name_max=%" PRIu32 ", .file_max=%" PRIu32 ", "
                 ".attr_max=%" PRIu32 "})",
                 (void *)eHNFFS, (void *)cfg, cfg->context,
                 (void *)(uintptr_t)cfg->read, (void *)(uintptr_t)cfg->prog,
                 (void *)(uintptr_t)cfg->erase, (void *)(uintptr_t)cfg->sync,
                 cfg->read_size, cfg->prog_size, cfg->block_size, cfg->block_count,
                 cfg->block_cycles, cfg->cache_size, cfg->lookahead_size,
                 cfg->read_buffer, cfg->prog_buffer, cfg->lookahead_buffer,
                 cfg->name_max, cfg->file_max, cfg->attr_max);

    err = eHNFFS_rawmount(eHNFFS, cfg);

    eHNFFS_TRACE("eHNFFS_mount -> %d", err);
    return err;
}

/**
 * Umount eHNFFS if we don't need to use it.
 */
int eHNFFS_unmount(eHNFFS_t *eHNFFS)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_TRACE("eHNFFS_unmount(%p)", (void *)eHNFFS);

    err = eHNFFS_rawunmount(eHNFFS);

    eHNFFS_TRACE("eHNFFS_unmount -> %d", err);
    return err;
}

/**
 * How many nor flash space we have totally used now, in a sector granularity.
 */
eHNFFS_ssize_t eHNFFS_fs_size(eHNFFS_t *eHNFFS)
{
    eHNFFS_TRACE("eHNFFS_fs_size(%p)", (void *)eHNFFS);

    eHNFFS_ssize_t res = eHNFFS_fs_rawsize(eHNFFS);

    eHNFFS_TRACE("eHNFFS_fs_size -> %" PRId32, res);
    return res;
}

/**
 * -------------------------------------------------------------------------------------------------------
 * -------------------------------------    File level operations    -------------------------------------
 * -------------------------------------------------------------------------------------------------------
 */

/**
 * Open a file before we use it, if we don't have the file, just create a new one.
 * Flags is used to choose the open type of file, but in our demo is no use now.
 */
int eHNFFS_file_open(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t **file, char *path, int flags)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_TRACE("eHNFFS_file_open(%p, %p, \"%s\", %x)",
                 (void *)eHNFFS, (void *)file, path, flags);
    // Now we haven't done name resolution yet, so it can only be done after resolution
    // and find id in file list.
    // eHNFFS_ASSERT(eHNFFS_mlist_isopen(eHNFFS->mlist, (struct eHNFFS_mlist *)file));

    err = eHNFFS_file_rawopen(eHNFFS, file, path, flags);
    if (err)
        printf("open file err is %d\r\n", err);

    eHNFFS_TRACE("eHNFFS_file_open -> %d", err);
    return err;
}

/**
 * Close a file if we don't need it anymore.
 */
int eHNFFS_file_close(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_TRACE("eHNFFS_file_close(%p, %p)", (void *)eHNFFS, (void *)file);
    eHNFFS_ASSERT(eHNFFS_file_isopen(eHNFFS->file_list, file->id));

    err = eHNFFS_file_rawclose(eHNFFS, file);

    eHNFFS_TRACE("eHNFFS_file_close -> %d", err);
    return err;
}

/**
 * Read a file beginned at file.pos to buffer, length is size.
 */
eHNFFS_ssize_t eHNFFS_file_read(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                                void *buffer, eHNFFS_size_t size)
{
    eHNFFS_TRACE("eHNFFS_file_read(%p, %p, %p, %" PRIu32 ")",
                 (void *)eHNFFS, (void *)file, buffer, size);
    eHNFFS_ASSERT(eHNFFS_file_isopen(eHNFFS->file_list, file->id));

    eHNFFS_ssize_t res = eHNFFS_file_rawread(eHNFFS, file, buffer, size);

    eHNFFS_TRACE("eHNFFS_file_read -> %" PRId32, res);
    return res;
}

/**
 * Write a file beginned at file.pos from buffer, length is size.
 */
eHNFFS_ssize_t eHNFFS_file_write(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                                 void *buffer, eHNFFS_size_t size)
{
    eHNFFS_TRACE("eHNFFS_file_write(%p, %p, %p, %" PRIu32 ")",
                 (void *)eHNFFS, (void *)file, buffer, size);
    eHNFFS_ASSERT(eHNFFS_file_isopen(eHNFFS->file_list, file->id));

    eHNFFS_ssize_t res = eHNFFS_file_rawwrite(eHNFFS, file, buffer, size);

    eHNFFS_TRACE("eHNFFS_file_write -> %" PRId32, res);
    return res;
}

/**
 * Change the file.pos to off/end/current, all depends on whence.
 *
 * Whence if the seek module.
 * Off could be current position, absolute position or the end of file.
 * See more at eHNFFS_whence_flags
 */
eHNFFS_soff_t eHNFFS_file_seek(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                               eHNFFS_soff_t off, int whence)
{
    eHNFFS_TRACE("eHNFFS_file_seek(%p, %p, %" PRId32 ", %d)",
                 (void *)eHNFFS, (void *)file, off, whence);
    eHNFFS_ASSERT(eHNFFS_file_isopen(eHNFFS->file_list, file->id));

    eHNFFS_soff_t res = eHNFFS_file_rawseek(eHNFFS, file, off, whence);

    eHNFFS_TRACE("eHNFFS_file_seek -> %" PRId32, res);
    return res;
}

/**
 * Delete a file if we don't need anymore.
 */
int eHNFFS_file_delete(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_TRACE("eHNFFS_delete(%p, \"%s\")", (void *)eHNFFS, path);
    eHNFFS_ASSERT(eHNFFS_file_isopen(eHNFFS->file_list, file->id));

    err = eHNFFS_file_rawdelete(eHNFFS, file);

    eHNFFS_TRACE("eHNFFS_delete -> %d", err);
    return err;
}

/**
 * Sync data in file cache.
 */
int eHNFFS_file_sync(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_ASSERT(eHNFFS_file_isopen(eHNFFS->file_list, file->id));

    err = eHNFFS_file_flush(eHNFFS, file);

    eHNFFS_TRACE("eHNFFS_file_sync -> %d", err);
    return err;
}

/**
 * -------------------------------------------------------------------------------------------------------
 * -------------------------------------    Dir level operations    --------------------------------------
 * -------------------------------------------------------------------------------------------------------
 */

/**
 * Open a dir, we can't open a dir again if it isn't closed.
 */
int eHNFFS_dir_open(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t **dir, char *path)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_TRACE("eHNFFS_dir_open(%p, %p, \"%s\")", (void *)eHNFFS, (void *)dir, path);
    // Now we haven't done name resolution yet, so it can only be done after resolution
    // and find id in dir list.
    // eHNFFS_ASSERT(eHNFFS_mlist_isopen(eHNFFS->mlist, (struct eHNFFS_mlist *)dir));

    err = eHNFFS_dir_rawopen(eHNFFS, dir, path);

    eHNFFS_TRACE("eHNFFS_dir_open -> %d", err);
    return err;
}

/**
 * Close a dir, we can't close a dir before it is opened.
 */
int eHNFFS_dir_close(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_TRACE("eHNFFS_dir_close(%p, %p)", (void *)eHNFFS, (void *)dir);
    eHNFFS_ASSERT(eHNFFS_dir_isopen(eHNFFS->dir_list, dir->id));

    err = eHNFFS_dir_rawclose(eHNFFS, dir);

    eHNFFS_TRACE("eHNFFS_dir_close -> %d", err);
    return err;
}

/**
 * Delete a dir if we don't need anymore.
 */
int eHNFFS_dir_delete(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_TRACE("eHNFFS_delete(%p, \"%s\")", (void *)eHNFFS, path);
    eHNFFS_ASSERT(eHNFFS_dir_isopen(eHNFFS->dir_list, dir->id));

    err = eHNFFS_dir_rawdelete(eHNFFS, dir);

    eHNFFS_TRACE("eHNFFS_delete -> %d", err);
    return err;
}

int eHNFFS_dir_read(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_info_ram_t *info)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_TRACE("eHNFFS_dir_read(%p, %p, %p)",
                 (void *)eHNFFS, (void *)dir, (void *)info);

    err = eHNFFS_dir_rawread(eHNFFS, dir, info);

    eHNFFS_TRACE("eHNFFS_dir_read -> %d", err);
    return err;
}
