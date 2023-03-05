/**
 * Big related related operations.
 */

#include "eHNFFS_file.h"
#include "eHNFFS_head.h"
#include "eHNFFS_cache.h"
#include "eHNFFS_tree.h"
#include "eHNFFS_gc.h"
#include "eHNFFS_manage.h"
#include "eHNFFS_dir.h"

/**
 * Initialize file list structure.
 */
int eHNFFS_file_list_initialization(eHNFFS_t *eHNFFS, eHNFFS_file_list_ram_t **list_addr)
{
    int err = eHNFFS_ERR_OK;

    // Allocate memory for file list.
    eHNFFS_file_list_ram_t *list = eHNFFS_malloc(sizeof(eHNFFS_file_list_ram_t));
    if (!list)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    // Initialize basic message.
    list->count = 0;
    list->used_space = 0;
    list->file = NULL;
    *list_addr = list;
    return err;

cleanup:
    if (list)
        eHNFFS_free(list);

    return err;
}

/**
 * Free file list structure.
 */
void eHNFFS_file_list_free(eHNFFS_t *eHNFFS, eHNFFS_file_list_ram_t *list)
{
    eHNFFS_file_ram_t *file = list->file;
    while (file != NULL)
    {
        eHNFFS_free(file);
        file = file->next_file;
    }
    eHNFFS_free(list);
}

/**
 * Free specific file in file list.
 */
int eHNFFS_file_free(eHNFFS_file_list_ram_t *list, eHNFFS_file_ram_t *file)
{
    eHNFFS_file_ram_t *list_file = list->file;

    // If file is at the begin of list.
    if (list_file->id == file->id)
    {
        list->file = file->next_file;
        eHNFFS_free(file->file_cache.buffer);
        eHNFFS_free(file);
        list->count--;
        return eHNFFS_ERR_OK;
    }

    // Find file's previous file in file list.
    while (list_file->next_file != NULL)
    {
        if (list_file->next_file->id == file->id)
            break;
        list_file = list_file->next_file;
    }

    if (list_file->next_file == NULL)
        return eHNFFS_ERR_NOFILEOPEN;
    else
    {
        list_file->next_file = file->next_file;
        eHNFFS_free(file->file_cache.buffer);
        eHNFFS_free(file);
        list->count--;
        return eHNFFS_ERR_OK;
    }
}

/**
 * The basic prog function of big file during gc.
 */
int eHNFFS_bfile_prog(eHNFFS_t *eHNFFS, eHNFFS_size_t *sector, eHNFFS_off_t *off,
                      const void *buffer, eHNFFS_size_t len)
{
    int err = eHNFFS_ERR_OK;
    uint8_t *data = (uint8_t *)buffer;
    while (len > 0)
    {
        // Prog data to flash.
        eHNFFS_size_t size = eHNFFS_min(eHNFFS->cfg->sector_size - *off,
                                        eHNFFS_min(eHNFFS->cfg->cache_size, len));
        err = eHNFFS->cfg->prog(eHNFFS->cfg, *sector, *off, data, size);
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            return err;
        }

        err = eHNFFS_cache_cmp(eHNFFS, *sector, *off, data, size);
        if (err < 0)
        {
            return err;
        }

        if (err != eHNFFS_CMP_EQ)
        {
            return eHNFFS_ERR_WRONGPROG;
        }

        // Change basic information.
        *off += size;
        len -= size;
        data += size;
        if (*off == eHNFFS->cfg->sector_size)
        {
            *sector += 1;
            *off = sizeof(eHNFFS_bfile_sector_flash_t);
        }
    }

    return err;
}

/**
 * The traverse function of big file during gc.
 * When traversing, we should prog valid data to new sector.
 */
int eHNFFS_ftraverse_gc(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file, eHNFFS_size_t num)
{
    int err = eHNFFS_ERR_OK;

		eHNFFS_size_t len;
	
    // Find new sequential space to do gc.
    eHNFFS_size_t new_begin, new_sector;
    eHNFFS_off_t new_off = sizeof(eHNFFS_bfile_sector_flash_t);
    err = eHNFFS_sector_alloc(eHNFFS, eHNFFS->manager, eHNFFS_SECTOR_BFILE, num,
                              eHNFFS_NULL, file->id, file->father_id, &new_sector, NULL);
    if (err)
    {
        return err;
    }
    new_begin = new_sector;

    eHNFFS_cache_ram_t my_cache;
    my_cache.buffer = eHNFFS_malloc(eHNFFS->cfg->cache_size);
    if (my_cache.buffer == NULL)
        return eHNFFS_ERR_NOMEM;

    // The index number in file cache.
    eHNFFS_size_t index_num = (file->file_cache.size - sizeof(eHNFFS_head_t)) /
                              sizeof(eHNFFS_bfile_index_ram_t);
    eHNFFS_bfile_index_flash_t *bfile_index = (eHNFFS_bfile_index_flash_t *)file->file_cache.buffer;
    for (int i = 0; i < index_num; i++)
    {
        eHNFFS_size_t sector = bfile_index->index[i].sector;
        eHNFFS_size_t off = bfile_index->index[i].off;
        eHNFFS_size_t rest_size = bfile_index->index[i].size;

        while (rest_size > 0)
        {
            // Read data of index sector.
            eHNFFS_size_t size = eHNFFS_min(eHNFFS->cfg->sector_size - off,
                                            eHNFFS_min(eHNFFS->cfg->cache_size,
                                                       rest_size));
            // err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, eHNFFS->rcache,
            //                            sector, off, size);
            err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, &my_cache,
                                       sector, off, size);
            if (err)
            {
                goto cleanup;
            }

            // Prog readed data.
            // err = eHNFFS_bfile_prog(eHNFFS, &new_sector, &new_off,
            //                         eHNFFS->rcache->buffer, size);
            err = eHNFFS_bfile_prog(eHNFFS, &new_sector, &new_off,
                                    my_cache.buffer, size);
            if (err)
            {
                goto cleanup;
            }

            // Update basic message.
            rest_size -= size;
            off += size;
            if (off == eHNFFS->cfg->sector_size)
            {
                sector++;
                off = sizeof(eHNFFS_bfile_sector_flash_t);
            }
        }
    }

    // Turn sectors belongs to old index to old, so we can reuse them.
    err = eHNFFS_bfile_sector_old(eHNFFS, bfile_index->index, index_num);
    if (err)
    {
        goto cleanup;
    }

    // Turn old in-flash index to deleted.
    err = eHNFFS_data_delete(eHNFFS, file->father_id, file->file_cache.sector,
                             file->file_cache.off, file->file_cache.size);
    if (err)
    {
        goto cleanup;
    }

    // Prog new big file index.
    eHNFFS_cache_one(eHNFFS, &file->file_cache);
    len = sizeof(eHNFFS_head_t) + sizeof(eHNFFS_bfile_index_ram_t);
    bfile_index->head = eHNFFS_MKDHEAD(0, 1, file->id, eHNFFS_DATA_BFILE_INDEX, len);
    bfile_index->index->sector = new_begin;
    bfile_index->index->off = sizeof(eHNFFS_bfile_sector_flash_t);
    bfile_index->index->size = file->file_size;

    // Turn mode(changing flag) to 1.
    file->file_cache.mode = 1;
    file->file_cache.size = len;

cleanup:
    if (my_cache.buffer != NULL)
        eHNFFS_free(my_cache.buffer);
    return err;
}

/**
 * For each index entry, find its end sector.
 * In some cases, many index entrys may use one sector at the same time.
 * So we should do this to know when to delete this sector.
 */
void eHNFFS_esector_calcullate(eHNFFS_t *eHNFFS, eHNFFS_bfile_index_ram_t *index,
                               eHNFFS_size_t num, eHNFFS_size_t *end_sector)
{
    for (int i = 0; i < num; i++)
    {
        end_sector[i] = index[i].sector;
        eHNFFS_off_t off = index[i].off;
        eHNFFS_size_t rest_size = index[i].size;

        while (rest_size > 0)
        {
            eHNFFS_size_t size = eHNFFS_min(eHNFFS->cfg->sector_size - off,
                                            rest_size);
            rest_size -= size;
            off += size;
            if (off == eHNFFS->cfg->sector_size)
            {
                end_sector[i]++;
                off = sizeof(eHNFFS_bfile_sector_flash_t);
            }
        }

        if (off == sizeof(eHNFFS_bfile_sector_flash_t))
        {
            end_sector[i]--;
        }
    }
}

/**
 * GC function for very big file.
 * The size of big file is large than region size, but gc size is smaller than region size.
 */
int eHNFFS_bfile_part_gc(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file, eHNFFS_size_t start,
                         eHNFFS_size_t end, eHNFFS_size_t len, eHNFFS_size_t index_num,
                         eHNFFS_size_t sector_num)
{
    int err = eHNFFS_ERR_OK;

    // Find new sequential space to do gc.
    eHNFFS_size_t new_begin,
        new_sector;
    err = eHNFFS_sector_alloc(eHNFFS, eHNFFS->manager, eHNFFS_SECTOR_BFILE, sector_num,
                              eHNFFS_NULL, file->id, file->father_id, &new_sector, NULL);
    if (err)
    {
        return err;
    }
    new_begin = new_sector;

    // GC what need to gc.
    eHNFFS_bfile_index_flash_t *bfile_index = (eHNFFS_bfile_index_flash_t *)file->file_cache.buffer;
    for (int i = start; i <= end; i++)
    {
        eHNFFS_size_t sector = bfile_index->index[i].sector;
        eHNFFS_size_t off = bfile_index->index[i].off;
        eHNFFS_size_t rest_size = bfile_index->index[i].size;

        while (rest_size > 0)
        {
            // Read data of index sector.
            eHNFFS_size_t size = eHNFFS_min(eHNFFS->cfg->sector_size - off,
                                            eHNFFS_min(eHNFFS->cfg->cache_size,
                                                       rest_size));
            err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, eHNFFS->rcache,
                                       sector, off, size);
            if (err)
            {
                return err;
            }

            // Prog readed data.
            err = eHNFFS_bfile_prog(eHNFFS, &new_sector, &off,
                                    eHNFFS->rcache->buffer, size);
            if (err)
            {
                return err;
            }

            // Update basic message.
            rest_size -= size;
            // off += size;

            if (off == eHNFFS->cfg->sector_size)
            {
                sector++;
                off = sizeof(eHNFFS_bfile_sector_flash_t);
            }
        }
    }

    // Turn sectors belongs to old index to old, so we can reuse them.
    err = eHNFFS_bfile_sector_old(eHNFFS, &bfile_index->index[start], end - start + 1);
    if (err)
    {
        return err;
    }

    // Turn old in-flash index to deleted.
    err = eHNFFS_data_delete(eHNFFS, file->father_id, file->file_cache.sector,
                             file->file_cache.off, file->file_cache.size);
    if (err)
    {
        return err;
    }

    // Prog new big file index.
    bfile_index->index[start].sector = new_begin;
    bfile_index->index[start].off = sizeof(eHNFFS_bfile_sector_flash_t);
    bfile_index->index[start].size = len;
    eHNFFS_size_t rest = index_num - end;
    memcpy(&bfile_index->index[start + 1], &bfile_index->index[end + 1],
           rest * sizeof(eHNFFS_bfile_index_ram_t));
    file->file_cache.mode = 1;
    file->file_cache.size -= (end - start) * sizeof(eHNFFS_bfile_index_ram_t);

    return err;
}

/**
 * Initialize garbage collection map.
 */
int eHNFFS_bfile_gcmap_initialize(eHNFFS_t *eHNFFS, eHNFFS_bfile_index_ram_t *index,
                                  eHNFFS_size_t num, eHNFFS_size_t region, uint32_t *gc_map)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t begin = region * eHNFFS->manager->region_size;
    eHNFFS_size_t end = (region + 1) * eHNFFS->manager->region_size - 1;

    // Calculate end sector.
    eHNFFS_size_t end_sector[num];
    eHNFFS_esector_calcullate(eHNFFS, index, num, end_sector);

    for (int i = 0; i < num; i++)
    {
        eHNFFS_size_t min = eHNFFS_max(begin, index[i].sector);
        eHNFFS_size_t max = eHNFFS_min(end, end_sector[i]);

        eHNFFS_size_t a = (min % eHNFFS->manager->region_size) /
                          eHNFFS_SIZE_T_NUM;
        eHNFFS_size_t b = (min % eHNFFS->manager->region_size) %
                          eHNFFS_SIZE_T_NUM;
        for (int j = min; j <= max; j++)
        {
            // Bit 0 indicates that there still has something in the sector,
            // need to migrate.
            gc_map[a] |= (1U << b);
            b++;
            if (b == eHNFFS_SIZE_T_NUM)
            {
                a++;
                b = 0;
            }
        }
    }
    return err;
}

int eHNFFS_bfile_region_gc(eHNFFS_t *eHNFFS, eHNFFS_flash_manage_ram_t *manager,
                           eHNFFS_file_ram_t *file, eHNFFS_size_t start, eHNFFS_size_t end,
                           eHNFFS_size_t len, eHNFFS_size_t index_num, eHNFFS_size_t sector_num)
{
    int err = eHNFFS_ERR_OK;

    // Choose candidate to be the next reserve region.
    // Current approach is not perfect and may wrong.
    eHNFFS_bfile_index_flash_t *bfile_index = (eHNFFS_bfile_index_flash_t *)file->file_cache.buffer;
    // eHNFFS_bfile_index_ram_t index[end - start + 1];
    // memcpy(index, &bfile_index->index[start], (end - start + 1) * sizeof(eHNFFS_bfile_index_ram_t));
    eHNFFS_size_t candidate = start;
    for (int i = start; i <= end; i++)
    {
        if (bfile_index->index[candidate].size < bfile_index->index[i].size)
        {
            candidate = i;
        }
    }
    candidate = bfile_index->index[candidate].sector / manager->region_size;

    // Add reserve to bfile map.
    err = eHNFFS_reserve_region_use(manager->region_map, eHNFFS_SECTOR_BFILE);
    if (err)
    {
        return err;
    }

    // Update sector head belongs to sectors that we need to use in the future.
    eHNFFS_size_t sector = manager->region_map->reserve * manager->region_size;
    eHNFFS_bfile_sector_flash_t bfile_head = {
        .head = eHNFFS_MKSHEAD(0, eHNFFS_SECTOR_USING, eHNFFS_SECTOR_BFILE,
                               0, eHNFFS_NULL),
        .id = file->id,
        .father_id = file->father_id,
    };

    // Reprog all these heads.
    err = eHNFFS_shead_reprog(eHNFFS, sector, sector_num, &bfile_head,
                              sizeof(eHNFFS_bfile_sector_flash_t));
    if (err)
    {
        return err;
    }

    // Set all these in map to use
    err = eHNFFS_directly_map_using(eHNFFS, manager->dir_map->begin, manager->dir_map->off,
                                    sector, sector_num);
    if (err)
    {
        return err;
    }

    // Remove candidate region from corresponding region map.
    err = eHNFFS_remove_region(eHNFFS, eHNFFS->manager->region_map,
                               eHNFFS_SECTOR_BFILE, candidate);
    if (err)
    {
        return err;
    }

    // Do gc and data migrations.
    err = eHNFFS_bfile_part_gc(eHNFFS, file, start, end, len, index_num, sector_num);
    if (err)
    {
        return err;
    }

    // If region of eraese map is just the candidate region, then flush it to flash first.
    if (manager->erase_map->region == candidate)
    {
        err = eHNFFS_map_flush(eHNFFS, manager->erase_map, manager->region_size / 8);
        if (err)
        {
            return err;
        }
    }

    // Get used sectors message of candidate region.
    uint32_t map_buffer[manager->region_size / eHNFFS_SIZE_T_NUM];
    err = eHNFFS_region_read(eHNFFS, candidate, map_buffer);
    if (err)
    {
        return err;
    }

    // Migrate rest valid data in candidate region to other place.
    err = eHNFFS_bfile_region_migration(eHNFFS, candidate, map_buffer);
    if (err)
    {
        return err;
    }

    manager->region_map->reserve = candidate;
    return err;
}

/**
 * GC function for very big file.
 */
int eHNFFS_part_ftraverse_gc(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_bfile_index_flash_t *bfile_index = (eHNFFS_bfile_index_flash_t *)file->file_cache.buffer;
    eHNFFS_size_t num = (eHNFFS_dhead_dsize(bfile_index->head) - sizeof(eHNFFS_size_t)) /
                        sizeof(eHNFFS_bfile_index_ram_t);

    if (num < eHNFFS_FILE_INDEX_LEN)
    {
        // No need to do gc
        return err;
    }

    eHNFFS_size_t threshold = eHNFFS->cfg->sector_size;
    eHNFFS_size_t candidate_arr[22] = {0};
    eHNFFS_size_t arr_len = 0;
    for (int i = 0; i < num; i++)
    {
        if (bfile_index->index[i].size <= threshold)
        {
            candidate_arr[arr_len] = i;
            arr_len++;
        }
    }

    eHNFFS_size_t gc_size = 0;
    eHNFFS_size_t min, max;
    eHNFFS_size_t distance = 0;
    for (int i = 0; i < arr_len; i++)
    {
        for (int j = i + 1; j < arr_len; j++)
        {
            gc_size = 0;
            for (int k = candidate_arr[i]; k <= candidate_arr[j]; k++)
            {
                gc_size += bfile_index->index[k].size;
            }

            if (gc_size < eHNFFS->manager->region_size * eHNFFS->cfg->sector_size)
            {
                if (candidate_arr[j] - candidate_arr[i] > distance)
                {
                    min = candidate_arr[i];
                    max = candidate_arr[j];
                    distance = max - min;
                }
            }
        }
    }

    if (distance == 0)
        // Now we ignore it, but in the future we should do something else.
        return err;

    // Version 3
    eHNFFS_size_t len = eHNFFS->cfg->sector_size - sizeof(eHNFFS_bfile_sector_flash_t);
    gc_size = 0;
    for (int i = min; i <= max; i++)
    {
        gc_size += bfile_index->index[i].size;
    }
    eHNFFS_size_t gc_num = eHNFFS_alignup(gc_size, len) / len;

    err = eHNFFS_bfile_part_gc(eHNFFS, file, min, max, gc_size, num, gc_num);
    if (err)
    {
        printf("...\n");
        return err;
    }

    // Version 2
    // eHNFFS_size_t gc_size = -1;
    // eHNFFS_size_t min = 0;
    // eHNFFS_size_t max = arr_len - 1;
    // while (true)
    // {
    //     if (min == max)
    //         // Now we don't have ability to do gc.
    //         return err;

    //     gc_size = 0;
    //     for (int i = candidate_arr[min]; i <= candidate_arr[max]; i++)
    //         gc_size += bfile_index->index[i].size;

    //     if (gc_size >= eHNFFS->cfg->sector_size * eHNFFS->manager->region_size)
    //     {
    //         if ((candidate_arr[min + 1] - candidate_arr[min]) >=
    //             (candidate_arr[max] - candidate_arr[max - 1]))
    //             max--;
    //         else
    //             min++;
    //     }
    //     else
    //         break;
    // }

    // eHNFFS_size_t len = eHNFFS->cfg->sector_size - sizeof(eHNFFS_bfile_sector_flash_t);
    // eHNFFS_size_t gc_num = eHNFFS_alignup(gc_size, len) / len;

    // printf("min and max, %d, %d, %d, %d, %d\n", candidate_arr[min], candidate_arr[max], gc_size, num, gc_num);

    // err = eHNFFS_bfile_part_gc(eHNFFS, file, candidate_arr[min], candidate_arr[max], gc_size, num, gc_num);
    // if (err)
    // {
    //     return err;
    // }

    // Version 1
    // eHNFFS_size_t min = eHNFFS_NULL;
    // eHNFFS_size_t max = 0;
    // eHNFFS_size_t threshold = eHNFFS->cfg->sector_size;

    // for (int i = candidate_arr[0]; i <= candidate_arr[arr_len];)

    // // Find a set of index to merge and gc.
    // while (max - min < 5)
    // {
    //     for (int i = 0; i < num; i++)
    //     {
    //         if (bfile_index->index[i].size <= threshold)
    //         {
    //             min = eHNFFS_min(min, i);
    //             max = eHNFFS_max(max, i);
    //         }
    //     }

    //     threshold += 2 * eHNFFS->cfg->sector_size;
    // }

    // eHNFFS_size_t gc_size = 0;
    // for (int i = min; i <= max; i++)
    // {
    //     gc_size += bfile_index->index[i].size;
    // }
    // eHNFFS_size_t len = eHNFFS->cfg->sector_size - sizeof(eHNFFS_bfile_sector_flash_t);
    // eHNFFS_size_t gc_num = eHNFFS_alignup(gc_size, len) / len;

    // if (gc_num < eHNFFS->manager->region_size / 2)
    // {

    //     // Debug
    //     printf("min and max, %d, %d, %d, %d, %d\n", min, max, gc_size, num, gc_num);

    //     err = eHNFFS_bfile_part_gc(eHNFFS, file, min, max, gc_size, num, gc_num);
    //     if (err)
    //     {
    //         return err;
    //     }
    // }
    // else if (gc_num <= eHNFFS->manager->region_size)
    // {

    //     // Debug
    //     printf("。。。min and max, %d, %d, %d, %d, %d\n", min, max, gc_size, num, gc_num);

    //     err = eHNFFS_bfile_region_gc(eHNFFS, eHNFFS->manager, file, min, max, gc_size, num, gc_num);
    //     if (err)
    //     {
    //         return err;
    //     }
    // }

    // If we can't merge index into one region, then not do gc now.
    return err;
}

/**
 * File open function with id.
 */
int eHNFFS_file_lowopen(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_size_t id,
                        eHNFFS_size_t sector, eHNFFS_off_t off, eHNFFS_size_t namelen,
                        eHNFFS_file_ram_t **file_addr, int type)
{
    int err = eHNFFS_ERR_OK;

    // Find file in file list first.
    eHNFFS_file_list_ram_t *list = eHNFFS->file_list;
    eHNFFS_file_ram_t *file = list->file;

    while (file != NULL)
    {
        if (file->id == id)
        {
            *file_addr = file;
            return err;
        }
        file = file->next_file;
    }

    // Allocate memory for file.
    file = eHNFFS_malloc(sizeof(eHNFFS_file_ram_t));
    if (!file)
    {
        printf("NO MEMORY\r\n");
        return eHNFFS_ERR_NOMEM;
    }

    // Basic id, type message.
    file->id = id;
    file->father_id = dir->id;
    file->type = type;
    file->sector = sector;
    file->off = off;
    file->namelen = namelen;

    // Allocate memory for buffer, The size is the max size of data in dir.
    file->file_cache.buffer = eHNFFS_malloc(eHNFFS_FILE_CACHE_SIZE);
    if (!file->file_cache.buffer)
    {
        printf("NO MEMORY\r\n");
        return eHNFFS_ERR_NOMEM;
    }

    // Traverse dir to find data with id.
    err = eHNFFS_dtraverse_data(eHNFFS, dir, id, file->file_cache.buffer,
                                &file->file_cache.sector, &file->file_cache.off);
    if (err)
    {
        return err;
    }

    // Updata basic message.
    eHNFFS_head_t head = *(eHNFFS_head_t *)file->file_cache.buffer;
    file->file_cache.size = eHNFFS_dhead_dsize(head);
    file->file_cache.mode = 0;
    file->pos = 0;

    if (eHNFFS_dhead_type(head) == eHNFFS_DATA_SFILE_DATA)
    {
        // Calculate small file size.
        file->file_size = file->file_cache.size - sizeof(eHNFFS_head_t);
    }
    else
    {
        // Calculate big file size.
        eHNFFS_bfile_index_flash_t *bfile_index = (eHNFFS_bfile_index_flash_t *)file->file_cache.buffer;
        eHNFFS_size_t num = (file->file_cache.size - sizeof(eHNFFS_head_t)) /
                            sizeof(eHNFFS_bfile_index_ram_t);
        file->file_size = 0;
        for (int i = 0; i < num; i++)
        {
            file->file_size += bfile_index->index[i].size;
        }
    }

    // Add file to list.
    if (list->file != NULL)
        file->next_file = list->file->next_file;
    else
        file->next_file = NULL;
    list->file = file;
    list->count++;

    *file_addr = file;
    return err;
}

/**
 * Flush data in file cache to corresponding dir.
 */
int eHNFFS_file_flush(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file)
{
    int err = eHNFFS_ERR_OK;

    if (file->file_cache.mode == 0)
        return err;

    // Find file's father dir.
    eHNFFS_dir_ram_t *dir = eHNFFS->dir_list->dir;
    while (dir->id != file->father_id)
    {
        dir = dir->next_dir;
        if (dir == NULL)
        {
            return eHNFFS_ERR_NODIROPEN;
        }
    }

    // Set type of old file index to delete.
    eHNFFS_head_t old_head = *(eHNFFS_head_t *)file->file_cache.buffer;
    err = eHNFFS_data_delete(eHNFFS, file->father_id, file->file_cache.sector,
                             file->file_cache.off, eHNFFS_dhead_dsize(old_head));
    if (err)
    {
        return err;
    }

    // Prog new file index to dir.
    eHNFFS_head_t *head = (eHNFFS_head_t *)file->file_cache.buffer;

    *head = eHNFFS_MKDHEAD(0, 1, file->id,
                           (file->file_size <= eHNFFS_FILE_CACHE_SIZE) ? eHNFFS_DATA_SFILE_DATA
                                                                       : eHNFFS_DATA_BFILE_INDEX,
                           file->file_cache.size);

    err = eHNFFS_dir_prog(eHNFFS, dir, file->file_cache.buffer, file->file_cache.size,
                          eHNFFS_BACKWARD);
    if (err)
    {
        printf("...\n");
        return err;
    }

    file->file_cache.sector = dir->tail;
    file->file_cache.off = dir->backward;
    file->file_cache.mode = 0;
    return err;
}

/**
 * Close file function.
 */
int eHNFFS_file_lowclose(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_file_ram_t *pre_file = NULL;
    // If mode(i.e change flag) is true, then we should flush data to flash.
    if (file->file_cache.mode)
    {
        err = eHNFFS_file_flush(eHNFFS, file);
        if (err)
        {
            return err;
        }
    }

    // Delete file in in-ram file list.
    eHNFFS_file_ram_t *list_file = eHNFFS->file_list->file;
    if (list_file->id == file->id)
    {
        eHNFFS->file_list->file = list_file->next_file;
        eHNFFS->file_list->count--;
        goto cleanup;
    }

    // Other conditions.
    pre_file = eHNFFS->file_list->file;
    while (pre_file->next_file->id != file->id)
    {
        pre_file = pre_file->next_file;
        if (pre_file == NULL)
        {
            return eHNFFS_ERR_NOFILEOPEN;
        }
    }

    eHNFFS->file_list->count--;
    pre_file->next_file = file->next_file;

cleanup:
    if (file)
    {
        if (file->file_cache.buffer)
            eHNFFS_free(file->file_cache.buffer);
        file->file_cache.buffer = NULL;
        eHNFFS_free(file);
    }
    return err;
}

/**
 * Create a new file in dir.
 */
int eHNFFS_create_file(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_file_ram_t **file_addr,
                       char *name, eHNFFS_size_t namelen)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t size;
    eHNFFS_file_name_flash_t *flash_name;

    // Allocate in-ram memory for file.
    eHNFFS_file_ram_t *file = *file_addr;
    file = eHNFFS_malloc(sizeof(eHNFFS_file_ram_t));
    if (file == NULL)
    {
        err = eHNFFS_ERR_NOMEM;
        return err;
    }

    // Allocate in-ram memory for cache buffer of the file.
    file->file_cache.buffer = eHNFFS_malloc(eHNFFS_FILE_CACHE_SIZE);
    if (file->file_cache.buffer == NULL)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    // Allocate id for the new file.
    err = eHNFFS_id_alloc(eHNFFS, &file->id);
    if (err)
    {
        goto cleanup;
    }

    // Initialize basic message for the file.
    file->father_id = dir->id;
    file->type = eHNFFS_DATA_REG;
    file->file_size = 0;
    file->pos = 0;
    file->file_cache.sector = eHNFFS_NULL;
    file->file_cache.off = eHNFFS_NULL;
    file->file_cache.mode = 0;
    file->file_cache.size = 0;
    memset(file->file_cache.buffer, eHNFFS_NULL, eHNFFS_FILE_CACHE_SIZE);

    // Add the in-ram file to file list.
    file->next_file = eHNFFS->file_list->file;
    eHNFFS->file_list->file = file;

    // Create file name data and initialize it.
    size = sizeof(eHNFFS_file_name_flash_t) + namelen;
    flash_name = eHNFFS_malloc(size);
    if (flash_name == NULL)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }
    flash_name->head = eHNFFS_MKDHEAD(0, 1, file->id, eHNFFS_DATA_FILE_NAME, size);
    memcpy(flash_name->name, name, namelen);

    // Prog file name data to its father dir, which means we have already
    // createn a new file. After proging, flash name is no use and we should
    // free its memory.
    err = eHNFFS_dir_prog(eHNFFS, dir, flash_name, size, eHNFFS_FORWARD);
    eHNFFS_free(flash_name);
    flash_name = NULL;
    if (err)
    {
        goto cleanup;
    }

    // Notice that file.off can be calculated by dir.forward - size.
    // Because we can make sure that after proging, dir tail wouldn't change.
    file->sector = dir->tail;
    file->off = dir->forward - size;
    file->namelen = namelen;
    *file_addr = file;
    eHNFFS->file_list->count++;

    return err;

cleanup:
    if (file)
    {
        if (file->file_cache.buffer)
            eHNFFS_free(file->file_cache.buffer);
        eHNFFS_free(file);
    }
    return err;
}

/**
 * Read small file data function.
 */
int eHNFFS_small_file_read(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                           void *buffer, eHNFFS_size_t size)
{
    eHNFFS_sfile_data_flash_t *sfile_data = (eHNFFS_sfile_data_flash_t *)file->file_cache.buffer;
    uint8_t *pos = sfile_data->data + file->pos;
    memcpy(buffer, pos, size);
    return size;
}

/**
 * Read big file data through (begin, off)(usually it's a index entry).
 */
int eHNFFS_big_file_read_once(eHNFFS_t *eHNFFS, eHNFFS_size_t begin,
                              eHNFFS_off_t off, eHNFFS_size_t len, void *buffer)
{
    int err = eHNFFS_ERR_OK;

    // Change (begin, off) to valid (sector, off).
    eHNFFS_size_t sector = begin;
    while (off >= eHNFFS->cfg->sector_size)
    {
        sector++;
        off -= eHNFFS->cfg->sector_size;
    }

    // Read data to buffer directly, notice that because read big file data
    // usually very big and only read once at a time, we don't use cache.
    uint8_t *data = (uint8_t *)buffer;
    while (len > 0)
    {
        eHNFFS_size_t size = eHNFFS_min(eHNFFS->cfg->sector_size - off, len);
        err = eHNFFS->cfg->read(eHNFFS->cfg, sector, off, data, size);
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            return err;
        }

        data += size;
        len -= size;
        off += size;
        if (off == eHNFFS->cfg->sector_size)
        {
            off = sizeof(eHNFFS_bfile_sector_flash_t);
            sector++;
        }
    }

    return err;
}

/**
 * Read function of big file.
 */
int eHNFFS_big_file_read(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                         void *buffer, eHNFFS_size_t size)
{
    int err = eHNFFS_ERR_OK;

    // Calculate the number of index the file has.
    int num = (file->file_cache.size - sizeof(eHNFFS_head_t)) /
              sizeof(eHNFFS_bfile_index_ram_t);
    eHNFFS_bfile_index_flash_t *index = (eHNFFS_bfile_index_flash_t *)file->file_cache.buffer;

    // Read module.
    eHNFFS_off_t off = 0;
    eHNFFS_size_t rest_size = size;
    uint8_t *data = (uint8_t *)buffer;
    for (int i = 0; i < num; i++)
    {
        if (off + index->index[i].size <= file->pos)
        {
            // It's not we should read, just skip.
            off += index->index[i].size;
            continue;
        }

        // Read a index of data once.
        // file->pos - off is used to process the situation that what we
        // begin to read is not the start of the index.
        eHNFFS_size_t len = eHNFFS_min(index->index[i].size - (file->pos - off),
                                       rest_size);

        eHNFFS_size_t temp_sector = index->index[i].sector;
        eHNFFS_off_t temp_off = index->index[i].off + (file->pos - off);
        while (temp_off >= eHNFFS->cfg->sector_size)
        {
            temp_off -= eHNFFS->cfg->sector_size;
            temp_sector++;
        }

        err = eHNFFS_big_file_read_once(eHNFFS, temp_sector, temp_off, len, data);
        if (err)
        {
            return err;
        }

        data += len;
        file->pos += len;
        off += index->index[i].size;
        rest_size -= len;
        if (rest_size == 0)
            break;
    }

    return size;
}

/**
 * Write data to small file.
 */
int eHNFFS_small_file_write(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                            const void *buffer, eHNFFS_size_t size)
{
    // Write to file buffer.
    eHNFFS_sfile_data_flash_t *small_file = (eHNFFS_sfile_data_flash_t *)file->file_cache.buffer;
    uint8_t *data = small_file->data + file->pos;
    memcpy(data, buffer, size);

    // Change message.
    file->pos += size;
    file->file_size = eHNFFS_max(file->file_size, file->pos);
    file->file_cache.size = file->file_size + sizeof(eHNFFS_head_t);
    file->file_cache.mode = 1; // Flag.

    return size;
}

/**
 * Write data function when small file turns to big file.
 * Or the first time we prog is big data.
 */
int eHNFFS_s2b_file_write(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                          const void *buffer, eHNFFS_size_t size)
{
    int err = eHNFFS_ERR_OK;

    // Calculate the number of sectors we need.
    eHNFFS_size_t sector = eHNFFS_NULL;
    eHNFFS_size_t off = sizeof(eHNFFS_bfile_sector_flash_t);
    eHNFFS_size_t num = eHNFFS_alignup(file->pos + size, eHNFFS->cfg->sector_size - off) /
                        (eHNFFS->cfg->sector_size - off);
    err = eHNFFS_sector_alloc(eHNFFS, eHNFFS->manager, eHNFFS_SECTOR_BFILE, num,
                              eHNFFS_NULL, file->id, file->father_id, &sector, NULL);
    if (err)
    {
        return err;
    }

    // Prog the part of data in file cache.
    eHNFFS_size_t begin = sector;
    err = eHNFFS_bfile_prog(eHNFFS, &sector, &off, file->file_cache.buffer + sizeof(eHNFFS_head_t),
                            file->pos);
    if (err)
    {
        return err;
    }

    // Prog the part of data in buffer.
    err = eHNFFS_bfile_prog(eHNFFS, &sector, &off, buffer, size);
    if (err)
    {
        return err;
    }

    // Change basic message.
    file->pos += size;
    file->file_size = file->pos;

    // Delete old data.
    eHNFFS_size_t head = *(eHNFFS_head_t *)file->file_cache.buffer;

    if (head != eHNFFS_NULL)
    {

        err = eHNFFS_data_delete(eHNFFS, file->father_id, file->file_cache.sector,
                                 file->file_cache.off, eHNFFS_dhead_dsize(head));
        if (err)
        {
            return err;
        }
    }

    // Create new big file index data.
    eHNFFS_bfile_index_flash_t *bfile_index = (eHNFFS_bfile_index_flash_t *)file->file_cache.buffer;
    eHNFFS_size_t index_len = sizeof(eHNFFS_bfile_index_flash_t) + sizeof(eHNFFS_bfile_index_ram_t);
    bfile_index->head = eHNFFS_MKDHEAD(0, 1, file->id, eHNFFS_DATA_BFILE_INDEX, index_len);
    bfile_index->index[0].sector = begin;
    bfile_index->index[0].off = sizeof(eHNFFS_bfile_sector_flash_t);
    bfile_index->index[0].size = file->file_size;

    // Find file's father dir.
    eHNFFS_dir_ram_t *dir = eHNFFS->dir_list->dir;
    while (dir != NULL && dir->id != file->father_id)
        dir = dir->next_dir;

    if (dir == NULL)
    {
        return eHNFFS_ERR_NODIROPEN;
    }

    // Prog new big file index data to flash.
    err = eHNFFS_dir_prog(eHNFFS, dir, file->file_cache.buffer, index_len, eHNFFS_BACKWARD);
    if (err)
    {
        return err;
    }

    // Change file cache message.
    file->file_cache.sector = dir->tail;
    file->file_cache.off = dir->backward;
    file->file_cache.size = index_len;
    file->file_cache.mode = 0;

    return size;
}

/**
 * Index jump the jump size to a new index.
 */
void eHNFFS_index_jump(eHNFFS_t *eHNFFS, eHNFFS_bfile_index_ram_t *index, eHNFFS_size_t jump_size)
{
    index->size -= jump_size;

    while (jump_size > 0)
    {
        eHNFFS_size_t size = eHNFFS_min(eHNFFS->cfg->sector_size - index->off, jump_size);

        jump_size -= size;
        index->off += size;
        if (index->off == eHNFFS->cfg->sector_size)
        {
            index->sector++;
            index->off = sizeof(eHNFFS_bfile_sector_flash_t);
        }
    }
}

/**
 * Copy data in source index to destinate index.
 */
void eHNFFS_index_cpy(eHNFFS_bfile_index_ram_t *dst_index, eHNFFS_bfile_index_ram_t *src_index)
{
    dst_index->sector = src_index->sector;
    dst_index->off = src_index->off;
    dst_index->size = src_index->size;
}

/**
 * Write data to big file.
 */
int eHNFFS_big_file_write(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                          void *buffer, eHNFFS_size_t size)
{
    int err = eHNFFS_ERR_OK;

    // TODO in the future.
    // Now all index don't use one sector, but it may should change in the future.

    int origin_size = size;

    // Calculate the number of index.
    eHNFFS_bfile_index_flash_t *bfile = (eHNFFS_bfile_index_flash_t *)file->file_cache.buffer;
    eHNFFS_bfile_index_ram_t *bfile_index = bfile->index;
    eHNFFS_size_t index_num = (file->file_cache.size - sizeof(eHNFFS_head_t)) /
                              sizeof(eHNFFS_bfile_index_ram_t);

    // TODO in the future
    // You should change the index num in the future!!
    if (index_num >= eHNFFS_FILE_INDEX_LEN)
    {
        err = eHNFFS_bfile_gc(eHNFFS, file);
        if (err)
        {
            return err;
        }

        index_num = (file->file_cache.size - sizeof(eHNFFS_head_t)) /
                    sizeof(eHNFFS_bfile_index_ram_t);

        // // Debug
        // printf("-----------------------\n");
        // printf("After gc\n");
        // int my_cnt = 0;
        // for (int i = 0; i < index_num; i++)
        // {
        //     printf("%d, %d, %d, %d\n", bfile_index[i].sector, bfile_index[i].off,
        //            bfile_index[i].size, bfile_index[i].size / 4084);
        //     my_cnt += bfile_index[i].size;
        // }
        // printf("total size is %d, index num is %d\n", my_cnt, index_num);
        // printf("-----------------------\n");
    }

    // If it's appended write, We may use free space behind the last index.
    // Jump function is used to calculate free spcae in the last index.
    uint8_t *data = buffer;
    eHNFFS_bfile_index_ram_t temp_index = {
        .sector = bfile_index[index_num - 1].sector,
        .off = bfile_index[index_num - 1].off,
        .size = bfile_index[index_num - 1].size,
    };
    eHNFFS_index_jump(eHNFFS, &temp_index, temp_index.size);

    // If it's appended write there is some free space, prog some data first.
    if (file->pos == file->file_size &&
        temp_index.off != sizeof(eHNFFS_bfile_sector_flash_t))
    {
        eHNFFS_size_t len = eHNFFS_min(eHNFFS->cfg->sector_size - temp_index.off, size);
        eHNFFS->cfg->prog(eHNFFS->cfg, temp_index.sector, temp_index.off,
                          data, len);
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            return err;
        }

        err = eHNFFS_cache_cmp(eHNFFS, temp_index.sector, temp_index.off, data, len);
        if (err < 0)
        {
            return err;
        }

        if (err != eHNFFS_CMP_EQ)
        {
            return eHNFFS_ERR_WRONGPROG;
        }

        data += len;
        size -= len;
        bfile_index[index_num - 1].size += len;
        file->file_cache.mode = 1;
        file->file_size += len;
        file->pos = file->file_size;

        if (size == 0)
        {
            return origin_size;
        }
    }

    // Calculate the number of sectors we need and allocate.
    eHNFFS_size_t sector = eHNFFS_NULL;
    eHNFFS_size_t off = sizeof(eHNFFS_bfile_sector_flash_t);
    eHNFFS_size_t num = eHNFFS_alignup(size, eHNFFS->cfg->sector_size - off) /
                        (eHNFFS->cfg->sector_size - off);

    err = eHNFFS_sector_alloc(eHNFFS, eHNFFS->manager, eHNFFS_SECTOR_BFILE, num,
                              eHNFFS_NULL, file->id, file->father_id, &sector, NULL);
    if (err)
    {
        return err;
    }

    // Prog data to flash.
    eHNFFS_size_t begin = sector;
    err = eHNFFS_bfile_prog(eHNFFS, &sector, &off, buffer, size);
    if (err)
    {
        return err;
    }

    if (file->pos == file->file_size)
    {
        // If it's appended write.
        // temp_index.sector is because when off reaching sector size, we
        // automatically add 1 to temp_index.sector, but the sector is free.
        if (temp_index.sector == begin || temp_index.sector + 1 == begin)
        {
            // If we can merge new index and the last old index.
            bfile_index[index_num - 1].size += size;
            file->file_cache.mode = 1;
            file->file_size += size;
            file->pos += size;
        }
        else
        {
            // If the new index and the last old index can't merge, we should add
            // a new index.
            bfile_index[index_num].sector = begin;
            bfile_index[index_num].off = sizeof(eHNFFS_bfile_sector_flash_t);
            bfile_index[index_num].size = size;
            file->file_cache.size += sizeof(eHNFFS_bfile_index_ram_t);
            file->file_cache.mode = 1;
            file->file_size += size;
            file->pos += size;
        }
        return origin_size;
    }

    // If it's ramdom write, do following things.
    // Record index of data we have just prog.
    eHNFFS_bfile_index_ram_t new_index = {
        .sector = begin,
        .off = sizeof(eHNFFS_bfile_sector_flash_t),
        .size = size,
    };

    // If the first index covered by new index still has valid date in former part,
    // we should record it.
    eHNFFS_bfile_index_ram_t begin_index = {
        .sector = eHNFFS_NULL,
        .off = eHNFFS_NULL,
        .size = eHNFFS_NULL,
    };

    // If the last index covered by new index still has valid data in behind part,
    // we should record it.
    eHNFFS_bfile_index_ram_t end_index = {
        .sector = eHNFFS_NULL,
        .off = eHNFFS_NULL,
        .size = eHNFFS_NULL,
    };

    // Find the first index covered by new index.
    off = 0;
    int i = 0;
    for (i = 0; i < index_num; i++)
    {
        if (off + bfile_index[i].size <= file->pos)
        {
            off += bfile_index[i].size;
        }
        else
            break;
    }

    // If it still has valid data, record it, then change old index to pure deleted index.
    // Notice that if the first sector of changed index still has valid index, we shouldn't
    // set it to old in the future.
    if (off != file->pos)
    {
        eHNFFS_index_cpy(&begin_index, &bfile_index[i]);
        begin_index.size = file->pos - off;
        eHNFFS_index_jump(eHNFFS, &bfile_index[i], file->pos - off);
    }

    // If new data we prog can cover all behind part of file's data, then it's easy to do.
    if (file->pos + size >= file->file_size)
    {

        // Set all deleted sectors to old.
        err = eHNFFS_bfile_sector_old(eHNFFS, &bfile_index[i], index_num - i - 1);
        if (err)
        {
            return err;
        }

        if (begin_index.sector != eHNFFS_NULL)
        {
            eHNFFS_index_cpy(&bfile_index[i], &begin_index);
            i++;
        }
        eHNFFS_index_cpy(&bfile_index[i], &new_index);
        file->file_cache.size = (i + 1) * sizeof(eHNFFS_bfile_index_ram_t) + sizeof(eHNFFS_head_t);
        file->file_cache.mode = 1;
        file->pos += size;
        file->file_size = file->pos;
        return origin_size;
    }

    // Find the last index covered by new index.
    off = 0;
    int j = 0;
    for (j = i; j < index_num; j++)
    {
        // TODO, < change to <=
        if (off + bfile_index[j].size <= size)
        {
            off += bfile_index[j].size;
        }
        else
            break;
    }

    if (off + bfile_index[j].size != size)
    {
        // If not, record valid data to end index.
        eHNFFS_index_cpy(&end_index, &bfile_index[j]);
        eHNFFS_index_jump(eHNFFS, &end_index, size - off);

        // If the first sector of end index still has valid data,
        // we shouldn't erase it.
        // Debug
        // if (end_index.sector == bfile_index->sector)
        if (end_index.sector == bfile_index[j].sector)
            bfile_index[j].sector = eHNFFS_NULL;
        else
        {
            // If the last sector has valid data, then we shouldn't delete it.
            // Size is true size we should prog, off is size of before index.
            // size - off tells us how many size of space is free in the index.
            bfile_index[j].size = (size - off);

            // If end index's first sector isn't, we shouldn't delete it.
            // i.e index[j] should decrease this size.
            bfile_index[j].size -= (end_index.off - sizeof(eHNFFS_bfile_index_flash_t));
        }
    }

    if (i == j && (end_index.sector == bfile_index[i].sector))
    {
        // If i and j are same, it means that both the index's head and tail data should be reserved.
        // If two of sectors are same, we can't free any sector.
        bfile_index[i].sector = eHNFFS_NULL;
    }
    else if (bfile_index[i].sector != eHNFFS_NULL && bfile_index[i].off > sizeof(eHNFFS_bfile_index_flash_t))
    {
        // We can't free the first sector if it has valid data, i.e off is larger than sizeof.
        bfile_index[i].size -= eHNFFS_min(eHNFFS->cfg->sector_size - bfile_index[i].off,
                                          bfile_index[i].size);
        if (bfile_index[i].size == 0)
            // Skip size is so small that it only has the sector, so we should free nothing.
            bfile_index[i].sector = eHNFFS_NULL;
        else
        {
            // Normal condition.
            bfile_index[i].off = sizeof(eHNFFS_bfile_index_flash_t);
            bfile_index[i].sector++;
        }
    }

    // Set all deleted sectors to old.
    err = eHNFFS_bfile_sector_old(eHNFFS, &bfile_index[i], j - i + 1);
    if (err)
    {
        return err;
    }

    // Calculate number of new/changed index we should prog.
    eHNFFS_size_t new_index_num = 1;
    if (begin_index.sector != eHNFFS_NULL)
        new_index_num++;
    if (end_index.sector != eHNFFS_NULL)
        new_index_num++;

    // Calculate the number of index behin we cover and move vaild index.
    if (j < index_num)
    {
        num = index_num - j - 1;
        if (num > 0)
        {
            memcpy(&bfile_index[i + new_index_num], &bfile_index[j + 1],
                   num * sizeof(eHNFFS_bfile_index_ram_t));
        }
    }

    // Write begin index.
    int k = i;
    if (begin_index.sector != eHNFFS_NULL)
    {
        eHNFFS_index_cpy(&bfile_index[k], &begin_index);
        k++;
    }

    // Write new index.
    eHNFFS_index_cpy(&bfile_index[k], &new_index);
    k++;

    // Write end index.
    if (end_index.sector != eHNFFS_NULL)
    {
        eHNFFS_index_cpy(&bfile_index[k], &end_index);
        k++;
    }

    num = j - i + 1;
    file->file_cache.size += (int)(new_index_num - num) *
                             sizeof(eHNFFS_bfile_index_ram_t);
    file->file_cache.mode = 1;
    file->pos = file->pos + size;
    file->file_size = eHNFFS_max(file->pos, file->file_size);

    return origin_size;
}
