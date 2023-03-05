/**
 * The basic cache operations of eHNFFS
 */

#ifndef eHNFFS_CACHE_H
#define eHNFFS_CACHE_H

#include "eHNFFS.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * -------------------------------------------------------    Auxiliary cache functions    -------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */
    int eHNFFS_cache_initialize(eHNFFS_t *eHNFFS, eHNFFS_cache_ram_t **cache_addr, eHNFFS_size_t buffer_size);

    void eHNFFS_cache_one(eHNFFS_t *eHNFFS, eHNFFS_cache_ram_t *cache);

    void eHNFFS_cache_drop(eHNFFS_t *eHNFFS, eHNFFS_cache_ram_t *cache);

    int eHNFFS_cache_transition(eHNFFS_t *eHNFFS, void *src_buffer,
                                void *dst_buffer, eHNFFS_size_t size);

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * -------------------------------------------------------    Prog/Erase cache functions    ------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */
    int eHNFFS_cache_flush(eHNFFS_t *eHNFFS, eHNFFS_cache_ram_t *pcache,
                           eHNFFS_cache_ram_t *rcache, bool validate);

    int eHNFFS_cache_read(eHNFFS_t *eHNFFS, eHNFFS_size_t mode,
                          const eHNFFS_cache_ram_t *pcache,
                          eHNFFS_cache_ram_t *rcache, eHNFFS_size_t sector,
                          eHNFFS_off_t off, void *buffer, eHNFFS_size_t size);

    int eHNFFS_cache_prog(eHNFFS_t *eHNFFS, eHNFFS_size_t mode, eHNFFS_cache_ram_t *pcache,
                          eHNFFS_cache_ram_t *rcache, bool validate, eHNFFS_size_t sector,
                          eHNFFS_off_t off, const void *buffer, eHNFFS_size_t size);

    int eHNFFS_read_to_cache(eHNFFS_t *eHNFFS, eHNFFS_size_t mode, eHNFFS_cache_ram_t *cache,
                             eHNFFS_size_t sector, eHNFFS_off_t off, eHNFFS_size_t size);

    int eHNFFS_cache_cmp(eHNFFS_t *eHNFFS, eHNFFS_size_t sector, eHNFFS_off_t off,
                         const void *buffer, eHNFFS_size_t size);

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * --------------------------------------------------------    Prog/Erase without cache    -------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_direct_shead_prog(eHNFFS_t *eHNFFS, eHNFFS_size_t sector, eHNFFS_off_t *off,
                                 eHNFFS_size_t len, void *buffer);

    int eHNFFS_direct_data_prog(eHNFFS_t *eHNFFS, eHNFFS_size_t sector, eHNFFS_off_t *off,
                                eHNFFS_size_t len, void *buffer);

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ---------------------------------------------------------    Further prog operations    -------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_superblock_prog(eHNFFS_t *eHNFFS, eHNFFS_superblock_ram_t *super,
                               eHNFFS_cache_ram_t *pcache);

    int eHNFFS_addr_prog(eHNFFS_t *eHNFFS, eHNFFS_superblock_ram_t *super,
                         void *buffer, eHNFFS_size_t len);

    int eHNFFS_super_cache_prog(eHNFFS_t *eHNFFS, eHNFFS_superblock_ram_t *super,
                                eHNFFS_cache_ram_t *pcache, void *buffer, eHNFFS_size_t len);

    int eHNFFS_sector_old(eHNFFS_t *eHNFFS, eHNFFS_size_t begin, eHNFFS_size_t num);

    int eHNFFS_bfile_sector_old(eHNFFS_t *eHNFFS, eHNFFS_bfile_index_ram_t *index,
                                eHNFFS_size_t num);

    int eHNFFS_data_delete(eHNFFS_t *eHNFFS, eHNFFS_size_t father_id, eHNFFS_size_t sector,
                           eHNFFS_off_t off, eHNFFS_size_t len);

    int eHNFFS_shead_reprog(eHNFFS_t *eHNFFS, eHNFFS_size_t begin, eHNFFS_size_t num,
                            void *buffer, eHNFFS_size_t len);

    int eHNFFS_region_read(eHNFFS_t *eHNFFS, eHNFFS_size_t region, uint32_t *buffer);

    eHNFFS_size_t eHNFFS_cache_valid_num(eHNFFS_cache_ram_t *cache);

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ---------------------------------------------------------    Further erase operations    ------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_sector_erase(eHNFFS_t *eHNFFS, eHNFFS_size_t sector);

    int eHNFFS_old_msector_erase(eHNFFS_t *eHNFFS, eHNFFS_size_t num,
                                 eHNFFS_size_t begin, eHNFFS_size_t *etimes);
#ifdef __cplusplus
}
#endif

#endif
