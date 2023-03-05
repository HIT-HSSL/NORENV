/**
 * The middle level operations of eHNFFS
 */

#ifndef eHNFFS_MIDDLE_H
#define eHNFFS_MIDDLE_H

#include "eHNFFS.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * -------------------------------------------------------------------------------------------------------
     * --------------------------------------    FS level operations    --------------------------------------
     * -------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_rawformat(eHNFFS_t *eHNFFS, const struct eHNFFS_config *cfg);

    int eHNFFS_rawmount(eHNFFS_t *eHNFFS, const struct eHNFFS_config *cfg);

    int eHNFFS_rawunmount(eHNFFS_t *eHNFFS);

    eHNFFS_size_t eHNFFS_fs_rawsize(eHNFFS_t *eHNFFS);

    bool eHNFFS_file_isopen(eHNFFS_file_list_ram_t *file_list, eHNFFS_size_t id);

    bool eHNFFS_dir_isopen(eHNFFS_dir_list_ram_t *dir_list, eHNFFS_size_t id);

    /**
     * -------------------------------------------------------------------------------------------------------
     * -------------------------------------    File level operations    -------------------------------------
     * -------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_file_rawopen(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t **file,
                            char *path, int flags);

    int eHNFFS_file_rawclose(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file);

    eHNFFS_soff_t eHNFFS_file_rawseek(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                                      eHNFFS_soff_t off, int whence);

    eHNFFS_ssize_t eHNFFS_file_rawread(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                                       void *buffer, eHNFFS_size_t size);

    eHNFFS_ssize_t eHNFFS_file_rawwrite(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                                        void *buffer, eHNFFS_size_t size);

    int eHNFFS_file_rawdelete(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file);

    /**
     * -------------------------------------------------------------------------------------------------------
     * -------------------------------------    Dir level operations    --------------------------------------
     * -------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_dir_rawopen(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t **dir, char *path);

    int eHNFFS_dir_rawclose(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir);

    int eHNFFS_dir_rawdelete(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir);

    int eHNFFS_dir_rawread(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_info_ram_t *info);

#ifdef __cplusplus
}
#endif

#endif
