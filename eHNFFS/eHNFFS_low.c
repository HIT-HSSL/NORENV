/**
 * The low level operations of eHNFFS
 */
#include "eHNFFS_low.h"
#include "eHNFFS_util.h"
#include "eHNFFS_cache.h"
#include "eHNFFS_dir.h"
#include "eHNFFS_file.h"
#include "eHNFFS_gc.h"
#include "eHNFFS_head.h"
#include "eHNFFS_manage.h"
#include "eHNFFS_tree.h"

/**
 * -------------------------------------------------------------------------------------------------------
 * --------------------------------    read/prg/erase related operations    ------------------------------
 * -------------------------------------------------------------------------------------------------------
 */

/**
 * The sync function.
 *
 * In this case, we should flush data in pcache to nor flash, empty rcache
 * and pcache and use sync function offered by user.
 */
int eHNFFS_sync(eHNFFS_t *eHNFFS, eHNFFS_cache_ram_t *pcache,
                eHNFFS_cache_ram_t *rcache, bool validate)
{
    eHNFFS_cache_drop(eHNFFS, rcache);

    int err = eHNFFS_cache_flush(eHNFFS, pcache, rcache, validate);
    if (err)
    {
        return err;
    }

    err = eHNFFS->cfg->sync(eHNFFS->cfg);
    eHNFFS_ASSERT(err <= 0);
    return err;
}

/**
 * Erase the sector of nor flash.
 */
int eHNFFS_erase(eHNFFS_t *eHNFFS, eHNFFS_size_t sector)
{
    eHNFFS_ASSERT(sector < eHNFFS->cfg->sector_count);
    int err = eHNFFS->cfg->erase(eHNFFS->cfg, sector);
    eHNFFS_ASSERT(err <= 0);
    return err;
}

/**
 * -------------------------------------------------------------------------------------------------------
 * ----------------------------------    Initialize related operations    --------------------------------
 * -------------------------------------------------------------------------------------------------------
 */

/**
 * Initialize eHNFFS when mounting or formating.
 *
 * We also should allocate ram memory for rcache/pcache buffer.
 */
int eHNFFS_init(eHNFFS_t *eHNFFS, const struct eHNFFS_config *cfg)
{
    eHNFFS->cfg = cfg;
    int err = 0;
    int err2 = 0;

    // Validate that the eHNFFS-cfg sizes were initiated properly before
    // performing any arithmetic logics with them
    eHNFFS_ASSERT(eHNFFS->cfg->read_size != 0);
    eHNFFS_ASSERT(eHNFFS->cfg->prog_size != 0);
    eHNFFS_ASSERT(eHNFFS->cfg->sector_size != 0);
    eHNFFS_ASSERT(eHNFFS->cfg->cache_size != 0);

    // Check that sector size is a multiple of cache size is a multiple
    // of prog and read sizes
    eHNFFS_ASSERT(eHNFFS->cfg->cache_size % eHNFFS->cfg->read_size == 0);
    eHNFFS_ASSERT(eHNFFS->cfg->cache_size % eHNFFS->cfg->prog_size == 0);
    eHNFFS_ASSERT(eHNFFS->cfg->sector_size % eHNFFS->cfg->cache_size == 0);

    // Check whether or not region map message is valid.
    eHNFFS_ASSERT(eHNFFS->cfg->region_cnt > 0);
    eHNFFS_ASSERT(eHNFFS->cfg->region_cnt <= eHNFFS_REGION_NUM_MAX);
    eHNFFS_ASSERT(eHNFFS->cfg->sector_count > 0);
    eHNFFS_ASSERT(eHNFFS->cfg->sector_count % eHNFFS->cfg->region_cnt == 0);

    // Initialize prog cache.
    err = eHNFFS_cache_initialize(eHNFFS, &eHNFFS->pcache, eHNFFS->cfg->cache_size);
    if (err)
    {
        goto cleanup;
    }

    // Initialize read cache.
    err = eHNFFS_cache_initialize(eHNFFS, &eHNFFS->rcache, eHNFFS->cfg->cache_size);
    if (err)
    {
        goto cleanup;
    }

    // Initialize superblock message.
    err = eHNFFS_super_initialization(eHNFFS, &eHNFFS->superblock);
    if (err)
    {
        goto cleanup;
    }

    // Initialize manager structure.
    err = eHNFFS_manager_initialization(eHNFFS, &eHNFFS->manager);
    if (err)
    {
        goto cleanup;
    }

    // Initialize hash tree structure.
    err = eHNFFS_hash_tree_initialization(eHNFFS, &eHNFFS->hash_tree);
    if (err)
    {
        goto cleanup;
    }

    // Initialize id map structure.
    err = eHNFFS_id_map_initialization(eHNFFS, &eHNFFS->id_map);
    if (err)
    {
        goto cleanup;
    }

    // Initialize file list structure.
    err = eHNFFS_file_list_initialization(eHNFFS, &eHNFFS->file_list);
    if (err)
    {
        goto cleanup;
    }

    // Initialize dir list structure.
    err = eHNFFS_dir_list_initialization(eHNFFS, &eHNFFS->dir_list);
    if (err)
    {
        goto cleanup;
    }

    return err;

cleanup:
    err2 = eHNFFS_deinit(eHNFFS);
    if (err2)
        return err2;
    return err;
}

/**
 * Free ram memory when deinit.
 */
int eHNFFS_deinit(eHNFFS_t *eHNFFS)
{
    int err = eHNFFS_ERR_OK;

    // Free superblock.
    if (eHNFFS->superblock)
        eHNFFS_free(eHNFFS->superblock);

    // Free manager.
    if (eHNFFS->manager)
    {
        if (eHNFFS->manager->region_map)
        {
            if (eHNFFS->manager->region_map->free_region)
                eHNFFS_free(eHNFFS->manager->region_map->free_region);
            if (eHNFFS->manager->region_map->dir_region)
                eHNFFS_free(eHNFFS->manager->region_map->dir_region);
            if (eHNFFS->manager->region_map->bfile_region)
                eHNFFS_free(eHNFFS->manager->region_map->bfile_region);
            eHNFFS_free(eHNFFS->manager->region_map);
        }

        if (eHNFFS->manager->dir_map)
        {
            if (eHNFFS->manager->dir_map->etimes)
                eHNFFS_free(eHNFFS->manager->dir_map->etimes);
            eHNFFS_free(eHNFFS->manager->dir_map);
        }

        if (eHNFFS->manager->bfile_map)
            eHNFFS_free(eHNFFS->manager->bfile_map);
        if (eHNFFS->manager->erase_map)
            eHNFFS_free(eHNFFS->manager->erase_map);
        if (eHNFFS->manager->wl)
            eHNFFS_free(eHNFFS->manager->wl);
        eHNFFS_free(eHNFFS->manager);
    }

    // Free hash tree.
    if (eHNFFS->hash_tree)
    {
        if (eHNFFS->hash_tree->num != 0)
        {
            err = eHNFFS_hash_tree_free(eHNFFS->hash_tree->root_hash);
            if (err)
            {
                return err;
            }
        }
        eHNFFS_free(eHNFFS->hash_tree);
    }

    // Free id map.
    if (eHNFFS->id_map)
    {
        if (eHNFFS->id_map->free_map)
        {
            if (eHNFFS->id_map->free_map->etimes)
                eHNFFS_free(eHNFFS->id_map->free_map->etimes);
            eHNFFS_free(eHNFFS->id_map->free_map);
        }
        if (eHNFFS->id_map->remove_map)
            eHNFFS_free(eHNFFS->id_map->remove_map);
        eHNFFS_free(eHNFFS->id_map);
    }

    // Free file list.
    if (eHNFFS->file_list)
        eHNFFS_file_list_free(eHNFFS, eHNFFS->file_list);

    // Free dir list.
    if (eHNFFS->dir_list)
        eHNFFS_dir_list_free(eHNFFS, eHNFFS->dir_list);

    // Free read cache.
    if (eHNFFS->rcache)
    {
        if (eHNFFS->rcache->buffer)
            eHNFFS_free(eHNFFS->rcache->buffer);
        eHNFFS_free(eHNFFS->rcache);
    }

    // Free prog cache.
    if (eHNFFS->pcache)
    {
        if (eHNFFS->pcache->buffer)
            eHNFFS_free(eHNFFS->pcache->buffer);
        eHNFFS_free(eHNFFS->pcache);
    }

    return err;
}

/**
 * When mounting, choose the right superblock that could use.
 */
int eHNFFS_select_supersector(eHNFFS_t *eHNFFS, eHNFFS_size_t *sector)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_head_t superhead[2];

    // Read sector head of sector 0.
    err = eHNFFS->cfg->read(eHNFFS->cfg, 0, 0, &superhead[0], sizeof(eHNFFS_head_t));
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    // Read sector head of sector 1.
    err = eHNFFS->cfg->read(eHNFFS->cfg, 1, 0, &superhead[1], sizeof(eHNFFS_head_t));
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    // If one value of sector head is eHNFFS_NULL,
    // then we choose another sector as super sector.
    if (superhead[0] == eHNFFS_NULL && superhead[1] == eHNFFS_NULL)
    {
        return eHNFFS_ERR_CORRUPT;
    }
    else if (superhead[1] == eHNFFS_NULL)
    {
        *sector = 0;
        return err;
    }
    else if (superhead[0] == eHNFFS_NULL)
    {
        *sector = 1;
        return err;
    }

    // If one sector head message is wrong, then we read wrong message.
    if (eHNFFS_shead_novalid(superhead[0]) || eHNFFS_shead_novalid(superhead[1]) ||
        eHNFFS_shead_type(superhead[0]) != eHNFFS_SECTOR_SUPER ||
        eHNFFS_shead_type(superhead[1]) != eHNFFS_SECTOR_SUPER)
    {
        err = eHNFFS_ERR_CORRUPT;
        return err;
    }

    // If one supersector can't be used, then what we need must be the next one.
    if (eHNFFS_shead_state(superhead[1]) == eHNFFS_SECTOR_FREE ||
        eHNFFS_shead_state(superhead[1]) == eHNFFS_SECTOR_OLD)
    {
        *sector = 0;
        return err;
    }
    else if (eHNFFS_shead_state(superhead[0]) == eHNFFS_SECTOR_FREE ||
             eHNFFS_shead_state(superhead[0]) == eHNFFS_SECTOR_OLD)
    {
        *sector = 1;
        return err;
    }

    // If two of them could be used, corrupt must happen during compact of one a version of supersector
    // so current valid one is the lower version of supersector.
    if (eHNFFS_shead_extend(superhead[0]) < eHNFFS_shead_extend(superhead[1]))
    {
        *sector = 0;
        return err;
    }
    else
    {
        *sector = 1;
        return err;
    }
}

/**
 * Calculate the valid bit(0) of bit map in given nor flash space (sector, off, map_len).
 */
eHNFFS_ssize_t eHNFFS_valid_num(eHNFFS_t *eHNFFS, eHNFFS_size_t sector, eHNFFS_off_t off,
                                eHNFFS_ssize_t map_len, eHNFFS_cache_ram_t *rcache)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t num = 0;
    eHNFFS_size_t size = 0;
    eHNFFS_size_t current_sector = sector;
    eHNFFS_size_t current_off = off;
    while (map_len > 0)
    {
        // Read to rcache firstly.
        size = eHNFFS_min(eHNFFS->cfg->sector_size - current_off,
                          eHNFFS_min(eHNFFS->cfg->cache_size, map_len));
        err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, eHNFFS->rcache,
                                   current_sector, current_off, size);
        if (err)
        {
            return err;
        }

        // Now rcache size should be aligned at 4 Bytes.
        num += eHNFFS_cache_valid_num(eHNFFS->rcache);

        current_off += size;
        if (current_off == eHNFFS->cfg->sector_size)
        {
            current_off = 0;
            current_sector++;
        }
        else if (current_off > eHNFFS->cfg->sector_size)
        {
            err = eHNFFS_ERR_WRONGCAL;
            return err;
        }

        map_len -= size;
    }

    if (map_len < 0)
    {
        err = eHNFFS_ERR_WRONGCAL;
        return err;
    }

    return num;
}
