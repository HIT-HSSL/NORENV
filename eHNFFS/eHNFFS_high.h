/**
 * The high level operations of eHNFFS
 */

#ifndef eHNFFS_HIGH_H
#define eHNFFS_HIGH_H

#include "eHNFFS.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Thread-safe wrappers if enabled，
 */
#ifdef eHNFFS_THREADSAFE // thread safty, we should offer lock and unlock function
#define eHNFFS_LOCK(cfg) cfg->lock(cfg)
#define eHNFFS_UNLOCK(cfg) cfg->unlock(cfg)
#else
#define eHNFFS_LOCK(cfg) ((void)cfg, 0) // what ?
#define eHNFFS_UNLOCK(cfg) ((void)cfg)
#endif

    /**
     * -------------------------------------------------------------------------------------------------------
     * --------------------------------------    FS level operations    --------------------------------------
     * -------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_format(eHNFFS_t *eHNFFS, const struct eHNFFS_config *cfg);

    int eHNFFS_mount(eHNFFS_t *eHNFFS, const struct eHNFFS_config *cfg);

    int eHNFFS_unmount(eHNFFS_t *eHNFFS);

    eHNFFS_ssize_t eHNFFS_fs_size(eHNFFS_t *eHNFFS);

    /**
     * -------------------------------------------------------------------------------------------------------
     * -------------------------------------    File level operations    -------------------------------------
     * -------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_file_open(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t **file, char *path, int flags);

    int eHNFFS_file_close(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file);

    eHNFFS_ssize_t eHNFFS_file_read(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                                    void *buffer, eHNFFS_size_t size);

    eHNFFS_ssize_t eHNFFS_file_write(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                                     void *buffer, eHNFFS_size_t size);

    eHNFFS_soff_t eHNFFS_file_seek(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                                   eHNFFS_soff_t off, int whence);

    int eHNFFS_file_delete(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file);

    int eHNFFS_file_sync(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file);

    /**
     * -------------------------------------------------------------------------------------------------------
     * -------------------------------------    Dir level operations    --------------------------------------
     * -------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_dir_open(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t **dir, char *path);

    int eHNFFS_dir_close(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir);

    int eHNFFS_dir_delete(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir);

    int eHNFFS_dir_read(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_info_ram_t *info);

#ifdef __cplusplus
}
#endif

#endif
