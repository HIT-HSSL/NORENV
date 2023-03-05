/**
 * The basic gc operations of eHNFFS
 */

#ifndef eHNFFS_GC_H
#define eHNFFS_GC_H

#include "eHNFFS.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * --------------------------------------------------------    Superblock gc functions    --------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_super_initialization(eHNFFS_t *eHNFFS, eHNFFS_superblock_ram_t **super_addr);

    int eHNFFS_superblock_change(eHNFFS_t *eHNFFS, eHNFFS_superblock_ram_t *super);

    int eHNFFS_bfile_region_migration(eHNFFS_t *eHNFFS, eHNFFS_size_t region, uint32_t *map_buffer);

    int eHNFFS_dir_gc(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_size_t region,
                      uint32_t *map_buffer);

    int eHNFFS_bfile_gc(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file);

    int eHNFFS_dir_file_free(eHNFFS_t *eHNFFS);

    int eHNFFS_region_migration(eHNFFS_t *eHNFFS, eHNFFS_size_t region, int type);

#ifdef __cplusplus
}
#endif

#endif
