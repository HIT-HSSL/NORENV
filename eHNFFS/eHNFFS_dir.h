/**
 * Dir related operations.
 */
#ifndef eHNFFS_DIR_H
#define eHNFFS_DIR_H

#include "eHNFFS.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ------------------------------------------------------------    List operations    ------------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_list_insert(eHNFFS_dir_ram_t *dir, eHNFFS_off_t off);

    void eHNFFS_list_delete(eHNFFS_list_ram_t *list);

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * --------------------------------------------------------    Dir Traversing operations    ------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_dir_list_initialization(eHNFFS_t *eHNFFS, eHNFFS_dir_list_ram_t **list_addr);

    void eHNFFS_dir_list_free(eHNFFS_t *eHNFFS, eHNFFS_dir_list_ram_t *list);

    int eHNFFS_dir_free(eHNFFS_dir_list_ram_t *list, eHNFFS_dir_ram_t *dir);

    int eHNFFS_dtraverse_name(eHNFFS_t *eHNFFS, eHNFFS_size_t tail, char *name, eHNFFS_size_t namelen,
                              int type, bool *if_find, eHNFFS_size_t *sector, eHNFFS_size_t *id,
                              eHNFFS_size_t *name_sector, eHNFFS_off_t *name_off);

    int eHNFFS_dtraverse_data(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_size_t id,
                              void *buffer, eHNFFS_size_t *index_sector, eHNFFS_off_t *index_off);

    int eHNFFS_dtraverse_bfile_delete(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir);

    int eHNFFS_dtraverse_ospace(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_size_t sector);

    int eHNFFS_dir_prog(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, void *buffer, eHNFFS_size_t len,
                        int mode);

    int eHNFFS_dtraverse_gc(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_size_t region,
                            uint32_t *map_buffer);

    int eHNFFS_dir_old(eHNFFS_t *eHNFFS, eHNFFS_size_t tail);

    int eHNFFS_dir_update(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir);

    int eHNFFS_dir_lowopen(eHNFFS_t *eHNFFS, eHNFFS_size_t tail, eHNFFS_size_t id,
                           eHNFFS_size_t father_id, eHNFFS_size_t sector, eHNFFS_size_t off,
                           eHNFFS_size_t namelen, int type, eHNFFS_dir_ram_t **dir_addr);

    int eHNFFS_dir_lowclose(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir);

    int eHNFFS_dir_region_migration(eHNFFS_t *eHNFFS, eHNFFS_size_t region, uint32_t *map_buffer);

    int eHNFFS_fdir_file_find(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file, eHNFFS_dir_ram_t **dir_addr);

    int eHNFFS_fdir_dir_find(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_dir_ram_t **dir_addr);

    int eHNFFS_create_dir(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *father_dir, eHNFFS_dir_ram_t **dir_addr,
                          char *name, eHNFFS_size_t namelen);

    int eHNFFS_dir_migration(eHNFFS_t *eHNFFS, eHNFFS_size_t sector, eHNFFS_size_t region,
                             uint32_t *map_buffer);

#ifdef __cplusplus
}
#endif

#endif
