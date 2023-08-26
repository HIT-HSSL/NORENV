/**
 * The basic manager operations of eHNFFS
 */

#include "eHNFFS_manage.h"
#include "eHNFFS_cache.h"
#include "eHNFFS_head.h"
#include "eHNFFS_gc.h"

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * -----------------------------------------------------------    Manager operations    ----------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Updata all in-ram structure with in-flash commit message.
 */
int eHNFFS_commit_update(eHNFFS_t *eHNFFS, eHNFFS_commit_flash_t *commit,
                         eHNFFS_cache_ram_t *cache)
{
    int err = eHNFFS_ERR_OK;

    // Read corresponding region to idmap.
    eHNFFS_idmap_ram_t *idmap = eHNFFS->id_map;
    eHNFFS_size_t region = commit->next_id / idmap->bits_in_buffer;

    err = eHNFFS_ram_map_change(eHNFFS, region, idmap->bits_in_buffer, idmap->free_map);
    if (err)
    {
        return err;
    }
    idmap->free_map->index = commit->next_id % idmap->bits_in_buffer;

    // Index in remove map is change flag, we should turn it to 0 first.
    idmap->remove_map->index = 0;

    // Update scan_times.
    eHNFFS_flash_manage_ram_t *manager = eHNFFS->manager;
    manager->scan_times = commit->scan_times;

    // Index in erase map is change flag, we should turn it to 0 first.
    manager->erase_map->index = 0;

    // Read corresponding region to free dir sector map.
    region = commit->next_dir_sector / manager->region_size;
    err = eHNFFS_ram_map_change(eHNFFS, region, manager->region_size, manager->dir_map);
    if (err)
    {
        return err;
    }
    manager->region_map->dir_index = region;
    manager->dir_map->index = commit->next_dir_sector % manager->region_size;

    // Read corresponding region to free big file map.
    region = commit->next_bfile_sector / manager->region_size;
    err = eHNFFS_ram_map_change(eHNFFS, region, manager->region_size, manager->bfile_map);
    if (err)
    {
        return err;
    }
    manager->region_map->bfile_index = region;
    manager->bfile_map->index = commit->next_bfile_sector % manager->region_size;

    // Update reserve region message.
    manager->region_map->reserve = commit->reserve;

    // If we don't do wl now, then return directly.
    if (!manager->wl)
    {
        return err;
    }

    // Read candidate dir regions to wl module.
    eHNFFS_cache_one(eHNFFS, cache);
    manager->wl->index = commit->wl_index;
    eHNFFS_size_t sector = manager->wl->begin;
    eHNFFS_off_t off = eHNFFS_aligndown(manager->wl->index, eHNFFS_RAM_REGION_NUM) *
                           sizeof(eHNFFS_wl_message_t) +
                       2 * sizeof(eHNFFS_head_t);
    err = eHNFFS->cfg->read(eHNFFS->cfg, sector, off, cache->buffer,
                            eHNFFS_RAM_REGION_NUM * sizeof(eHNFFS_wl_message_t));
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    eHNFFS_wl_message_t *wl_message = (eHNFFS_wl_message_t *)cache->buffer;
    for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
    {
        manager->wl->regions[i].region = wl_message->region;
        manager->wl->regions[i].erase_times = 0;
    }

    // Read candidate big file regions to wl module.
    manager->wl->bfile_index = commit->wl_bfile_index;
    off = eHNFFS_aligndown(manager->wl->bfile_index, eHNFFS_RAM_REGION_NUM) *
              sizeof(eHNFFS_wl_message_t) +
          2 * sizeof(eHNFFS_head_t);
    err = eHNFFS->cfg->read(eHNFFS->cfg, sector, off, cache->buffer,
                            eHNFFS_RAM_REGION_NUM * sizeof(eHNFFS_wl_message_t));
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
    {
        manager->wl->bfile_regions[i].region = wl_message->region;
        manager->wl->bfile_regions[i].erase_times = 0;
        manager->wl->exten_regions[i].erase_times = 0;
    }

    eHNFFS_cache_one(eHNFFS, cache);
    return err;
}

/**
 * Initialization function of region map.
 */
int eHNFFS_region_map_initialization(eHNFFS_size_t region_num, eHNFFS_region_map_ram_t **region_map_addr)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t size;

    // Allocate memory for region map.
    eHNFFS_region_map_ram_t *region_map = eHNFFS_malloc(sizeof(eHNFFS_region_map_ram_t));
    if (!region_map)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    // Initialize basic data.
    region_map->begin = eHNFFS_NULL;
    region_map->off = eHNFFS_NULL;
    region_map->change_flag = 0;
    region_map->reserve = eHNFFS_NULL;

    // Allocate memory for free region buffer.
    size = eHNFFS_alignup(region_num, eHNFFS_SIZE_T_NUM) / 8;
    region_map->free_region = eHNFFS_malloc(size);
    if (!region_map->free_region)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }
    memset(region_map->free_region, eHNFFS_NULL, size);

    // Initialize and allocate memory for dir region message.
    region_map->dir_region_num = 0;
    region_map->dir_index = eHNFFS_NULL;
    region_map->dir_region = eHNFFS_malloc(size);
    if (!region_map->dir_region)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }
    memset(region_map->dir_region, eHNFFS_NULL, size);

    // Initialize and allocate memory for dir region message.
    region_map->bfile_region_num = 0;
    region_map->bfile_index = eHNFFS_NULL;
    region_map->bfile_region = eHNFFS_malloc(size);
    if (!region_map->bfile_region)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }
    memset(region_map->bfile_region, eHNFFS_NULL, size);

    *region_map_addr = region_map;
    return err;

cleanup:
    if (region_map)
    {
        if (region_map->free_region)
            eHNFFS_free(region_map->free_region);

        if (region_map->dir_region)
            eHNFFS_free(region_map->dir_region);

        if (region_map->bfile_region)
            eHNFFS_free(region_map->bfile_region);

        eHNFFS_free(region_map);
    }
    return err;
}

/**
 * Initialization function of dir/bfile/erase map.
 */
int eHNFFS_map_initialization(eHNFFS_t *eHNFFS, eHNFFS_map_ram_t **map_addr, eHNFFS_size_t size)
{
    int err = eHNFFS_ERR_OK;

    // Allocate memory for map.
    eHNFFS_map_ram_t *map = eHNFFS_malloc(sizeof(eHNFFS_map_ram_t) + size);
    if (!map)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    // Initialize basic message.
    map->begin = eHNFFS_NULL;
    map->off = eHNFFS_NULL;
    map->etimes = NULL;
    map->region = eHNFFS_NULL;
    map->index = eHNFFS_NULL;
    map->free_num = 0;
    memset(map->buffer, eHNFFS_NULL, size);
    *map_addr = map;
    return err;

cleanup:
    if (map)
        eHNFFS_free(map);
    return err;
}

/**
 * Initialization function of manager structure.
 */
int eHNFFS_manager_initialization(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t **manager_addr)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t size;
    // Allocate memory for manager.
    eHNFFS_flash_manage_ram_t *manager = eHNFFS_malloc(sizeof(eHNFFS_flash_manage_ram_t));
    if (!manager)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    // Initialize basic message.
    manager->region_num = eHNFFS->cfg->region_cnt;
    manager->region_size = eHNFFS->cfg->sector_count / eHNFFS->cfg->region_cnt;
    manager->scan_times = 0;

    // Initialize region map.
    err = eHNFFS_region_map_initialization(manager->region_num, &manager->region_map);
    if (err)
    {
        goto cleanup;
    }

    // Initialize dir map.
    size = eHNFFS_alignup(manager->region_size, eHNFFS_SIZE_T_NUM) / 8;
    err = eHNFFS_map_initialization(eHNFFS, &manager->dir_map, size);
    if (err)
    {
        goto cleanup;
    }

    // Initialize big file map.
    err = eHNFFS_map_initialization(eHNFFS, &manager->bfile_map, size);
    if (err)
    {
        goto cleanup;
    }

    // Initialize erase map.
    err = eHNFFS_map_initialization(eHNFFS, &manager->erase_map, size);
    if (err)
    {
        goto cleanup;
    }

    // We judge whether or not to create wl structure in mount function.
    // Wl module memory is allocated only when we should use it.
    manager->wl = NULL;

    *manager_addr = manager;
    return err;

cleanup:
    if (manager)
    {
        if (manager->region_map)
        {
            if (manager->region_map->free_region)
                eHNFFS_free(manager->region_map->free_region);

            if (manager->region_map->dir_region)
                eHNFFS_free(manager->region_map->dir_region);

            if (manager->region_map->bfile_region)
                eHNFFS_free(manager->region_map->bfile_region);

            eHNFFS_free(manager->region_map);
        }

        if (manager->dir_map)
            eHNFFS_free(manager->dir_map);

        if (manager->bfile_map)
            eHNFFS_free(manager->bfile_map);

        if (manager->erase_map)
            eHNFFS_free(manager->erase_map);

        if (manager->wl)
            eHNFFS_free(manager->wl);

        eHNFFS_free(manager);
    }
    return err;
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * ---------------------------------------------------------    Basic region operations    -------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Assign basic ram region map message with flash region map.
 */
int eHNFFS_regionmap_assign(eHNFFS_t *eHNFFS, eHNFFS_region_map_ram_t *ram_region,
                            eHNFFS_region_map_flash_t *flash_region,
                            eHNFFS_size_t sector, eHNFFS_size_t free_off)
{
    int err = eHNFFS_ERR_OK;

    ram_region->begin = sector;
    ram_region->off = free_off;
    ram_region->reserve = eHNFFS_NULL;
    ram_region->change_flag = 0;

    int p = 0;
    int q = 0;

    // TODO
    // after writing two region map message, we have malloced twice memory.

    // Free region map message.
    eHNFFS_size_t len = eHNFFS->cfg->region_cnt / 8;
    eHNFFS_size_t size = eHNFFS_alignup(len, sizeof(eHNFFS_size_t));
    if (ram_region->free_region == NULL)
    {
        ram_region->free_region = eHNFFS_malloc(size);
        if (!ram_region)
        {
            err = eHNFFS_ERR_NOMEM;
            goto cleanup;
        }
        memset(ram_region->free_region, eHNFFS_NULL, size);
    }
    memcpy(ram_region->free_region, flash_region->map, len);

    // Dir region map message.
    if (ram_region->dir_region == NULL)
    {
        ram_region->dir_region = eHNFFS_malloc(size);
        if (!ram_region->dir_region)
        {
            err = eHNFFS_ERR_NOMEM;
            goto cleanup;
        }
        memset(ram_region->dir_region, eHNFFS_NULL, size);
    }
    memcpy(ram_region->dir_region, &flash_region->map[len], len);
    ram_region->dir_region_num = 0;
    for (int i = 0; i < eHNFFS->cfg->region_cnt; i++)
    {
        if ((ram_region->dir_region[p] >> q) & 1U == 0)
            ram_region->dir_region_num++;
    }

    // Big file region map message.
    if (ram_region->bfile_region == NULL)
    {
        ram_region->bfile_region = eHNFFS_malloc(size);
        if (!ram_region->bfile_region)
        {
            err = eHNFFS_ERR_NOMEM;
            goto cleanup;
        }
        memset(ram_region->bfile_region, eHNFFS_NULL, size);
    }
    memcpy(ram_region->bfile_region, &flash_region->map[2 * len], len);
    p = 0;
    q = 0;
    ram_region->bfile_region_num = 0;
    for (int i = 0; i < eHNFFS->cfg->region_cnt; i++)
    {
        if ((ram_region->bfile_region[p] >> q) & 1U == 0)
            ram_region->bfile_region_num++;
    }

    return err;
cleanup:
    if (ram_region->free_region)
        eHNFFS_free(ram_region->free_region);
    if (ram_region->dir_region)
        eHNFFS_free(ram_region->dir_region);
    if (ram_region->bfile_region)
        eHNFFS_free(ram_region->bfile_region);
    return err;
}

/**
 * Tell us what is region used to do, for dir or for big file, or reserve for
 * other operations?
 */
int eHNFFS_region_type(eHNFFS_region_map_ram_t *region_map, eHNFFS_size_t region)
{
    if (region == region_map->reserve)
    {
        return eHNFFS_SECTOR_RESERVE;
    }

    int i = region / eHNFFS_SIZE_T_NUM;
    int j = region % eHNFFS_SIZE_T_NUM;
    if ((region_map->dir_region[i] >> j) & 1U)
    {
        return eHNFFS_SECTOR_DIR;
    }
    else if ((region_map->bfile_region[i] >> j) & 1U)
    {
        return eHNFFS_SECTOR_BFILE;
    }
    else
    {
        return eHNFFS_ERR_INVAL;
    }
}

/**
 * The basic data migration function for all regions.
 */
int eHNFFS_global_region_migration(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager,
                                   eHNFFS_wl_message_t *wlarr)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_region_map_ram_t *region_map = manager->region_map;
    eHNFFS_size_t threshold = region_map->dir_region_num;
    int begin = 0;
    int end = manager->region_num - 1;
    while (begin != end)
    {
        // If reserve region is less than threshold,
        // i.e should use to store dir data.
        if (region_map->reserve < threshold)
        {
            // Get type of the region used for.
            eHNFFS_size_t region = wlarr[end].region;
            int type = eHNFFS_region_type(region_map, region);
            if (type < 0)
            {
                return type;
            }

            if (type == eHNFFS_SECTOR_DIR)
            {
                // Do migrate.
                err = eHNFFS_region_migration(eHNFFS, region, (eHNFFS_size_t)type);
                if (err)
                {
                    return err;
                }
            }
            end--;
            continue;
        }
        else
        {
            // If reserve region is larger than threshold
            // i.e should use to store bigfile data.
            eHNFFS_size_t region = wlarr[begin].region;
            int type = eHNFFS_region_type(region_map, region);
            if (type < 0)
            {
                return type;
            }

            if (type == eHNFFS_SECTOR_BFILE)
            {
                // Do migrate.
                err = eHNFFS_region_migration(eHNFFS, region, (eHNFFS_size_t)type);
                if (err)
                {
                    return err;
                }
            }
            begin--;
            continue;
        }
    }

    return err;
}

/**
 * Change reserve to use in region map.
 */
int eHNFFS_reserve_region_use(eHNFFS_region_map_ram_t *region_map, int type)
{
    int err = eHNFFS_ERR_OK;

    // Checkout reserve region is valid to use.
    eHNFFS_size_t region = region_map->reserve;
    if (region == eHNFFS_NULL)
    {
        return eHNFFS_ERR_INVAL;
    }
    region_map->reserve = eHNFFS_NULL;

    // Change corresponded region.
    uint32_t *region_buffer = (type == eHNFFS_SECTOR_DIR) ? region_map->dir_region
                                                          : region_map->bfile_region;
    int i = region % eHNFFS_SIZE_T_NUM;
    int j = region / eHNFFS_SIZE_T_NUM;
    region_buffer[i] &= ~(1U << j);

    // Change corresponded region count.
    eHNFFS_size_t *region_num = (type == eHNFFS_SECTOR_DIR) ? &region_map->dir_region_num
                                                            : &region_map->bfile_region_num;
    *region_num++;

    // Set change flag to truth.
    region_map->change_flag = 1;
    return err;
}

/**
 * Remove region from its region map.
 */
int eHNFFS_remove_region(eHNFFS_t *eHNFFS, eHNFFS_region_map_ram_t *region_map, int type,
                         eHNFFS_size_t region)
{
    int err = eHNFFS_ERR_OK;

    uint32_t *map_buffer = (type == eHNFFS_SECTOR_DIR) ? region_map->dir_region
                                                       : region_map->bfile_region;
    eHNFFS_size_t *region_num = (type == eHNFFS_SECTOR_DIR) ? &region_map->dir_region_num
                                                            : &region_map->bfile_region_num;
    eHNFFS_off_t *dir_index = (type == eHNFFS_SECTOR_DIR) ? &region_map->dir_index
                                                          : &region_map->bfile_index;

    // If current use region is the region, the change sector map first.
    if (*dir_index == region)
    {
        err = eHNFFS_sector_nextsmap(eHNFFS, eHNFFS->manager, type);
        if (err)
        {
            return err;
        }
    }

    // Change region map message.
    int i = region / eHNFFS_SIZE_T_NUM;
    int j = region % eHNFFS_SIZE_T_NUM;
    map_buffer[i] |= (1U << j); // Set it to free, i.e 1.
    *region_num--;
    region_map->change_flag = 1;
    return err;
}

/**
 * Flush in-ram region map to super block.
 */
int eHNFFS_region_map_flush(eHNFFS_t *eHNFFS, eHNFFS_region_map_ram_t *region_map)
{
    int err = eHNFFS_ERR_OK;

    uint8_t *dest;
    uint8_t *src;

    // Create in-flash region map.
    eHNFFS_size_t len = eHNFFS->manager->region_num / 8;
    eHNFFS_size_t size = sizeof(eHNFFS_region_map_flash_t) + 3 * len;
    eHNFFS_region_map_flash_t *flash_map = eHNFFS_malloc(size);
    if (!flash_map)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    // Assign data for these message.
    flash_map->head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_REGION_MAP, size);
    dest = flash_map->map;
    src = (uint8_t *)region_map->free_region;
    memcpy(dest, src, len);

    src = (uint8_t *)region_map->dir_region;
    dest += len;
    memcpy(dest, src, len);

    src = (uint8_t *)region_map->bfile_region;
    dest += len;
    memcpy(dest, src, len);

    // Prog to pcache first.
    err = eHNFFS_super_cache_prog(eHNFFS, eHNFFS->superblock, eHNFFS->pcache,
                                  flash_map, size);
    if (err)
    {
        goto cleanup;
    }

cleanup:
    if (flash_map)
        eHNFFS_free(flash_map);
    return err;
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * ----------------------------------------------------------    Basic map operations    ---------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Flush in-ram (sector/id) map message into flash.
 * len is the size of a region.
 */
int eHNFFS_map_flush(eHNFFS_t *eHNFFS, eHNFFS_map_ram_t *map, eHNFFS_size_t len)
{
    int err = eHNFFS_ERR_OK;

    if (map->region == eHNFFS_NULL)
    {
        return err;
    }

    eHNFFS_size_t sector = map->begin;
    eHNFFS_size_t off = map->off + map->region * len;
    while (off >= eHNFFS->cfg->sector_size)
    {
        // Turn (begin, off) to (sector, off)
        sector++;
        off -= eHNFFS->cfg->sector_size;
    }

    // Flushing of map is different from cache_cmp
    err = eHNFFS->cfg->prog(eHNFFS->cfg, sector, off, map->buffer, len);
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    // TODO, may useful or maynot
    // memset(map->buffer, eHNFFS_NULL, len);
    // map->region = eHNFFS_NULL;
    // map->index = eHNFFS_NULL;
    // map->free_num = 0;
    return err;
}

/**
 * The in-ram map change function.
 * Notice that bits_num is how many sectors in a region(or how many id in a ram buffer),
 * so the true size should divide 8.
 */
int eHNFFS_ram_map_change(eHNFFS_t *eHNFFS, eHNFFS_size_t region, eHNFFS_size_t bits_num,
                          eHNFFS_map_ram_t *map)
{
    int err = eHNFFS_ERR_OK;
    map->region = region;
    map->index = 0;

    eHNFFS_size_t i = map->begin;
    eHNFFS_off_t j = map->region * bits_num / 8;

    while (j >= eHNFFS->cfg->sector_size)
    {
        // Turn (begin, off) to (sector off'), off could >= 4KB, but off' < 4KB.
        i++;
        j -= eHNFFS->cfg->sector_size;
    }

    // Read function.
    err = eHNFFS->cfg->read(eHNFFS->cfg, i, j, map->buffer, bits_num / 8);
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    map->free_num = 0;
    i = 0;
    j = 0;
    for (int cnt = 0; cnt < bits_num; cnt++)
    {
        if ((map->buffer[i] >> j) & 1U)
            map->free_num++;

        j++;
        if (j == 32)
        {
            i++;
            j = 0;
        }
    }
    return err;
}

/**
 * Find the sequential num of sectors in map, return the first sector to begin.
 */
int eHNFFS_find_in_map(eHNFFS_t *eHNFFS, eHNFFS_size_t bits_num, eHNFFS_map_ram_t *map,
                       eHNFFS_size_t num, eHNFFS_size_t *begin)
{
    int err = eHNFFS_ERR_OK;

    // There are 32 bits in eHNFFS_size_t
    int max = bits_num / eHNFFS_SIZE_T_NUM;
    int i = map->index / eHNFFS_SIZE_T_NUM;
    int j = map->index % eHNFFS_SIZE_T_NUM;

    int cnt = 0;
    for (; i < max;)
    {

        for (; j < eHNFFS_SIZE_T_NUM; j++)
        {
            // Find num sequential sectors to use.
            if ((map->buffer[i] >> j) & 1)
            {
                cnt++;
                if (cnt == num)
                {
                    j++;
                    break;
                }
            }
            else
                cnt = 0;
        }

        if (j == eHNFFS_SIZE_T_NUM)
        {
            j = 0;
            i++;
        }

        if (cnt == num)
        {
            break;
        }
    }

    // There are two cases when we change bit map
    // 1. In the same eHNFFS_size_t, i.e map->buffer[0/1]
    // 2. In map->buffer[0] and in map->buffer[1]
    if (cnt == num)
    {
        eHNFFS_size_t origin = i * 32 + j - num;
        eHNFFS_size_t temp = 0;

        // If it's case 2, turn map->buffer[0] first.
        if (i > 0 && j < num)
        {
            for (int k = 0; k < num - j; k++)
                temp = (temp >> 1) | (1U << 31);
            map->buffer[0] &= ~temp;
            origin = 32 * i;
        }

        // Turn others.
        temp = 0;
        for (int k = 0; k < i * 32 + j - origin; k++)
        {
            temp = (temp << 1) | 1U;
        }
        map->buffer[i] &= ~(temp << ((j < num) ? 0 : j - num));

        // Change other things.
        map->free_num -= num;
        map->index = i * eHNFFS_SIZE_T_NUM + j;
        *begin = bits_num * map->region + map->index - num;

        return cnt;
    }

    if (cnt == 0)
        *begin = eHNFFS_NULL;
    else
        *begin = bits_num * (map->region + 1) - cnt;
    return cnt;
}

/**
 * When using reserve region, we should set sectors to using in map by ourself.
 */
int eHNFFS_directly_map_using(eHNFFS_t *eHNFFS, eHNFFS_size_t sector, eHNFFS_size_t off,
                              eHNFFS_size_t begin, eHNFFS_size_t num)
{
    int err = eHNFFS_ERR_OK;

    // Calculate the size we need to program.
    // The begin may not aligned to 32 bits, so we should prog one more eHNFFS_size_t to make
    // sure all changes has proged into flash.
    eHNFFS_size_t size = (eHNFFS_alignup(num, eHNFFS_SIZE_T_NUM) / eHNFFS_SIZE_T_NUM + 1) *
                         sizeof(eHNFFS_size_t);
    uint32_t *buffer = eHNFFS_malloc(size);
    if (!buffer)
    {
        return eHNFFS_ERR_NOMEM;
    }
    memset(buffer, eHNFFS_NULL, size);

    // Turn corresponded bits to 0.
    int i = 0;
    int j = begin % eHNFFS_SIZE_T_NUM;
    for (int k = 0; k < num; k++)
    {
        buffer[i] &= ~(1U << j);
        j++;
        if (j == eHNFFS_SIZE_T_NUM)
        {
            i++;
            j = 0;
        }
    }

    // Calculate address we start to prog.
    off = off + (begin / eHNFFS_SIZE_T_NUM) * sizeof(eHNFFS_size_t);
    while (off >= eHNFFS->cfg->sector_size)
    {
        sector++;
        off -= eHNFFS->cfg->sector_size;
    }

    // Prog all changes to flash.
    uint8_t *data = (uint8_t *)buffer;
    while (size > 0)
    {
        eHNFFS_size_t len = eHNFFS_min(eHNFFS->cfg->sector_size - off, size);
        err = eHNFFS->cfg->prog(eHNFFS->cfg, sector, off, data, len);
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            goto cleanup;
        }

        size -= len;
        data += len;
        off += len;
        if (off == eHNFFS->cfg->sector_size)
        {
            sector++;
            off = 0;
        }
    }

cleanup:
    eHNFFS_free(buffer);
    return err;
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * ----------------------------------------------------------    Sector map operations    --------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Assign basic sector map message with sector addr data.
 */
int eHNFFS_sectormap_assign(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager,
                            eHNFFS_mapaddr_flash_t *map_addr, eHNFFS_size_t num)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t size;

    // Dir map's (begin, off) and erase times message.
    // Erase times for the three maps are same, so we only record one.
    eHNFFS_map_ram_t *map = manager->dir_map;
    map->begin = map_addr->begin;
    map->off = map_addr->off;
    if (map->etimes == NULL)
    {
        map->etimes = eHNFFS_malloc(num * sizeof(eHNFFS_size_t));
        if (!map->etimes)
        {
            err = eHNFFS_ERR_NOMEM;
            goto cleanup;
        }
    }
    for (int i = 0; i < num; i++)
        map->etimes[i] = map_addr->erase_times[i];

    // Big file map's (begin, off) and erase times message.
    map = manager->bfile_map;
    map->begin = map_addr->begin;
    map->off = map_addr->off;

    // Erase map's (begin, off) and erase times message.
    size = eHNFFS->cfg->sector_count / 8;
    map = manager->erase_map;
    map->begin = map_addr->begin + num / 2;
    map->off = (map_addr->off + size >= eHNFFS->cfg->sector_size)
                   ? 0
                   : map_addr->off + size;
    return err;

cleanup:
    if (manager->dir_map->etimes)
        eHNFFS_free(manager->dir_map->etimes);
    return err;
}

/**
 * Flush sector(dir/bfile/erase) map message into flash.
 */
int eHNFFS_smap_flush(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t len = manager->region_size / 8;
    err = eHNFFS_map_flush(eHNFFS, manager->dir_map, len);
    if (err)
    {
        return err;
    }

    err = eHNFFS_map_flush(eHNFFS, manager->bfile_map, len);
    if (err)
    {
        return err;
    }

    // Index in erase_map is a flag tells us if there are changes in the map.
    // If not, we don't need to flush.
    if (!manager->erase_map->index)
    {
        return err;
    }

    err = eHNFFS_map_flush(eHNFFS, manager->erase_map, len);
    return err;
}

/**
 * Change in-flash sector map.
 */
int eHNFFS_smap_change(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager,
                       eHNFFS_cache_ram_t *pcache, eHNFFS_cache_ram_t *rcache)
{
    int err = eHNFFS_ERR_OK;

    err = eHNFFS_smap_flush(eHNFFS, manager);
    if (err)
    {
        return err;
    }

    err = eHNFFS_cache_flush(eHNFFS, pcache, rcache, eHNFFS_VALIDATE);
    if (err)
    {
        return err;
    }

    eHNFFS_map_ram_t *map = manager->dir_map;
    eHNFFS_size_t old_sector = map->begin;
    eHNFFS_size_t old_off = map->off;
    eHNFFS_size_t need_space = 2 * eHNFFS->cfg->sector_count / 8;
    eHNFFS_size_t num = eHNFFS_alignup(need_space, eHNFFS->cfg->sector_size) /
                        eHNFFS->cfg->sector_size;
    if (map->off + need_space >= num * eHNFFS->cfg->sector_size)
    {
        // We need to find new sector to store map message.
        eHNFFS_size_t new_begin = eHNFFS_NULL;
        eHNFFS_size_t etimes[num];
        err = eHNFFS_sector_alloc(eHNFFS, manager, eHNFFS_SECTOR_MAP, num, eHNFFS_NULL,
                                  eHNFFS_NULL, eHNFFS_NULL, &new_begin, etimes);
        if (err)
        {
            return err;
        }

        err = eHNFFS_old_msector_erase(eHNFFS, num, map->begin, map->etimes);
        if (err)
        {
            return err;
        }

        for (int i = 0; i < num; i++)
        {
            err = eHNFFS_sector_erase(eHNFFS, new_begin + num);
            if (err)
            {
                return err;
            }
        }

        // Update basic map message.
        manager->dir_map->begin = new_begin;
        manager->bfile_map->begin = new_begin;
        manager->erase_map->begin = new_begin + num / 2;

        manager->dir_map->off = 0;
        manager->bfile_map->off = 0;
        manager->erase_map->off = (need_space >= 2 * eHNFFS->cfg->sector_size)
                                      ? 0
                                      : need_space / 2;

        for (int i = 0; i < num; i++)
        {
            map->etimes[i] = etimes[i];
        }
    }
    else
    {
        // Update basic map message.
        manager->dir_map->off += need_space;
        manager->bfile_map->off += need_space;
        manager->erase_map->off += need_space;
    }

    // Now we need to use both pcache and rcache.
    need_space /= 2;
    eHNFFS_size_t new_sector = map->begin;
    eHNFFS_size_t old_sector2 = old_sector + num / 2;
    eHNFFS_off_t off = map->off;
    eHNFFS_off_t old_off2 = (old_off + need_space >= eHNFFS->cfg->sector_size)
                                ? 0
                                : old_off + need_space;
    while (need_space > 0)
    {
        // Read free map and erase map to pcache and rcache.
        eHNFFS_size_t size = eHNFFS_min(eHNFFS->cfg->sector_size - off,
                                        eHNFFS_min(need_space, eHNFFS->cfg->cache_size));
        err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, pcache, old_sector, old_off, size);
        if (err)
        {
            return err;
        }

        err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, rcache, old_sector2, old_off2, size);
        if (err)
        {
            return err;
        }

        // Emerge into one free map.
        eHNFFS_size_t *data1 = (eHNFFS_size_t *)pcache->buffer;
        eHNFFS_size_t *data2 = (eHNFFS_size_t *)rcache->buffer;
        for (int i = 0; i < size; i += sizeof(eHNFFS_size_t))
        {
            // XNOR operation
            *data1 = ~(*data1 ^ *data2);
            data1++;
            data2++;
        }

        // Program the new free map into flash, we don't need to program anything to erase map.
        err = eHNFFS->cfg->prog(eHNFFS->cfg, new_sector, off, pcache->buffer, size);
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            return err;
        }

        off += size;
        old_off += size;
        old_off2 += size;
        if (old_off >= eHNFFS->cfg->sector_size)
        {
            off = 0;
            old_off = 0;
            old_off2 = 0;
            new_sector++;
            old_sector++;
            old_sector2++;
        }
        need_space -= size;
    }

    eHNFFS_cache_one(eHNFFS, pcache);
    eHNFFS_size_t len = sizeof(eHNFFS_mapaddr_flash_t) + num * sizeof(eHNFFS_size_t);
    eHNFFS_mapaddr_flash_t *addr = eHNFFS_malloc(len);
    if (addr == NULL)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    addr->head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_SECTOR_MAP, len);
    addr->begin = map->begin;
    addr->off = map->off;
    for (int i = 0; i < num; i++)
    {
        addr->erase_times[i] = map->etimes[i];
    }
    err = eHNFFS_addr_prog(eHNFFS, eHNFFS->superblock, addr, len);

cleanup:
    if (addr != NULL)
        eHNFFS_free(addr);
    return err;
}

/**
 * Find next region of sector map to scan.
 * Type could be dir or reg(i.e big file/some super messages)
 */
int eHNFFS_sector_nextsmap(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager,
                           int type)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t *region_num = (type == eHNFFS_SECTOR_DIR)
                                    ? &manager->region_map->dir_region_num
                                    : &manager->region_map->bfile_region_num;
    eHNFFS_off_t *region_index = (type == eHNFFS_SECTOR_DIR)
                                     ? &manager->region_map->dir_index
                                     : &manager->region_map->bfile_index;
    uint32_t *region_buffer = (type == eHNFFS_SECTOR_DIR)
                                  ? manager->region_map->dir_region
                                  : manager->region_map->bfile_region;
    eHNFFS_map_ram_t *map = (type == eHNFFS_SECTOR_DIR)
                                ? manager->dir_map
                                : manager->bfile_map;
    eHNFFS_region_map_ram_t *region_map = manager->region_map;

    // Flushing valid data to nor flash first.
    if (map->region != eHNFFS_NULL)
    {
        err = eHNFFS_map_flush(eHNFFS, map, manager->region_size / 8);
        if (err)
        {
            return err;
        }
    }

    if (manager->scan_times < eHNFFS_WL_START)
    {
        // When first scan flash, reserve is default to region_num - 1.
        // So we can use it as index when scan_times is 0.
        if (manager->scan_times == 0 &&
            region_map->reserve != manager->region_num - 1)
        {
            map->free_num = manager->region_size;
            map->region = region_map->reserve;
            map->index = 0;
            memset(map->buffer, eHNFFS_NULL, manager->region_size / 8);

            region_map->change_flag = 1;
            int i = region_map->reserve / 32;
            int j = region_map->reserve % 32;
            region_map->free_region[i] &= ~(1U << j);
            region_map->reserve++;

            *region_num = *region_num + 1;
            *region_index = region_map->reserve;
            region_buffer[i] &= ~(1U << j);

            return err;
        }

        eHNFFS_size_t i = *region_index / eHNFFS_SIZE_T_NUM;
        eHNFFS_size_t j = *region_index % eHNFFS_SIZE_T_NUM;
        while (true)
        {
            if (*region_index == map->region)
            {
                // Steal from the other region map.
                err = eHNFFS_ERR_NOSPC;
                return err;
            }

            // Find the next used region.
            // In nor flash, bit 0 means used, bit 1 means not used.
            if (!((region_buffer[i] >> j) & 1U))
            {
                err = eHNFFS_ram_map_change(eHNFFS, *region_index, manager->region_size, map);
                *region_index = *region_index + 1;
                return err;
            }

            // Find the next region to use
            *region_index = *region_index + 1;
            j++;
            if (j == eHNFFS_SIZE_T_NUM)
            {
                i++;
                j = 0;
            }

            // If we traverse it once, then we should change in-flash map.
            if (*region_index == manager->region_num)
            {
                i = 0;
                j = 0;
                *region_index = 0;
                err = eHNFFS_smap_change(eHNFFS, manager, eHNFFS->pcache, eHNFFS->rcache);
                if (err)
                {
                    printf("err is %d\n", err);
                    return err;
                }
            }
        }
    }
    else if (manager->scan_times == eHNFFS_WL_START ||
             manager->scan_times == eHNFFS_WL_ING)
    {
        // Change sector map with wl module.
        eHNFFS_wl_ram_t *wl = manager->wl;
        eHNFFS_size_t *index = (type == eHNFFS_SECTOR_DIR)
                                   ? &wl->index
                                   : &wl->bfile_index;
        eHNFFS_size_t index_in_regions = *index % eHNFFS_RAM_REGION_NUM;
        eHNFFS_wl_message_t *regions = (type == eHNFFS_SECTOR_DIR)
                                           ? wl->regions
                                           : wl->bfile_regions;
        if (index_in_regions == 0 && *index != 0)
        {
            // Flush wl added message.
            err = eHNFFS_wladd_flush(eHNFFS, manager, regions);
            if (err)
            {
                return err;
            }

            // Now we should change the in-flash map.
            if (*index == wl->threshold)
            {
                err = eHNFFS_smap_change(eHNFFS, manager, eHNFFS->pcache, eHNFFS->rcache);
                if (err)
                {
                    return err;
                }
                *index = 0;
            }

            // Read new regions into wl module.
            eHNFFS_off_t off = 2 * sizeof(eHNFFS_head_t) +
                               *index * sizeof(eHNFFS_wl_message_t);
            err = eHNFFS->cfg->read(eHNFFS->cfg, wl->begin, off, regions,
                                    eHNFFS_RAM_REGION_NUM * sizeof(eHNFFS_wl_message_t));
            eHNFFS_ASSERT(err <= 0);
            if (err)
            {
                return err;
            }

            // We need to record the add message, so turn etimes to 0 first.
            for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
            {
                regions[i].erase_times = 0;
            }
        }

        // Change in-ram map.
        err = eHNFFS_ram_map_change(eHNFFS, regions[index_in_regions].region,
                                    manager->region_size, map);
        if (err)
        {
            return err;
        }

        (*index)++;
        return err;
    }
    else
    {
        err = eHNFFS_ERR_INVAL;
        return err;
    }
}

/**
 * Allocate sequential sectors.
 */
int eHNFFS_sectors_find(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager, eHNFFS_size_t num,
                        int type, eHNFFS_size_t *begin)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_map_ram_t *map = (type == eHNFFS_SECTOR_DIR)
                                ? manager->dir_map
                                : manager->bfile_map;
    eHNFFS_size_t last_region = map->region;
    eHNFFS_size_t flag = 0;
    eHNFFS_size_t record[2];
    record[1] = eHNFFS_NULL;
    record[0] = 0;
    while (true)
    {
        if (manager->scan_times < eHNFFS_WL_START)
        {
            eHNFFS_size_t region_num = (type == eHNFFS_SECTOR_DIR)
                                           ? manager->region_map->dir_region_num
                                           : manager->region_map->bfile_region_num;
            // It's time to wl.
            if (last_region > map->region && flag > region_num)
            {
                // We scan the map and couldn't find free sector,
                // need to do gc and migrate data.
                if (record[1] == eHNFFS_NULL)
                {
                    return eHNFFS_ERR_NOSPC;
                }

                err = eHNFFS_region_migration(eHNFFS, record[1], type);
                if (err)
                {
                    return err;
                }
            }
            else if (last_region > map->region && flag > 2 * region_num)
            {
                // After gc we still can't find free sector,
                // There is no space in flash.
                return eHNFFS_ERR_NOSPC;
            }
        }
        else
        {
            // If scan time is just eHNFFS_WL_START, it means that we just need to
            // do wl and haven't create wl structure.
            if (manager->scan_times == eHNFFS_WL_START)
            {
                err = eHNFFS_wl_create(eHNFFS, manager, &manager->wl);
                if (err)
                {
                    return err;
                }

                manager->scan_times = eHNFFS_WL_ING;
            }

            eHNFFS_size_t index = (type == eHNFFS_SECTOR_DIR)
                                      ? manager->wl->index
                                      : manager->wl->bfile_index;
            // Sequentially find the next region.
            if (index == manager->wl->threshold && flag > manager->wl->threshold)
            {
                // We scan the map and couldn't find free sector,
                // need to do gc and migrate data.
                if (record[1] == eHNFFS_NULL)
                {
                    return eHNFFS_ERR_NOSPC;
                }

                err = eHNFFS_region_migration(eHNFFS, record[1], type);
                if (err)
                {
                    return err;
                }
            }
            else if (index == manager->wl->threshold &&
                     flag > 2 * manager->wl->threshold)
            {
                return eHNFFS_ERR_NOSPC;
            }
        }

        // There is no more space in the region, we should find a new region.
        if (map->free_num == 0)
        {
            last_region = map->region;
            err = eHNFFS_sector_nextsmap(eHNFFS, manager, type);
            if (err)
            {
                return err;
            }
            flag += 1;
            continue;
        }

        if (map->free_num > record[0])
        {
            record[1] = map->region;
            record[0] = map->free_num;
        }

        err = eHNFFS_find_in_map(eHNFFS, manager->region_size, map, num, begin);
        if (err < 0)
        {
            return err;
        }

        if (*begin != eHNFFS_NULL && err == num)
        {
            err = eHNFFS_ERR_OK;
            break;
        }
        else
        {
            // Debug, TODO
            // Attention here!!!,There must be something wrong here,
            // but now it's no need to worry.

            // In current region, we can get err sequential sectors, but still need rest.
            // So we try to find whether or not we can get it in the next region.
            int rest = num - err;
            int i = (map->region + 1) / eHNFFS_SIZE_T_NUM;
            int j = (map->region + 1) % eHNFFS_SIZE_T_NUM;
            uint32_t *temp_region =
                (type == eHNFFS_SECTOR_DIR) ? manager->region_map->dir_region
                                            : manager->region_map->bfile_region;
            if (!(manager->region_map->free_region[i] & (1U << j) ||
                  temp_region[i] & (1U << j)) &&
                map->region + 1 < 8191)
            {
                *begin = eHNFFS_NULL;
                continue;
            }

            // Read bit map of the next region.
            int buffer[manager->region_size / eHNFFS_SIZE_T_NUM];
            int temp_off = map->off + (map->region + 1) * manager->region_size / 8;
            err = eHNFFS->cfg->read(eHNFFS->cfg, map->begin, temp_off, buffer, manager->region_size / 8);
            eHNFFS_ASSERT(err <= 0);
            if (err < 0)
                return err;

            int temp_cnt = 0;
            for (i = 0; i < manager->region_size / eHNFFS_SIZE_T_NUM; i++)
            {
                for (j = 0; j < eHNFFS_SIZE_T_NUM; j++)
                {
                    if (buffer[i] & (1U << j))
                    {
                        temp_cnt++;
                        if (temp_cnt == rest)
                        {
                            // We can get the rest sector in the region, so turn bits to 0.
                            int temp_begin = *begin;
                            *begin = eHNFFS_NULL;
                            err = eHNFFS_find_in_map(eHNFFS, manager->region_size, map, num - rest, begin);
                            eHNFFS_ASSERT(*begin == temp_begin);
                            if (err < 0)
                                return err;
                            err = eHNFFS_sector_nextsmap(eHNFFS, manager, type);
                            if (err)
                                return err;
                            *begin = eHNFFS_NULL;
                            err = eHNFFS_find_in_map(eHNFFS, manager->region_size, map, rest, begin);
                            eHNFFS_ASSERT(*begin == map->region * manager->region_size);
                            if (err < 0)
                                return err;

                            *begin = temp_begin;
                            return eHNFFS_ERR_OK;
                        }
                    }
                    else
                    {
                        err = eHNFFS_sector_nextsmap(eHNFFS, manager, type);
                        if (err)
                            return err;
                        temp_cnt = -1;
                    }
                }
                if (temp_cnt < 0)
                    break;
            }
        }
    }
    return err;
}

/**
 * Allocate sector to user.
 * Notice that for dir and big file/some super message, the allocate logic is different.
 */
int eHNFFS_sector_alloc(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager, int type,
                        eHNFFS_size_t num, eHNFFS_size_t pre_sector, eHNFFS_size_t id,
                        eHNFFS_size_t father_id, eHNFFS_size_t *begin, eHNFFS_size_t *etimes)
{
    int err = eHNFFS_ERR_OK;

    // get sequential sectors.
    err = eHNFFS_sectors_find(eHNFFS, manager, num, type, begin);
    if (err)
    {
        return err;
    }

    // Check sector heads and erase if needed.
    eHNFFS_size_t sector = *begin;
    eHNFFS_head_t head;
    eHNFFS_size_t current_etimes;
    for (int i = 0; i < num; i++)
    {
        // Read original sector head.
        err = eHNFFS->cfg->read(eHNFFS->cfg, sector, 0, &head, sizeof(eHNFFS_size_t));
        if (err)
        {
            return err;
        }

        // Check if it's valid. Because the valid bit of head is the first bit to
        // erase, so if corrupt happens when erase, we can know it.
        err = eHNFFS_shead_check(head, eHNFFS_NULL, (int)eHNFFS_NULL);
        if (err)
        {
            return err;
        }

        current_etimes = eHNFFS_shead_etimes(head);
        if (current_etimes == eHNFFS_NULL)
            current_etimes = 0;

        switch (eHNFFS_shead_state(head))
        {
        case eHNFFS_SECTOR_FREE:
            // Free sector don't need to erase.
            break;

        case eHNFFS_SECTOR_OLD:
        case eHNFFS_SECTOR_GC:
            // Old sector can reuse after erase,
            // and we think gc sector only can be found after corrupt happens.
            // Because of undo, we reuse the sector directly.
            err = eHNFFS_sector_erase(eHNFFS, sector);
            if (err)
            {
                return err;
            }
            current_etimes++;
            break;

        default:
            return eHNFFS_ERR_WRONGHEAD;
        }

        // sector for map don't need a head, but we should return erase_times.
        if (type == eHNFFS_SECTOR_MAP)
        {
            etimes[i] = current_etimes;
            continue;
        }
        else if (type == eHNFFS_SECTOR_DIR)
        {
            eHNFFS_dir_sector_flash_t dsector = {
                .head = eHNFFS_MKSHEAD(0, eHNFFS_SECTOR_USING, type, 0, current_etimes),
                .pre_sector = pre_sector,
                .id = id,
            };
            eHNFFS_off_t off = 0;
            err = eHNFFS_direct_shead_prog(eHNFFS, sector, &off,
                                           sizeof(eHNFFS_dir_sector_flash_t), &dsector);
            if (err)
            {
                return err;
            }
        }
        else if (type == eHNFFS_SECTOR_BFILE)
        {
            eHNFFS_bfile_sector_flash_t fsector = {
                .head = eHNFFS_MKSHEAD(0, eHNFFS_SECTOR_USING, type, 0, current_etimes),
                .id = id,
                .father_id = father_id,
            };
            eHNFFS_off_t off = 0;
            err = eHNFFS_direct_shead_prog(eHNFFS, sector, &off,
                                           sizeof(eHNFFS_dir_sector_flash_t), &fsector);
            if (err)
            {
                return err;
            }
        }
        else
        {
            // write the new sector head.
            eHNFFS_off_t off = 0;
            eHNFFS_head_t new_head = eHNFFS_MKSHEAD(0, eHNFFS_SECTOR_USING, type,
                                                    0, current_etimes);
            err = eHNFFS_direct_shead_prog(eHNFFS, sector, &off,
                                           sizeof(eHNFFS_head_t), &new_head);
            if (err)
            {
                return err;
            }
        }

        sector++;
    }
    return err;
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * ----------------------------------------------------------    Erase map operations    --------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Set sectors in erase map to 0 so they can reuse in the future.
 */
int eHNFFS_emap_set(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager,
                    eHNFFS_size_t begin, eHNFFS_size_t num)
{
    int err = eHNFFS_ERR_OK;

    // Get the basic information.
    eHNFFS_map_ram_t *map = manager->erase_map;
    eHNFFS_size_t region = begin / manager->region_size;
    eHNFFS_size_t index = begin % manager->region_size;

    // If current erase map has valid data and it's not the region we
    // want to prog, we should flush it to flash first.
    if (region != map->region && map->index)
    {
        err = eHNFFS_map_flush(eHNFFS, map, manager->region_size / 8);
        if (err)
        {
            return err;
        }

        memset(map->buffer, -1, manager->region_size / 8);
        map->region = region;
        map->index = 0;
        map->free_num = 0;
    }

    // Turn bit to 0.
    int i = index / eHNFFS_SIZE_T_NUM;
    int j = index % eHNFFS_SIZE_T_NUM;
    for (int k = 0; k < num; k++)
    {
        map->buffer[i] &= ~(1U << j);
        map->index = 1;

        j++;
        if (j == eHNFFS_SIZE_T_NUM)
        {
            // If j is up to 32.
            i++;
            j = 0;
            if (i == manager->region_size / eHNFFS_SIZE_T_NUM)
            {
                // If i is up to region size, i.e it's time to next region.
                err = eHNFFS_map_flush(eHNFFS, map, manager->region_size / 8);
                if (err)
                {
                    return err;
                }
                memset(map->buffer, -1, manager->region_size / 8);
                map->region++;
                map->index = 0;
                i = 0;
                j = 0;
            }
        }
    }

    return err;
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * ------------------------------------------------------------    ID map operations    ----------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Initialization function of id map.
 */
int eHNFFS_id_map_initialization(eHNFFS_t *eHNFFS, eHNFFS_idmap_ram_t **map_addr)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t size;

    // Allocate memory for map
    eHNFFS_idmap_ram_t *map = eHNFFS_malloc(sizeof(eHNFFS_idmap_ram_t));
    if (!map)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    // Initialize basic message.
    map->max_id = eHNFFS_ID_MAX;
    map->bits_in_buffer = eHNFFS_ID_MAX / eHNFFS_REGION_NUM_MAX;

    // Allocate free id map.
    size = map->bits_in_buffer / 8;
    err = eHNFFS_map_initialization(eHNFFS, &map->free_map, size);
    if (err)
    {
        goto cleanup;
    }

    // Allocate remove id map.
    err = eHNFFS_map_initialization(eHNFFS, &map->remove_map, size);
    if (err)
    {
        goto cleanup;
    }

    *map_addr = map;
    return err;

cleanup:
    if (map)
    {
        if (map->free_map)
            eHNFFS_free(map->free_map);

        if (map->remove_map)
            eHNFFS_free(map->remove_map);

        eHNFFS_free(map);
    }
    return err;
}

/**
 * Assign basic id map message with id addr data.
 */
int eHNFFS_idmap_assign(eHNFFS_t *eHNFFS, eHNFFS_idmap_ram_t *id_map, eHNFFS_mapaddr_flash_t *map_addr,
                        eHNFFS_size_t num)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t size;

    // Free map message.
    eHNFFS_map_ram_t *map = id_map->free_map;
    map->begin = map_addr->begin;
    map->off = map_addr->off;
    if (map->etimes == NULL)
    {
        map->etimes = eHNFFS_malloc(num * sizeof(eHNFFS_size_t));
        if (!map->etimes)
        {
            err = eHNFFS_ERR_NOMEM;
            goto cleanup;
        }
    }
    for (int i = 0; i < num; i++)
        map->etimes[i] = map_addr->erase_times[i];

    // Remove map message.
    size = eHNFFS_ID_MAX / 8;
    map = id_map->remove_map;
    map->begin = map_addr->begin + num / 2;
    map->off = (map_addr->off + size >= eHNFFS->cfg->sector_size)
                   ? 0
                   : map_addr->off + size;
    return err;

cleanup:
    if (id_map->free_map->etimes)
        eHNFFS_free(id_map->free_map->etimes);
    return err;
}

/**
 * Flush in-ram id map(free and erase map) to flash.
 */
int eHNFFS_idmap_flush(eHNFFS_t *eHNFFS, eHNFFS_idmap_ram_t *id_map)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t len = id_map->bits_in_buffer / 8;
    err = eHNFFS_map_flush(eHNFFS, id_map->free_map, len);
    if (err)
    {
        return err;
    }

    if (!id_map->remove_map->index)
    {
        return err;
    }

    err = eHNFFS_map_flush(eHNFFS, id_map->remove_map, len);
    return err;
}

/**
 * Change the in-flash id map with old id map.
 */
int eHNFFS_idmap_change(eHNFFS_t *eHNFFS, eHNFFS_idmap_ram_t *id_map,
                        eHNFFS_cache_ram_t *pcache, eHNFFS_cache_ram_t *rcache)
{
    int err = eHNFFS_ERR_OK;

    // Flush in-ram message into flash first.
    err = eHNFFS_idmap_flush(eHNFFS, id_map);
    if (err)
    {
        return err;
    }

    err = eHNFFS_cache_flush(eHNFFS, pcache, NULL, eHNFFS_VALIDATE);
    if (err)
    {
        return err;
    }

    // eHNFFS_size_t len = id_map->bits_in_buffer / 8;
    eHNFFS_map_ram_t *map = id_map->free_map;
    eHNFFS_size_t old_sector = map->begin;
    eHNFFS_off_t old_off = map->off;
    eHNFFS_size_t need_space = 2 * eHNFFS_ID_MAX / 8;
    if (map->off + 2 * need_space >= eHNFFS->cfg->sector_size)
    {
        // We need to find new sector to store map message,
        // and only need one sector.
        eHNFFS_size_t new_begin = eHNFFS_NULL;
        eHNFFS_size_t etimes;
        err = eHNFFS_sector_alloc(eHNFFS, eHNFFS->manager, eHNFFS_SECTOR_MAP, 1, eHNFFS_NULL,
                                  eHNFFS_NULL, eHNFFS_NULL, &new_begin, &etimes);
        if (err)
        {
            return err;
        }

        // Erase the old sector belongs to id map.
        err = eHNFFS_old_msector_erase(eHNFFS, 1, map->begin, map->etimes);
        if (err)
        {
            return err;
        }

        // Erase all message in old map sectors.
        err = eHNFFS_sector_erase(eHNFFS, new_begin);
        if (err)
        {
            return err;
        }

        // Change basic in-ram message.
        id_map->free_map->begin = new_begin;
        id_map->remove_map->begin = new_begin;

        id_map->free_map->off = 0;
        id_map->remove_map->off = need_space / 2;

        map->etimes[0] = etimes;
    }
    else
    {
        // Change basic in-ram message.
        id_map->free_map->off += need_space;
        id_map->remove_map->off += need_space;
    }

    need_space /= 2;
    eHNFFS_off_t off = id_map->free_map->off;
    eHNFFS_off_t old_off2 = old_off + need_space;
    while (need_space > 0)
    {
        // Read free map and erase map to pcache and rcache.
        eHNFFS_size_t size = eHNFFS_min(need_space, eHNFFS->cfg->cache_size);
        err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, pcache, old_sector, old_off, size);
        if (err)
        {
            return err;
        }

        err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, rcache, old_sector, old_off2, size);
        if (err)
        {
            return err;
        }

        // Emerge into one free map.
        eHNFFS_size_t *data1 = (eHNFFS_size_t *)pcache->buffer;
        eHNFFS_size_t *data2 = (eHNFFS_size_t *)rcache->buffer;
        for (int i = 0; i < size; i += sizeof(eHNFFS_size_t))
        {
            *data1 |= ~(*data2);
            data1++;
            data2++;
        }

        // Program the new free map into flash, we don't need to program anything to erase map.
        err = eHNFFS->cfg->prog(eHNFFS->cfg, id_map->free_map->begin, off, eHNFFS->pcache->buffer, size);
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            return err;
        }

        off += size;
        old_off += size;
        old_off2 += size;
    }

    // After using pcache, we should reset data in it.
    eHNFFS_cache_one(eHNFFS, pcache);

    // Write the address of id map into superblock.
    eHNFFS_size_t len = sizeof(eHNFFS_mapaddr_flash_t) + sizeof(eHNFFS_size_t);
    eHNFFS_mapaddr_flash_t addr = {
        .head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_SECTOR_MAP, len),
        .begin = map->begin,
        .off = map->off,
    };
    addr.erase_times[0] = map->etimes[0];
    err = eHNFFS_addr_prog(eHNFFS, eHNFFS->superblock, &addr, len);
    return err;
}

/**
 * Allocate a id.
 */
int eHNFFS_id_alloc(eHNFFS_t *eHNFFS, eHNFFS_size_t *id)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_map_ram_t *idmap = eHNFFS->id_map->free_map;
    eHNFFS_size_t last_region = (idmap->region == 0) ? eHNFFS_ID_REGIONS
                                                     : idmap->region - 1;
    while (true)
    {
        if (idmap->free_num > 0)
        {
            err = eHNFFS_find_in_map(eHNFFS, eHNFFS->id_map->bits_in_buffer,
                                     idmap, 1, id);
            if (err < 0)
            {
                return err;
            }
            err = eHNFFS_ERR_OK;

            if (*id != eHNFFS_NULL)
            {
                return err;
            }
        }

        eHNFFS_size_t new = idmap->region + 1;
        if (new == eHNFFS_ID_REGIONS)
        {
            // new map
            err = eHNFFS_idmap_change(eHNFFS, eHNFFS->id_map,
                                      eHNFFS->pcache, eHNFFS->rcache);
            if (err)
            {
                return err;
            }
            new = 0;
        }

        err = eHNFFS_ram_map_change(eHNFFS, new,
                                    eHNFFS->id_map->bits_in_buffer, idmap);
        if (err)
        {
            return err;
        }

        if (new == last_region && idmap->free_num == 0)
        {
            err = eHNFFS_ERR_NOID;
            return err;
        }
    }
}

/**
 * Free id in id map.
 */
int eHNFFS_id_free(eHNFFS_t *eHNFFS, eHNFFS_idmap_ram_t *idmap, eHNFFS_size_t id)
{
    int err = eHNFFS_ERR_OK;

    // Calculate the region id belongs to.
    eHNFFS_size_t region = id / idmap->bits_in_buffer;
    eHNFFS_map_ram_t *remove_map = idmap->remove_map;

    // If region in remove map is not right, then flush and change to corresponding region.
    if (remove_map->region != region)
    {
        if (remove_map->index && remove_map->region != eHNFFS_NULL)
        {
            // index in remove map is changed flag.
            err = eHNFFS_map_flush(eHNFFS, remove_map, idmap->bits_in_buffer / 8);
            if (err)
            {
                return err;
            }
        }

        // We don't need to know true data in flash,
        // because bits 0 in flash couldn't be change to 1 by proging.
        remove_map->region = region;
        memset(remove_map->buffer, eHNFFS_NULL, idmap->bits_in_buffer / 8);
    }

    // Record the id.
    int i = (id % idmap->bits_in_buffer) / eHNFFS_SIZE_T_NUM;
    int j = (id % idmap->bits_in_buffer) % eHNFFS_SIZE_T_NUM;
    remove_map->buffer[i] &= ~(1U << j);
    remove_map->index = 1; // Changed flag in remove map.
    return err;
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * -----------------------------------------------------------    wl heap operations    ----------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * wl message sorted by heaps.
 */
int eHNFFS_wl_heaps_sort(eHNFFS_t *eHNFFS, eHNFFS_size_t num, eHNFFS_wl_message_t *heaps)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t son;
    // Initialize heaps.
    for (int i = num / 2 - 1; i >= 0; i--)
    {
        for (int j = i; 2 * j + 1 < num; j = son)
        {
            son = 2 * j + 1;
            if (son + 1 < num &&
                heaps[son].erase_times < heaps[son + 1].erase_times)
            {
                // If i have a right son, and the erase time of it is smaller
                // than left son. Change to choose the right son.
                son++;
            }

            if (heaps[i].erase_times < heaps[son].erase_times)
            {
                // If erase time of father is larger than son, then swap.
                eHNFFS_wl_swap(eHNFFS, &heaps[i], &heaps[son]);
            }
        }
    }

    // Sort the heap into an ascending series.
    eHNFFS_wl_message_t last;
    for (int i = num - 1; i >= 0;)
    {
        last.region = heaps[i].region;
        last.erase_times = heaps[i].erase_times;
        heaps[i].region = heaps[0].region;
        heaps[i].erase_times = heaps[0].erase_times;

        i--;
        son = 0;
        for (int j = 0; 2 * j + 1 <= i; j = son)
        {
            son = 2 * j + 1;
            if (son + 1 <= i &&
                heaps[son].erase_times < heaps[son + 1].erase_times)
            {
                son++;
            }

            if (last.erase_times < heaps[son].erase_times)
            {
                heaps[j].region = heaps[son].region;
                heaps[j].erase_times = heaps[son].erase_times;
            }
            else
            {
                heaps[j].region = last.region;
                heaps[j].erase_times = last.erase_times;
                last.region = heaps[son].region;
                last.erase_times = heaps[son].erase_times;
            }
        }
        heaps[son].region = last.region;
        heaps[son].erase_times = last.erase_times;
    }

    return err;
}

/**
 * Add in ram wl etimes message during resort.
 */
void eHNFFS_wl_ram_etimes(eHNFFS_t *eHNFFS, eHNFFS_wl_message_t *wlarr,
                          eHNFFS_wl_ram_t *wl)
{

    eHNFFS_wl_message_t *message = wl->regions;
    for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
    {
        wlarr[message[i].region].erase_times += message[i].erase_times;
    }

    message = wl->bfile_regions;
    for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
    {
        wlarr[message[i].region].erase_times += message[i].erase_times;
    }

    message = wl->exten_regions;
    for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
    {
        if (message[i].region != eHNFFS_NULL)
        {
            wlarr[message[i].region].erase_times += message[i].erase_times;
        }
    }
}

/**
 * Resort function of in-flash wl array and wl add message.
 * Only used when there is no more space for wl add message to write.
 */
int eHNFFS_wl_resort(eHNFFS_t *eHNFFS, eHNFFS_wl_ram_t *wl)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t sector;
    eHNFFS_size_t off;
    eHNFFS_size_t state;
    eHNFFS_size_t cnt;
    eHNFFS_wl_message_t *wl_message;
    eHNFFS_wladdr_flash_t wladdr;

    eHNFFS_cache_ram_t *rcache = eHNFFS->rcache;
    eHNFFS_size_t region_num = eHNFFS->manager->region_num;
    eHNFFS_size_t new_sector;

    // Allocate ram memory for to be sorted heaps.
    eHNFFS_wlarray_flash_t *heaps = NULL;
    int len = sizeof(eHNFFS_head_t) +
              region_num * sizeof(eHNFFS_wl_message_t);
    heaps = eHNFFS_malloc(len);
    if (!heaps)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    // Initialize heaps.
    for (int i = 0; i < region_num; i++)
    {
        heaps->array[i].region = i;
        heaps->array[i].erase_times = 0;
    }

    sector = wl->begin;
    off = 0;
    state = 0;
    cnt = region_num;
    while (sector - wl->begin < eHNFFS_WL_SECTOR_NUM)
    {
        // Sequentialy read data from sectors that belongs to wl.
        eHNFFS_size_t size = eHNFFS_min(eHNFFS->cfg->cache_size,
                                        eHNFFS->cfg->sector_size - off);
        err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, rcache,
                                   sector, off, size);
        if (err)
        {
            return err;
        }

        // Checkout sector head.
        if (off == 0)
        {
            err = eHNFFS_shead_check(*(eHNFFS_head_t *)rcache->buffer,
                                     eHNFFS_SECTOR_USING, eHNFFS_SECTOR_WL);
            if (err)
            {
                goto cleanup;
            }
            off += sizeof(eHNFFS_head_t);
        }

        uint8_t *p = rcache->buffer + off - rcache->off;
        while (true)
        {
            // Read all message of wl array and wl add in flash.
            if (state == 0)
            {
                // state 0 is the head of wl array.
                err = eHNFFS_dhead_check(*(eHNFFS_head_t *)p, eHNFFS_ID_SUPER,
                                         eHNFFS_DATA_WL_ARRAY);
                if (err)
                {
                    goto cleanup;
                }
                off += sizeof(eHNFFS_head_t);
                p += sizeof(eHNFFS_head_t);
                state++;
            }
            else if (state == 1)
            {
                // state 1 is wl message in wl array
                if (off + sizeof(eHNFFS_wl_message_t) > eHNFFS->cfg->sector_size)
                {
                    // Can't read more integrated message from current sector.
                    sector++;
                    off = 0;
                }
                else if (off - rcache->off + sizeof(eHNFFS_wl_message_t) >
                         eHNFFS->cfg->cache_size)
                {
                    // Can't read more integrated message from current read cache.
                    break;
                }

                wl_message = (eHNFFS_wl_message_t *)p;
                heaps->array[wl_message->region].erase_times += wl_message->erase_times;

                off += sizeof(eHNFFS_wl_message_t);
                p += sizeof(eHNFFS_wl_message_t);
                cnt--;
                if (cnt == 0)
                {
                    state++;
                }
            }
            else if (state == 2)
            {
                // state 2 is eHBFFS_wladd_flash_t structure.
                static eHNFFS_size_t len = sizeof(eHNFFS_head_t) +
                                           sizeof(eHNFFS_wl_message_t) * eHNFFS_RAM_REGION_NUM;
                if (off + len > eHNFFS->cfg->sector_size)
                {
                    // Similar to above.
                    sector++;
                    off = 0;
                }
                else if (off - rcache->off + len > eHNFFS->cfg->cache_size)
                {
                    break;
                }

                // Check wl add head.
                err = eHNFFS_dhead_check(*(eHNFFS_head_t *)p, eHNFFS_ID_SUPER,
                                         eHNFFS_DATA_WL_ADD);
                if (err)
                {
                    goto cleanup;
                }
                p += sizeof(eHNFFS_head_t);

                // Use data behind wl add head to update.
                for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
                {
                    wl_message = (eHNFFS_wl_message_t *)p;
                    heaps->array[wl_message->region].erase_times += wl_message->erase_times;
                    p += sizeof(eHNFFS_wl_message_t);
                }
                off += len;
            }
        }
    }
    eHNFFS_wl_ram_etimes(eHNFFS, heaps->array, wl);

    // Sort all regions in heaps.
    err = eHNFFS_wl_heaps_sort(eHNFFS, region_num, heaps->array);
    if (err)
    {
        goto cleanup;
    }

    // Find new flash space for wl.
    new_sector = eHNFFS_NULL;
    err = eHNFFS_sector_alloc(eHNFFS, eHNFFS->manager, eHNFFS_SECTOR_WL, eHNFFS_WL_SECTOR_NUM,
                              eHNFFS_NULL, eHNFFS_NULL, eHNFFS_NULL, &new_sector, NULL);
    if (err)
    {
        goto cleanup;
    }

    // updata wl module.
    wl->begin = new_sector;
    wl->free_off = sizeof(eHNFFS_head_t);
    wl->index = 0;
    wl->bfile_index = eHNFFS->manager->region_map->bfile_region_num + 1;
    memcpy(wl->regions, heaps->array,
           eHNFFS_RAM_REGION_NUM * sizeof(eHNFFS_wl_message_t));
    memcpy(wl->bfile_regions, &heaps->array[wl->bfile_index],
           eHNFFS_RAM_REGION_NUM * sizeof(eHNFFS_wl_message_t));
    eHNFFS_wlregion_reset(wl->exten_regions);

    // The len in head may larger than limited size, so we don't use it.
    heaps->head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_WL_ARRAY, len);
    err = eHNFFS_direct_data_prog(eHNFFS, wl->begin, &wl->free_off, len, heaps);
    if (err)
    {
        goto cleanup;
    }

    // create new wl message address message.
    len = sizeof(eHNFFS_wladdr_flash_t);
    wladdr.head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_WL_ADDR, len);
    wladdr.begin = wl->begin;

    // Add address of wl sectors to superblock.
    err = eHNFFS_addr_prog(eHNFFS, eHNFFS->superblock, &wladdr, len);
    if (err)
    {
        return err;
    }

cleanup:
    eHNFFS_free(heaps);
    return err;
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * -----------------------------------------------------------    basic wl operations    ---------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Reset wl message of wl module, including regions、bfile_regions、extent_regions
 */
void eHNFFS_wlregion_reset(eHNFFS_wl_message_t *regions)
{
    for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
    {
        regions[i].region = eHNFFS_NULL;
        regions[i].erase_times = 0;
    }
}

/**
 * Swap wl message, used for heap sort funtion.
 */
void eHNFFS_wl_swap(eHNFFS_t *eHNFFS, eHNFFS_wl_message_t *a, eHNFFS_wl_message_t *b)
{
    eHNFFS_wl_message_t temp;
    temp.region = a->region;
    temp.erase_times = a->erase_times;

    a->region = b->region;
    a->erase_times = b->erase_times;

    b->region = temp.region;
    b->erase_times = temp.erase_times;
}

/**
 * Flush wl add message in regions to flash.
 */
int eHNFFS_wladd_flush(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager,
                       eHNFFS_wl_message_t *regions)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_wl_ram_t *wl = manager->wl;

    eHNFFS_size_t len = sizeof(eHNFFS_head_t) +
                        eHNFFS_RAM_REGION_NUM * sizeof(eHNFFS_wl_message_t);
    eHNFFS_size_t sector = wl->begin;
    eHNFFS_off_t off = wl->free_off;
    while (off > eHNFFS->cfg->sector_size)
    {
        // free_off in wl can large than eHNFFS_sector_size, so we should turn
        // (begin, free_off) to normal (sector, off), which off < eHNFFS_sector_size.
        sector++;
        off -= eHNFFS->cfg->sector_size;
    }

    if (eHNFFS->cfg->sector_size - off < len)
    {
        // If rest space of sector is not enough, then find a new one.
        sector++;
        off = sizeof(eHNFFS_head_t);
    }

    if (sector - wl->begin >= eHNFFS_WL_SECTOR_NUM ||
        (sector - wl->begin == eHNFFS_WL_SECTOR_NUM - 1 &&
         wl->free_off + len > eHNFFS->cfg->sector_size))
    {
        // If we have no space to store wl add message, we should read all
        // these message and resort them.
        err = eHNFFS_wl_resort(eHNFFS, wl);
        return err;
    }

    // Make wl add data.
    eHNFFS_wladd_flash_t *temp = eHNFFS_malloc(len);
    temp->head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER,
                                eHNFFS_DATA_WL_ADD, len);
    memcpy(temp->add, regions, len - sizeof(eHNFFS_head_t));

    // Prog it to flash.
    err = eHNFFS_direct_data_prog(eHNFFS, sector, &off, len, temp);
    if (err)
    {
        goto cleanup;
    }

    // Chang free_off message.
    wl->free_off = (sector - wl->begin) * eHNFFS->cfg->sector_size + off;
    eHNFFS_wlregion_reset(regions);

cleanup:
    eHNFFS_free(temp);
    return err;
}

/**
 * Because map don't have sector head, we should calculate its etimes
 * for wl seperataly.
 */
static int eHNFFS_wl_map_etimes(eHNFFS_t *eHNFFS, eHNFFS_map_ram_t *map,
                                eHNFFS_size_t num, eHNFFS_wlarray_flash_t *wlarr)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t sector = map->begin;
    eHNFFS_size_t region;
    eHNFFS_head_t head;
    for (int i = 0; i < num; i++)
    {
        err = eHNFFS->cfg->read(eHNFFS->cfg, sector, 0, &head, sizeof(eHNFFS_head_t));
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            return err;
        }

        region = eHNFFS_aligndown(sector, eHNFFS->manager->region_size) /
                 eHNFFS->manager->region_size;
        wlarr->array[region].erase_times -= eHNFFS_shead_etimes(head);
        wlarr->array[region].erase_times += map->etimes[i];
    }
    return err;
}

/**
 * Create wl module when we should do wear leveling.
 */
int eHNFFS_wl_create(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager,
                     eHNFFS_wl_ram_t **wl_addr)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_head_t head;
    eHNFFS_size_t region;
    eHNFFS_size_t cnt;
    eHNFFS_size_t new_sector;
    eHNFFS_size_t bfile_begin;
    eHNFFS_wladdr_flash_t wladdr;

    eHNFFS_wl_ram_t *wl = eHNFFS_malloc(sizeof(eHNFFS_wl_ram_t));
    if (!wl)
        return eHNFFS_ERR_NOMEM;

    eHNFFS_size_t region_num = manager->region_num;
    eHNFFS_size_t region_size = manager->region_size;
    eHNFFS_size_t len = sizeof(eHNFFS_wlarray_flash_t) +
                        region_num * sizeof(eHNFFS_wl_message_t);
    eHNFFS_wlarray_flash_t *wlarr = eHNFFS_malloc(len);
    if (!wlarr)
    {
        goto cleanup;
    }

    // Notice that len in wlarr head may not big enough to record true length.
    // So we always use len.
    wlarr->head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_WL_ARRAY, len);
    for (int i = 0; i < region_num; i++)
    {
        wlarr->array[i].region = i;
        wlarr->array[i].erase_times = 0;
    }

    region = 0;
    cnt = 0;
    for (int i = 0; i < eHNFFS->cfg->sector_count; i++)
    {
        err = eHNFFS->cfg->read(eHNFFS->cfg, i, 0, &head, sizeof(eHNFFS_head_t));
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            goto cleanup;
        }
        wlarr->array[region].erase_times += eHNFFS_shead_etimes(head);

        cnt++;
        if (cnt == region_size)
        {
            cnt = 0;
            region++;
        }
    }

    // ID map and sector map don't have sector head, so now we should record their etimes.
    err = eHNFFS_wl_map_etimes(eHNFFS, eHNFFS->id_map->free_map, 1, wlarr);
    if (err)
    {
        return err;
    }

    cnt = eHNFFS_aligndown(2 * eHNFFS->cfg->sector_count / 8, eHNFFS->cfg->sector_size) /
          eHNFFS->cfg->sector_size;
    err = eHNFFS_wl_map_etimes(eHNFFS, manager->dir_map, cnt, wlarr);
    if (err)
    {
        return err;
    }

    err = eHNFFS_wl_heaps_sort(eHNFFS, manager->region_num, wlarr->array);
    if (err)
    {
        return err;
    }

    err = eHNFFS_global_region_migration(eHNFFS, manager, wlarr->array);
    if (err)
    {
        return err;
    }

    new_sector = eHNFFS_NULL;
    err = eHNFFS_sector_alloc(eHNFFS, manager, eHNFFS_SECTOR_WL, eHNFFS_WL_SECTOR_NUM,
                              eHNFFS_NULL, eHNFFS_NULL, eHNFFS_NULL, &new_sector, NULL);
    if (err)
    {
        goto cleanup;
    }

    // updata wl module.
    // Threshold divides regions into three parts. dir、bfile and
    // reserve(i.e threshold itself).
    bfile_begin = wl->threshold + 1;
    wl->begin = new_sector;
    wl->free_off = sizeof(eHNFFS_head_t);
    wl->threshold = manager->region_map->dir_region_num;
    wl->index = 0;
    wl->bfile_index = 0;
    eHNFFS_wlregion_reset(wl->regions);
    eHNFFS_wlregion_reset(wl->bfile_regions);
    eHNFFS_wlregion_reset(wl->exten_regions);
    for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
    {
        wl->regions[i].region = wlarr->array[i].region;
        wl->bfile_regions[i].region = wlarr->array[bfile_begin + i].region;
    }

    err = eHNFFS_direct_data_prog(eHNFFS, wl->begin, &wl->free_off, len, wlarr);
    if (err)
    {
        goto cleanup;
    }

    // create new wl message address message.
    len = sizeof(eHNFFS_wladdr_flash_t);
    wladdr.head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_WL_ADDR, len);
    wladdr.begin = wl->begin;

    // Add address of wl sectors to superblock.
    err = eHNFFS_addr_prog(eHNFFS, eHNFFS->superblock, &wladdr, len);
    if (err)
    {
        return err;
    }

    *wl_addr = wl;

cleanup:
    if (wlarr)
    {
        eHNFFS_free(wlarr);
    }
    return err;
}

/**
 * Allocate in-ram memory for wear leveling module.
 * Notice that now we don't have enough message to initialize basic data, it's done later.
 */
int eHNFFS_wl_malloc(eHNFFS_t *eHNFFS, eHNFFS_wl_ram_t *wl, eHNFFS_wladdr_flash_t *wladdr)
{
    int err = eHNFFS_ERR_OK;

    wl = eHNFFS_malloc(sizeof(eHNFFS_wl_ram_t));
    if (!wl)
    {
        return eHNFFS_ERR_NOMEM;
    }

    wl->begin = wladdr->begin;
    wl->free_off = eHNFFS_NULL;
    wl->threshold = eHNFFS->manager->region_map->dir_region_num;
    wl->index = eHNFFS_NULL;
    wl->bfile_index = eHNFFS_NULL;
    memset(wl->regions, eHNFFS_NULL, eHNFFS_RAM_REGION_NUM * sizeof(eHNFFS_size_t));
    memset(wl->bfile_regions, eHNFFS_NULL, eHNFFS_RAM_REGION_NUM * sizeof(eHNFFS_size_t));
    memset(wl->exten_regions, eHNFFS_NULL, eHNFFS_RAM_REGION_NUM * sizeof(eHNFFS_size_t));

    return err;
}

/**
 * Flush all data in wl to flash.
 */
int eHNFFS_wl_flush(eHNFFS_t *eHNFFS, eHNFFS_wl_ram_t *wl)
{
    int err = eHNFFS_ERR_OK;

    // If we record some erase time message in regions, then flush to flash.
    for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
    {
        if (wl->regions[i].erase_times > 0)
        {
            err = eHNFFS_wladd_flush(eHNFFS, eHNFFS->manager, wl->regions);
            if (err)
            {
                return err;
            }
            break;
        }
    }

    // If we record some erase time message in bfile_regions, then flush to flash.
    for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
    {
        if (wl->bfile_regions[i].erase_times > 0)
        {
            err = eHNFFS_wladd_flush(eHNFFS, eHNFFS->manager, wl->bfile_regions);
            if (err)
            {
                return err;
            }
            break;
        }
    }

    // If we record some erase time message in exten_regions, then flush to flash.
    for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
    {
        if (wl->exten_regions[i].erase_times > 0 &&
            wl->exten_regions[i].erase_times != eHNFFS_NULL)
        {
            err = eHNFFS_wladd_flush(eHNFFS, eHNFFS->manager, wl->exten_regions);
            if (err)
            {
                return err;
            }
            break;
        }
    }

    return err;
}
