/**
 * The basic manager operations of eHNFFS
 */

#ifndef eHNFFS_MAP_H
#define eHNFFS_MAP_H

#include "eHNFFS.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * -----------------------------------------------------------    Manager operations    ----------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_commit_update(eHNFFS_t *eHNFFS, eHNFFS_commit_flash_t *commit,
                             eHNFFS_cache_ram_t *cache);

    int eHNFFS_region_map_initialization(eHNFFS_size_t region_num, eHNFFS_region_map_ram_t **region_map_addr);

    int eHNFFS_map_initialization(eHNFFS_t *eHNFFS, eHNFFS_map_ram_t **map_addr, eHNFFS_size_t size);

    int eHNFFS_manager_initialization(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t **manager_addr);

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ---------------------------------------------------------    Basic region operations    -------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_regionmap_assign(eHNFFS_t *eHNFFS, eHNFFS_region_map_ram_t *ram_region,
                                eHNFFS_region_map_flash_t *flash_region,
                                eHNFFS_size_t sector, eHNFFS_size_t free_off);

    int eHNFFS_region_type(eHNFFS_region_map_ram_t *region_map, eHNFFS_size_t region);

    int eHNFFS_global_region_migration(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager,
                                       eHNFFS_wl_message_t *wlarr);

    int eHNFFS_reserve_region_use(eHNFFS_region_map_ram_t *region_map, int type);

    int eHNFFS_remove_region(eHNFFS_t *eHNFFS, eHNFFS_region_map_ram_t *region_map, int type,
                             eHNFFS_size_t region);

    int eHNFFS_region_map_flush(eHNFFS_t *eHNFFS, eHNFFS_region_map_ram_t *region_map);

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ----------------------------------------------------------    Basic map operations    ---------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_map_flush(eHNFFS_t *eHNFFS, eHNFFS_map_ram_t *map, eHNFFS_size_t len);

    int eHNFFS_ram_map_change(eHNFFS_t *eHNFFS, eHNFFS_size_t region, eHNFFS_size_t bits_num,
                              eHNFFS_map_ram_t *map);

    int eHNFFS_find_in_map(eHNFFS_t *eHNFFS, eHNFFS_size_t bits_num, eHNFFS_map_ram_t *map,
                           eHNFFS_size_t num, eHNFFS_size_t *begin);

    int eHNFFS_directly_map_using(eHNFFS_t *eHNFFS, eHNFFS_size_t sector, eHNFFS_size_t off,
                                  eHNFFS_size_t begin, eHNFFS_size_t num);

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ----------------------------------------------------------    Sector map operations    --------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_sectormap_assign(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager,
                                eHNFFS_mapaddr_flash_t *map_addr, eHNFFS_size_t num);

    int eHNFFS_smap_flush(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager);

    int eHNFFS_smap_change(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager,
                           eHNFFS_cache_ram_t *pcache, eHNFFS_cache_ram_t *rcache);

    int eHNFFS_sector_nextsmap(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager,
                               int type);

    int eHNFFS_sectors_find(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager, eHNFFS_size_t num,
                            int type, eHNFFS_size_t *begin);

    int eHNFFS_sector_alloc(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager, int type,
                            eHNFFS_size_t num, eHNFFS_size_t pre_sector, eHNFFS_size_t id,
                            eHNFFS_size_t father_id, eHNFFS_size_t *begin, eHNFFS_size_t *etimes);

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ----------------------------------------------------------    Erase map operations    --------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_emap_set(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager,
                        eHNFFS_size_t begin, eHNFFS_size_t num);

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ------------------------------------------------------------    ID map operations    ----------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_id_map_initialization(eHNFFS_t *eHNFFS, eHNFFS_idmap_ram_t **map_addr);

    int eHNFFS_idmap_assign(eHNFFS_t *eHNFFS, eHNFFS_idmap_ram_t *id_map, eHNFFS_mapaddr_flash_t *map_addr,
                            eHNFFS_size_t num);

    int eHNFFS_idmap_flush(eHNFFS_t *eHNFFS, eHNFFS_idmap_ram_t *id_map);

    int eHNFFS_idmap_change(eHNFFS_t *eHNFFS, eHNFFS_idmap_ram_t *id_map,
                            eHNFFS_cache_ram_t *pcache, eHNFFS_cache_ram_t *rcache);

    int eHNFFS_id_alloc(eHNFFS_t *eHNFFS, eHNFFS_size_t *id);

    int eHNFFS_id_free(eHNFFS_t *eHNFFS, eHNFFS_idmap_ram_t *idmap, eHNFFS_size_t id);

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * -----------------------------------------------------------    wl heap operations    ----------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_wl_heaps_sort(eHNFFS_t *eHNFFS, eHNFFS_size_t num, eHNFFS_wl_message_t *heaps);

    int eHNFFS_wl_resort(eHNFFS_t *eHNFFS, eHNFFS_wl_ram_t *wl);

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * -----------------------------------------------------------    basic wl operations    ---------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    void eHNFFS_wlregion_reset(eHNFFS_wl_message_t *regions);

    void eHNFFS_wl_swap(eHNFFS_t *eHNFFS, eHNFFS_wl_message_t *a, eHNFFS_wl_message_t *b);

    int eHNFFS_wladd_flush(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager,
                           eHNFFS_wl_message_t *regions);

    int eHNFFS_wl_create(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager, eHNFFS_wl_ram_t **wl_addr);

    int eHNFFS_wl_malloc(eHNFFS_t *eHNFFS, eHNFFS_wl_ram_t *wl, eHNFFS_wladdr_flash_t *wladdr);

    int eHNFFS_wl_flush(eHNFFS_t *eHNFFS, eHNFFS_wl_ram_t *wl);

#ifdef __cplusplus
}
#endif

#endif
