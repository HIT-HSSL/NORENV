/**
 * The basic cache operations of eHNFFS
 */

#include "eHNFFS_cache.h"
#include "eHNFFS_tree.h"
#include "eHNFFS_head.h"
#include "eHNFFS_gc.h"
#include "eHNFFS_manage.h"

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * -------------------------------------------------------    Auxiliary cache functions    -------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */
/**
 * Initialization function of cache.
 */
int eHNFFS_cache_initialize(eHNFFS_t *eHNFFS, eHNFFS_cache_ram_t **cache_addr, eHNFFS_size_t buffer_size)
{
    int err = eHNFFS_ERR_OK;

    // Malloc memory for cache.
    eHNFFS_cache_ram_t *cache = eHNFFS_malloc(sizeof(eHNFFS_cache_ram_t));
    if (!cache)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    // Malloc memory for cache buffer.
    cache->buffer = eHNFFS_malloc(buffer_size);
    if (!cache->buffer)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    // Reset cache data.
    eHNFFS_cache_one(eHNFFS, cache);
    *cache_addr = cache;
    return err;

cleanup:
    if (cache)
    {
        if (cache->buffer)
            eHNFFS_free(cache->buffer);

        eHNFFS_free(cache);
    }
    return err;
}

/**
 * Empty all data in cache to avoid information leakage, i.e set all bits to 1.
 *
 * The reason to set 1 is that in nor flash, bit 1 is the origin data, and
 * a program operation can turn bit 1 to bit 0.
 */
void eHNFFS_cache_one(eHNFFS_t *eHNFFS, eHNFFS_cache_ram_t *cache)
{
    memset(cache->buffer, 0xff, eHNFFS->cfg->cache_size);
    cache->sector = eHNFFS_NULL;
    cache->off = eHNFFS_NULL;
    cache->size = 0;
    cache->mode = 0;
}

/**
 * Drop the message in cache, a cheaper way than eHNFFS_cache_one.
 */
void eHNFFS_cache_drop(eHNFFS_t *eHNFFS, eHNFFS_cache_ram_t *cache)
{
    (void)eHNFFS;
    cache->sector = eHNFFS_NULL;
    cache->off = eHNFFS_NULL;
    cache->size = 0;
    cache->mode = eHNFFS_NULL;
}

/**
 * Tranverse forward to backward(or backward to forward) of data in cache.
 *
 * We program data backward and file/dir name forward in sector that belongs to dir,
 * so befor writing data to nor flash, we should convert forward to backward first.
 */
int eHNFFS_cache_transition(eHNFFS_t *eHNFFS, void *src_buffer,
                            void *dst_buffer, eHNFFS_size_t size)
{
    int err = eHNFFS_ERR_OK;

    uint8_t *data1, *data2;
    int rest_size;

    // Allocate memory for transition.
    uint8_t *temp_buffer = NULL;
    temp_buffer = eHNFFS_malloc(size);
    if (!temp_buffer)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }
    // uint8_t temp_buffer[size];
    memset(temp_buffer, 0, size);

    data1 = src_buffer;
    data2 = temp_buffer + size;
    eHNFFS_head_t head;
    eHNFFS_size_t len;
    rest_size = size;
    // Every time we write a data in the front of src_buffer to
    // the end of temp_buffer.
    while (rest_size > 0)
    {
        head = *(eHNFFS_head_t *)data1;

        len = eHNFFS_dhead_dsize(head);
        if (head == eHNFFS_NULL ||
            rest_size < sizeof(eHNFFS_head_t) ||
            rest_size < len)
        {
            // eHNFFS_NULL means there is no valid data behind.
            // The other two means that the data is not integrated.
            data2 -= rest_size;
            memcpy(data2, data1, rest_size);
            break;
        }

        // Don't need to check data head, because we may not turn
        // written flag to valid when writing.
        data2 -= len;
        memcpy(data2, data1, len);
        data1 += len;
        rest_size -= len;
    }
    memcpy((uint8_t *)dst_buffer, temp_buffer, size);

cleanup:
    eHNFFS_free(temp_buffer);
    return err;
}

/**
 * The comparation function with cache.
 *
 * We compare data in buffer with data in (sector, off, size) of nor flash.
 * And we should also checkout if we have already stored the in flash data
 * into pcache/rcache.
 *
 * cmp function doesn't care about forward or backward, so just read forward.
 */
int eHNFFS_cache_cmp(eHNFFS_t *eHNFFS, eHNFFS_size_t sector, eHNFFS_off_t off,
                     const void *buffer, eHNFFS_size_t size)
{
    int err = eHNFFS_ERR_OK;

    uint8_t *data1 = (uint8_t *)buffer;
    uint8_t *data2 = NULL;
    while (size > 0)
    {
        eHNFFS_size_t min = eHNFFS_min(size, eHNFFS->cfg->cache_size);
        err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, eHNFFS->rcache,
                                   sector, off, min);
        if (err)
        {
            goto cleanup;
        }

        off += min;
        size -= min;

        // The specital comparasion function.
        // Because we may set type of data in flash to delete directly(don't change cache message),
        // and at this time true data in cache hasn't flashing in flash, so normal compare function
        // isn't work because of delete type and normal type.
        data2 = eHNFFS->rcache->buffer;
        for (int i = 0; i < min; i++)
        {
            if (*data1 == *data2)
            {
                // Data is same.
                // data1 is what we write, data2 is what in nor flash.
                // If we write
                data1++;
                data2++;
                continue;
            }
            else if ((*data1 & ~1) == (*data2 & ~1))
            {
                // TODO, warning
                // Endian type may be different in stm32.

                // The max bit of type is different.
                // Because of little endian problem, decrease 2 is true data's head.
                data1 -= 2;
                data2 -= 2;
                i -= 2;

                // Checkout if it's because of delete type.
                eHNFFS_head_t head1 = *(eHNFFS_head_t *)data1;
                eHNFFS_head_t result = *(eHNFFS_head_t *)data1 ^ *(eHNFFS_head_t *)data2;
                if ((eHNFFS_dhead_type(head1) == eHNFFS_dhead_type(result)) &&
                    ((result & eHNFFS_DELETE_TYPE_SET) == 0))
                {
                    data1 += 4;
                    data2 += 4;
                    i += 4;
                    continue;
                }
                else
                {
                    err = (*(data2 + 1) - *(data1 + 1) < 0) ? eHNFFS_CMP_LT : eHNFFS_CMP_GT;
                    goto cleanup;
                }
            }
            else
            {
                // The max bit of type is different.
                // Because of little endian problem, decrease 1 is true data's head.
                data1 -= 1;
                data2 -= 1;
                i -= 1;

                // Checkout if it's because of delete type.
                eHNFFS_head_t head1 = *(eHNFFS_head_t *)data1;
                eHNFFS_head_t result = *(eHNFFS_head_t *)data1 ^ *(eHNFFS_head_t *)data2;
                if ((eHNFFS_dhead_type(head1) == eHNFFS_dhead_type(result)) &&
                    ((result & eHNFFS_DELETE_TYPE_SET) == 0))
                {
                    data1 += 4;
                    data2 += 4;
                    i += 4;
                    continue;
                }
                else
                {
                    err = (*(data2 + 2) - *(data1 + 2) < 0) ? eHNFFS_CMP_LT : eHNFFS_CMP_GT;
                    goto cleanup;
                }
            }
        }
    }

    err = eHNFFS_CMP_EQ;
cleanup:
    return err;
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * -------------------------------------------------------    Prog/Erase cache functions    ------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Set all writen flag of data head to 0 to confirm that it's writen without corrupt,
 * and the writen mode of data in buffer is forward.
 */
int eHNFFS_cache_writen_flag(eHNFFS_t *eHNFFS, eHNFFS_cache_ram_t *pcache)
{
    eHNFFS_ssize_t rest_size = pcache->size;
    uint8_t *data = pcache->buffer;
    eHNFFS_head_t head;
    eHNFFS_size_t len;
    while (rest_size > 0)
    {
        head = *(eHNFFS_head_t *)data;

        if (head == eHNFFS_NULL)
        {
            return eHNFFS_ERR_OK;
        }

        *(eHNFFS_head_t *)data &= eHNFFS_WRITTEN_FLAG_SET;

        len = eHNFFS_dhead_dsize(head);
        data += len;
        rest_size -= len;
    }

    if (rest_size != 0)
    {
        return eHNFFS_ERR_WRONGCAL;
    }

    int err = eHNFFS->cfg->prog(eHNFFS->cfg, pcache->sector, pcache->off,
                                pcache->buffer, pcache->size);
    eHNFFS_ASSERT(err <= 0);
    return err;
}

/**
 * Program(write) data in pcache to nor flash.
 *
 * If validata is valid, we should read what we have writen and compare to
 * data in pcache to make sure it's absolutely right.
 *
 * When flushing, we should turn writen bit of data head to 0, the flag tells us
 * the written mode in buffer, it's forward or backward.
 */
int eHNFFS_cache_flush(eHNFFS_t *eHNFFS, eHNFFS_cache_ram_t *pcache,
                       eHNFFS_cache_ram_t *rcache, bool validate)
{
    int err = eHNFFS_ERR_OK;
    if (pcache->sector == eHNFFS_NULL)
    {
        return eHNFFS_ERR_OK;
    }

    if (pcache->mode == eHNFFS_BACKWARD)
    {
        // If the mode of datas in pcache is backward,
        // we should transitate them to forward.
        err = eHNFFS_cache_transition(eHNFFS, pcache->buffer,
                                      pcache->buffer, pcache->size);
        if (err)
        {
            return err;
        }
    }

    // Program data into nor flash.
    eHNFFS_ASSERT(pcache->sector < eHNFFS->cfg->sector_count);
    eHNFFS_size_t diff = eHNFFS_alignup(pcache->size, eHNFFS->cfg->prog_size);
    err = eHNFFS->cfg->prog(eHNFFS->cfg, pcache->sector,
                            pcache->off, pcache->buffer, diff);
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    err = eHNFFS_cache_writen_flag(eHNFFS, pcache);
    if (err)
    {
        return err;
    }

    // Check the data we have just writen
    if (validate)
    {
        int res = eHNFFS_ERR_OK;
        res = eHNFFS_cache_cmp(eHNFFS, pcache->sector, pcache->off,
                               pcache->buffer, diff);
        if (res < 0)
        {
            return res;
        }

        if (res != eHNFFS_CMP_EQ)
        {
            return eHNFFS_ERR_WRONGPROG;
        }
    }

    // If we read data in pcache, we should flush it to flash first. and
    // read it in pcache, so we just drop data in pcache not turn to one.
    eHNFFS_cache_drop(eHNFFS, pcache);
    return err;
}

/**
 * The read function with cache, data we read are all in one sector..
 *
 * We only have a big read cache and a big program cache to do this to
 * decrease the cost of ram space.
 *
 * To read data of size sizes from (sector, off) to buffer, we find out
 * whether or not they are in pcache/rcache first, and then dicide to read
 * them from nor flash directly to rcache and buffer.
 *
 * Notice that for all files, there is a small cache. For small data, it
 * caches all data in it; For big data, it caches the index of true data.
 * So if we want to read big file with the cache, we should go through file
 * cache to read the index first.
 *
 * Now all data in rcache/pcache is in forward mode, after we flush pcache to
 * flash and turn it to backward, the message in pcache is droped.
 */
int eHNFFS_cache_read(eHNFFS_t *eHNFFS, eHNFFS_size_t mode,
                      const eHNFFS_cache_ram_t *pcache,
                      eHNFFS_cache_ram_t *rcache, eHNFFS_size_t sector,
                      eHNFFS_off_t off, void *buffer, eHNFFS_size_t size)
{
    int err = eHNFFS_ERR_OK;
    uint8_t *data = (uint8_t *)buffer;

    // Checkout whether or not (sector, off, size) is right.
    if (sector >= eHNFFS->cfg->sector_count ||
        off + size > eHNFFS->cfg->sector_size)
    {
        return eHNFFS_ERR_WRONGCAL;
    }

    eHNFFS_size_t rest_size = size;
    while (rest_size > 0)
    {
        // diff is the size of data memcpy function should copy.
        eHNFFS_size_t diff = rest_size;

        // Find data in pcache first.
        if (pcache && sector == pcache->sector &&
            off < pcache->off + pcache->size &&
            mode == pcache->mode)
        {
            // The start of the data we need is in pcache.
            if (off >= pcache->off)
            {
                eHNFFS_size_t temp_off = pcache->off;
                eHNFFS_size_t temp_size = pcache->size;

                // Now it seems no need
                /* if (mode == eHNFFS_BACKWARD)
                {
                    err = eHNFFS_cache_flush(eHNFFS, pcache, NULL, eHNFFS_VALIDATE);
                    if (err)
                    {
                        return err;
                    }
                } */

                diff = eHNFFS_min(diff, temp_size - (off - temp_off));
                memcpy(data, &pcache->buffer[off - temp_off], diff);

                data += diff;
                off += diff;
                rest_size -= diff;

                /* if (mode == eHNFFS_BACKWARD)
                {
                    eHNFFS_cache_one(eHNFFS, pcache);
                } */
                continue;
            }

            // If not, we should read the former part first(directly to buffer),
            // then read the rest part in pcache.
            diff = eHNFFS_min(diff, pcache->off - off);
        }

        // Find data in read cache second, similar to above.
        if (sector == rcache->sector &&
            off < rcache->off + rcache->size &&
            mode == rcache->mode)
        {
            if (off >= rcache->off)
            {
                diff = eHNFFS_min(diff, rcache->size - (off - rcache->off));
                memcpy(data, &rcache->buffer[off - rcache->off], diff);

                data += diff;
                off += diff;
                rest_size -= diff;
                continue;
            }

            diff = eHNFFS_min(diff, rcache->off - off);
        }

        // Read to buffer directly.
        // Because we read backward data from low address to high address,
        // what we read first is latest, we don't need to transit.
        int err = eHNFFS->cfg->read(eHNFFS->cfg, sector, off, data, diff);
        if (err)
        {
            return err;
        }

        data += diff;
        off += diff;
        rest_size -= diff;
        continue;
    }

    return err;
}

/**
 * The program function with cache.
 *
 * We program data from buffer to (sector, off, size) of nor flash, and they are
 * stored in pcache first. If pcache is full, we should flush data of it into
 * nor flash. pcache and validate are parameter of eHNFFS_cache_flush.
 *
 * When flushing, we should turn writen bit of data head to 0, the flag tells us
 * the written mode in buffer, it's forward or backward.
 */
int eHNFFS_cache_prog(eHNFFS_t *eHNFFS, eHNFFS_size_t mode, eHNFFS_cache_ram_t *pcache,
                      eHNFFS_cache_ram_t *rcache, bool validate, eHNFFS_size_t sector,
                      eHNFFS_off_t off, const void *buffer, eHNFFS_size_t size)
{
    int err = eHNFFS_ERR_OK;

    const uint8_t *data = (uint8_t *)buffer;
    eHNFFS_ASSERT(sector < eHNFFS->cfg->sector_count);
    eHNFFS_ASSERT(size <= eHNFFS->cfg->cache_size);
    if (mode == eHNFFS_FORWARD)
        eHNFFS_ASSERT(off + size <= eHNFFS->cfg->sector_size);

    eHNFFS_size_t rest_size = size;
    while (rest_size > 0)
    {
        // Checkout if pcache could store data we need to program directly.
        // The judging condition of forward and backward is different.
        // And we should prog the whole data in buffer to sector once to trun
        // writen flag to 0.
        // Notice that in BACKWARD condition, because our type is unsigned, we should
        // use eHNFFS_min function, or it may wrong.
        if ((mode == eHNFFS_FORWARD && sector == pcache->sector &&
             off >= pcache->off + pcache->size && mode == pcache->mode &&
             off + size < pcache->off + eHNFFS->cfg->cache_size) ||
            (mode == eHNFFS_BACKWARD && sector == pcache->sector &&
             off <= pcache->off && mode == pcache->mode &&
             off - size >= pcache->off - eHNFFS_min(eHNFFS->cfg->cache_size - pcache->size,
                                                    pcache->off)))
        {
            // We think it's append write, not random write.
            eHNFFS_size_t diff = eHNFFS_min(eHNFFS->cfg->cache_size - pcache->size,
                                            rest_size);
            memcpy(&pcache->buffer[pcache->size], data, diff);
            data += diff;
            if (mode == eHNFFS_FORWARD)
                off += diff;
            else
                off -= diff;
            rest_size -= diff;

            // we should frequently change the off in pcache if mode is backward.
            if (mode == eHNFFS_BACKWARD)
            {
                pcache->off -= diff;
            }

            // If pcache is full, then flush.
            pcache->size += size;
            if (pcache->size > eHNFFS->cfg->cache_size - sizeof(eHNFFS_head_t))
            {
                err = eHNFFS_cache_flush(eHNFFS, pcache, rcache, validate);
                if (err)
                {
                    return err;
                }
            }

            continue;
        }

        // Make sure pcache is not used by any other sectors when we use it,
        // i.e we have flushed all data in pcache.
        if (pcache->sector != eHNFFS_NULL)
        {
            err = eHNFFS_cache_flush(eHNFFS, pcache, rcache, validate);
            if (err)
            {
                return err;
            }
        }

        // prepare pcache for the next use.
        pcache->sector = sector;
        pcache->off = off;
        pcache->size = 0;
        pcache->mode = mode;
    }

    return err;
}

/**
 * Read data to cache.
 * Sometime eHNFFS_cache_read function doesn't work well, so we do this.
 */
int eHNFFS_read_to_cache(eHNFFS_t *eHNFFS, eHNFFS_size_t mode, eHNFFS_cache_ram_t *cache,
                         eHNFFS_size_t sector, eHNFFS_off_t off, eHNFFS_size_t size)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_ASSERT(off + size <= eHNFFS->cfg->sector_size);
    if (cache->sector == sector && cache->off == off &&
        cache->size == size && cache->mode == mode)
    {
        return err;
    }

    eHNFFS_cache_one(eHNFFS, cache);
    cache->sector = sector;
    cache->off = off;
    cache->size = size;
    cache->mode = mode;
    err = eHNFFS->cfg->read(eHNFFS->cfg, cache->sector, cache->off,
                            cache->buffer, cache->size);
    eHNFFS_ASSERT(err <= 0);

    return err;
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * --------------------------------------------------------    Prog/Erase without cache    -------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Directily prog shead.
 * For sectors belongs to dir, shead may have a name behind, so its length is larger than 4 Bytes.
 */
int eHNFFS_direct_shead_prog(eHNFFS_t *eHNFFS, eHNFFS_size_t sector, eHNFFS_off_t *off,
                             eHNFFS_size_t len, void *buffer)
{
    int err = eHNFFS_ERR_OK;

    err = eHNFFS->cfg->prog(eHNFFS->cfg, sector, *off, buffer, len);
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    err = eHNFFS_cache_cmp(eHNFFS, sector, *off, buffer, len);
    if (err < 0)
    {
        return err;
    }

    if (err != eHNFFS_CMP_EQ)
    {
        return eHNFFS_ERR_WRONGPROG;
    }

    *off += len;

    return err;
}

/**
 * Directily prog data with data head to flash.
 * Make sure data in buffer can be writen to sector.
 */
int eHNFFS_direct_data_prog(eHNFFS_t *eHNFFS, eHNFFS_size_t sector, eHNFFS_off_t *off,
                            eHNFFS_size_t len, void *buffer)
{
    int err = eHNFFS_ERR_OK;

    uint8_t *data = (uint8_t *)buffer;
    err = eHNFFS->cfg->prog(eHNFFS->cfg, sector, *off, data, len);
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    err = eHNFFS_cache_cmp(eHNFFS, sector, *off, buffer, len);
    if (err < 0)
    {
        return err;
    }

    if (err != eHNFFS_CMP_EQ)
    {
        return eHNFFS_ERR_WRONGPROG;
    }

    eHNFFS_head_t *head = (eHNFFS_head_t *)buffer;
    *head &= eHNFFS_WRITTEN_FLAG_SET;
    err = eHNFFS->cfg->prog(eHNFFS->cfg, sector, *off, data, len);
    eHNFFS_ASSERT(err <= 0);

    *off += len;

    return err;
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * ---------------------------------------------------------    Further prog operations    -------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Prog all metadata in a new superblock.
 */
int eHNFFS_superblock_prog(eHNFFS_t *eHNFFS, eHNFFS_superblock_ram_t *super,
                           eHNFFS_cache_ram_t *pcache)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_region_map_flash_t *region_map;
    eHNFFS_mapaddr_flash_t *id_map, *sector_map;
    eHNFFS_wladdr_flash_t wladdr;
    eHNFFS_size_t map_len;
    uint8_t *data;
    eHNFFS_size_t num;
    eHNFFS_treeaddr_flash_t taddr;
    eHNFFS_commit_flash_t commit;
    eHNFFS_magic_flash_t magic;

    // Super message.
    eHNFFS_size_t len = sizeof(eHNFFS_supermessage_flash_t);
    eHNFFS_supermessage_flash_t supermessage = {
        .head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_SUPER_MESSAGE, len),
        .fs_name = eHNFFS_FS_NAME,
        .version = eHNFFS_VERSION,
        .sector_size = eHNFFS->cfg->sector_size,
        .sector_count = eHNFFS->cfg->sector_count,
        .name_max = eHNFFS_min(eHNFFS->cfg->name_max, eHNFFS_NAME_MAX),
        .file_max = eHNFFS_min(eHNFFS->cfg->file_max, eHNFFS_FILE_MAX_SIZE),
        .region_cnt = eHNFFS->manager->region_num,
    };
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, &supermessage, len);
    if (err)
    {
        goto cleanup;
    }
    super->free_off += len;

    // Region map.
    len = sizeof(eHNFFS_region_map_flash_t);
    map_len = eHNFFS_alignup(eHNFFS->manager->region_num, 8);
    region_map = eHNFFS_malloc(sizeof(eHNFFS_region_map_flash_t) +
                               3 * map_len / 8);
    if (!region_map)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }
    region_map->head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_REGION_MAP,
                                      len + 3 * map_len);
    data = region_map->map;
    memcpy(data, eHNFFS->manager->region_map->free_region, map_len);
    data += map_len;
    memcpy(data, eHNFFS->manager->region_map->dir_region, map_len);
    data += map_len;
    memcpy(data, eHNFFS->manager->region_map->bfile_region, map_len);
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, region_map, len + 3 * map_len);
    if (err)
    {
        goto cleanup;
    }
    super->free_off += (len + 3 * map_len);

    // Address of id map.
    len = sizeof(eHNFFS_mapaddr_flash_t) + sizeof(eHNFFS_size_t);
    id_map = eHNFFS_malloc(len);
    if (!id_map)
    {
        goto cleanup;
    }
    id_map->head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_ID_MAP, len);
    id_map->begin = eHNFFS->id_map->free_map->begin;
    id_map->off = eHNFFS->id_map->free_map->off;
    id_map->erase_times[0] = eHNFFS->id_map->free_map->etimes[0];
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, id_map, len);
    if (err)
    {
        goto cleanup;
    }
    super->free_off += len;

    // Address of sector map.
    num = eHNFFS_alignup(2 * eHNFFS->cfg->sector_count / 8,
                         eHNFFS->cfg->sector_size);
    len = sizeof(eHNFFS_mapaddr_flash_t) + num * sizeof(eHNFFS_size_t);
    sector_map = eHNFFS_malloc(len);
    if (!sector_map)
    {
        goto cleanup;
    }
    sector_map->head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_SECTOR_MAP, len);
    sector_map->begin = eHNFFS->manager->dir_map->begin;
    sector_map->off = eHNFFS->manager->dir_map->off;
    // We store etimes of dir map and bfile map all in dir map.
    memcpy(sector_map->erase_times, eHNFFS->manager->dir_map->etimes, num * sizeof(eHNFFS_size_t));
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, sector_map, len);
    if (err)
    {
        goto cleanup;
    }
    super->free_off += len;

    // Address of wl message.
    if (eHNFFS->manager->scan_times != eHNFFS_WL_START)
    {
        goto treeaddr;
    }
    len = sizeof(eHNFFS_wladdr_flash_t);
    wladdr.head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_WL_ADDR, len);
    wladdr.begin = eHNFFS->manager->wl->begin;
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, &wladdr, len);
    if (err)
    {
        goto cleanup;
    }
    super->free_off += len;

treeaddr:
    // Address of hash tree.
    len = sizeof(eHNFFS_treeaddr_flash_t);
    taddr.head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_TREE_ADDR, len);
    taddr.begin = eHNFFS->hash_tree->begin;
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, &taddr, len);
    if (err)
    {
        goto cleanup;
    }
    super->free_off += len;

    // Commit message.
    len = sizeof(eHNFFS_commit_flash_t);
    commit.head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_COMMIT, len);
    commit.next_id = eHNFFS->id_map->free_map->index +
                     eHNFFS->id_map->free_map->region * eHNFFS->id_map->bits_in_buffer;
    commit.scan_times = eHNFFS->manager->scan_times;
    commit.next_dir_sector = eHNFFS->manager->dir_map->index +
                             eHNFFS->manager->dir_map->region * eHNFFS->manager->region_size;
    commit.next_bfile_sector = eHNFFS->manager->bfile_map->index +
                               eHNFFS->manager->bfile_map->region * eHNFFS->manager->region_size;
    commit.wl_index = (eHNFFS->manager->scan_times == eHNFFS_WL_START)
                          ? eHNFFS->manager->wl->index
                          : 0;
    commit.wl_free_off = (eHNFFS->manager->scan_times == eHNFFS_WL_START)
                             ? eHNFFS->manager->wl->free_off
                             : 0;
    commit.reserve = eHNFFS->manager->region_map->reserve;
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, &commit, len);
    if (err)
    {
        goto cleanup;
    }
    super->commit_off = super->free_off;
    super->free_off += len;

    // Magic number.
    len = sizeof(eHNFFS_magic_flash_t);
    magic.head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_MAGIC, len);
    magic.maigc = 0;
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, &magic, len);
    if (err)
    {
        goto cleanup;
    }
    super->free_off += len;

cleanup:
    if (region_map)
        eHNFFS_free(region_map);

    if (id_map)
        eHNFFS_free(id_map);

    if (sector_map)
        eHNFFS_free(sector_map);
    return err;
}

/**
 * Prog address of map/hash tree/wl to superblock directly.
 */
int eHNFFS_addr_prog(eHNFFS_t *eHNFFS, eHNFFS_superblock_ram_t *super,
                     void *buffer, eHNFFS_size_t len)
{
    int err = eHNFFS_ERR_OK;

    if (super->free_off + len > eHNFFS->cfg->sector_size)
    {
        // If current super block is full, choose the other one.
        err = eHNFFS_superblock_change(eHNFFS, super);
        if (err)
        {
            return err;
        }
    }

    // prog new wl message address to superblock directly.
    err = eHNFFS_direct_data_prog(eHNFFS, super->sector, &super->free_off, len, buffer);
    if (err)
    {
        return err;
    }

    return err;
}

/**
 * Prog data in superblock with prog cache.
 */
int eHNFFS_super_cache_prog(eHNFFS_t *eHNFFS, eHNFFS_superblock_ram_t *super,
                            eHNFFS_cache_ram_t *pcache, void *buffer, eHNFFS_size_t len)
{
    int err = eHNFFS_ERR_OK;

    if (super->free_off + len > eHNFFS->cfg->sector_size)
    {
        // If current super block is full, choose the other one.
        err = eHNFFS_superblock_change(eHNFFS, super);
        if (err)
        {
            return err;
        }
    }

    // prog new wl message address to superblock with cache.
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, buffer, len);
    if (err)
    {
        return err;
    }
    super->free_off += len;
    return err;
}

/**
 * Set state of sectors to old.
 * Notice that we should also set the erase map.
 */
int eHNFFS_sector_old(eHNFFS_t *eHNFFS, eHNFFS_size_t begin, eHNFFS_size_t num)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_head_t head = eHNFFS_OLD_SECTOR_SET;
    eHNFFS_size_t sector = begin;
    for (int i = 0; i < num; i++)
    {
        err = eHNFFS->cfg->prog(eHNFFS->cfg, sector, 0, &head, sizeof(eHNFFS_head_t));
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            return err;
        }
        sector++;
    }

    // Turn bits in erase map to 0, so it can reuse in the future.
    err = eHNFFS_emap_set(eHNFFS, eHNFFS->manager, begin, num);
    return err;
}

/**
 * Set state of sectors in index list to old.
 */
int eHNFFS_bfile_sector_old(eHNFFS_t *eHNFFS, eHNFFS_bfile_index_ram_t *index,
                            eHNFFS_size_t num)
{
    int err = eHNFFS_ERR_OK;

    for (int i = 0; i < num; i++)
    {
        if (index[i].sector == eHNFFS_NULL || index[i].size == 0)
        {
            continue;
        }

        // Loop for num of indexes.
        eHNFFS_size_t off = index[i].off;
        eHNFFS_size_t rest_size = index[i].size;

        eHNFFS_size_t cnt = 0;
        while (rest_size > 0)
        {
            // Calculate the number of sequential sectors the index has.
            eHNFFS_size_t size = eHNFFS_min(eHNFFS->cfg->sector_size - off,
                                            rest_size);
            cnt++;
            rest_size -= size;
            off += size;
            if (off == eHNFFS->cfg->sector_size)
            {
                off = sizeof(eHNFFS_bfile_sector_flash_t);
            }
        }

        // Set all these sequential sectors to old.
        err = eHNFFS_sector_old(eHNFFS, index[i].sector, cnt);
        if (err)
        {
            return err;
        }
    }

    return err;
}

/**
 * Set type of data head to delete data.
 */
int eHNFFS_data_delete(eHNFFS_t *eHNFFS, eHNFFS_size_t father_id, eHNFFS_size_t sector,
                       eHNFFS_off_t off, eHNFFS_size_t len)
{
    int err = eHNFFS_ERR_OK;

    // If sector is eHNFFS_NULL, it means that there is no data in flash.
    if (sector == eHNFFS_NULL)
        return err;

    // Turn to old.
    eHNFFS_head_t head = eHNFFS_DELETE_TYPE_SET;
    if (sector == eHNFFS->pcache->sector && off >= eHNFFS->pcache->off &&
        off + len <= eHNFFS->pcache->off + eHNFFS->pcache->size)
    {
        // If it's in pcache, we should find it.
        uint8_t *data;
        if (eHNFFS->pcache->mode == eHNFFS_FORWARD)
            data = eHNFFS->pcache->buffer + (off - eHNFFS->pcache->off);
        else
            data = eHNFFS->pcache->buffer + eHNFFS->pcache->size -
                   (off - eHNFFS->pcache->off) - len;
        *(eHNFFS_head_t *)data &= head;
    }
    else
    {
        err = eHNFFS->cfg->prog(eHNFFS->cfg, sector, off, &head, sizeof(eHNFFS_head_t));
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            return err;
        }
    }
    // We don't need to calculate old space of super message.
    if (father_id == eHNFFS_ID_SUPER)
    {
        return err;
    }

    // Find file's father dir.
    eHNFFS_dir_ram_t *father_dir = eHNFFS->dir_list->dir;
    while (father_dir != NULL && father_dir->id != father_id)
    {
        father_dir = father_dir->next_dir;
    }

    // Not found.
    if (father_dir == NULL)
    {
        return eHNFFS_ERR_NOFATHER;
    }

    father_dir->old_space += len;
    return err;
}

/**
 * Reprog sector head.
 * In some cases like reserve region, the sector head is free for use because we
 * don't use sector allocate function, so we should use this function first.
 */
int eHNFFS_shead_reprog(eHNFFS_t *eHNFFS, eHNFFS_size_t begin, eHNFFS_size_t num,
                        void *buffer, eHNFFS_size_t len)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t sector = begin;
    for (int i = 0; i < num; i++)
    {
        err = eHNFFS->cfg->prog(eHNFFS->cfg, sector, 0, buffer, len);
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            return err;
        }

        err = eHNFFS_cache_cmp(eHNFFS, sector, 0, buffer, len);
        if (err < 0)
        {
            return err;
        }

        if (err != eHNFFS_CMP_EQ)
        {
            return eHNFFS_ERR_WRONGPROG;
        }
    }

    return err;
}

/**
 * Read bit map of region to buffer, both read cache and prog cache should be read.
 */
int eHNFFS_region_read(eHNFFS_t *eHNFFS, eHNFFS_size_t region, uint32_t *buffer)
{
    int err = eHNFFS_ERR_OK;

    // Read dir map.
    eHNFFS_size_t size = eHNFFS->manager->region_size / 8;
    eHNFFS_map_ram_t *map = eHNFFS->manager->dir_map;
    eHNFFS_size_t sector = map->begin;
    eHNFFS_size_t off = map->off + region * size;
    while (off >= eHNFFS->cfg->sector_size)
    {
        sector++;
        off -= eHNFFS->cfg->sector_size;
    }

    err = eHNFFS->cfg->read(eHNFFS->cfg, sector, off, buffer, size);
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    // Read erase map.
    uint32_t temp_buffer[size / 4];
    map = eHNFFS->manager->erase_map;
    sector = map->begin;
    off = map->off + region * size;
    while (off >= eHNFFS->cfg->sector_size)
    {
        sector++;
        off -= eHNFFS->cfg->sector_size;
    }
    err = eHNFFS->cfg->read(eHNFFS->cfg, sector, off, temp_buffer, size);
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    // Merge data in two maps.
    size = size / 4;
    for (int i = 0; i < size; i++)
    {
        buffer[i] |= ~temp_buffer[i];
    }
    return err;
}

/**
 * Calculate the number of valid bits(0) in cache buffer.
 */
eHNFFS_size_t eHNFFS_cache_valid_num(eHNFFS_cache_ram_t *cache)
{
    eHNFFS_ASSERT(cache->size % sizeof(uint32_t) == 0);
    eHNFFS_size_t size = cache->size / sizeof(uint32_t);
    uint32_t *data = (uint32_t *)cache->buffer;

    eHNFFS_size_t num = 0;
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < eHNFFS_SIZE_T_NUM; j++)
        {
            // ((data[i] >> j) & 1U) ^ 1U
            if (((data[i] >> j) & 1U) == 0)
            {
                num++;
            }
        }
    }

    return num;
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * ---------------------------------------------------------    Further erase operations    ------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * The basic erase function in eHNFFS.
 */
int eHNFFS_sector_erase(eHNFFS_t *eHNFFS, eHNFFS_size_t sector)
{
    // Erase it first.
    int err = eHNFFS->cfg->erase(eHNFFS->cfg, sector);
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    // If it's not time to do wl, return.
    if (eHNFFS->manager->scan_times != eHNFFS_WL_START)
    {
        return err;
    }

    // Record the erase message in wl message.
    eHNFFS_size_t region = eHNFFS_aligndown(sector, eHNFFS->manager->region_size);
    eHNFFS_wl_message_t *regions = eHNFFS->manager->wl->regions;
    for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
    {
        if (regions[i].region == region)
        {
            regions[i].erase_times++;
        }
    }

    regions = eHNFFS->manager->wl->bfile_regions;
    for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
    {
        if (regions[i].region == region)
        {
            regions[i].erase_times++;
        }
    }

    regions = eHNFFS->manager->wl->exten_regions;
    for (int i = 0; i < eHNFFS_RAM_REGION_NUM; i++)
    {
        if (regions[i].region == region)
        {
            regions[i].erase_times++;
        }
        else if (regions[i].region == eHNFFS_NULL)
        {
            regions[i].region = region;
            regions[i].erase_times++;
        }
        else if (i == eHNFFS_RAM_REGION_NUM - 1)
        {
            err = eHNFFS_wladd_flush(eHNFFS, eHNFFS->manager, regions);
            if (err)
            {
                return err;
            }

            eHNFFS_wlregion_reset(regions);
            regions[i].region = region;
            regions[i].erase_times++;
        }
    }

    return err;
}

/**
 * We should erase old sectors belong to map before initializing
 * the new one.
 */
int eHNFFS_old_msector_erase(eHNFFS_t *eHNFFS, eHNFFS_size_t num,
                             eHNFFS_size_t begin, eHNFFS_size_t *etimes)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_head_t head;
    eHNFFS_size_t sector = begin;
    for (int i = 0; i < num; i++)
    {
        // Erase old one.
        err = eHNFFS_sector_erase(eHNFFS, sector);
        if (err)
        {
            return err;
        }

        // Prog new sector head, the main reason is to find a
        eHNFFS_off_t off = 0;
        head = eHNFFS_MKSHEAD(0, eHNFFS_SECTOR_FREE, eHNFFS_SECTOR_NOTSURE,
                              0xff, etimes[i] + 1);
        err = eHNFFS_direct_shead_prog(eHNFFS, sector, &off, sizeof(eHNFFS_size_t), &head);
        if (err)
        {
            return err;
        }
    }

    return err;
}
