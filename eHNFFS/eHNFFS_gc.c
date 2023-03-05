/**
 * The basic gc operations of eHNFFS
 */

#include "eHNFFS_gc.h"
#include "eHNFFS_cache.h"
#include "eHNFFS_head.h"
#include "eHNFFS_manage.h"
#include "eHNFFS_dir.h"
#include "eHNFFS_file.h"
#include "eHNFFS_tree.h"

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * --------------------------------------------------------    Superblock gc functions    --------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Initialization function of in-ram superblock structure.
 */
int eHNFFS_super_initialization(eHNFFS_t *eHNFFS, eHNFFS_superblock_ram_t **super_addr)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_superblock_ram_t *superblock = eHNFFS_malloc(sizeof(eHNFFS_superblock_ram_t));
    if (!superblock)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    superblock->sector = 0;
    superblock->free_off = 0;
    *super_addr = superblock;
    return err;

cleanup:
    if (superblock)
        eHNFFS_free(superblock);

    return err;
}

/**
 * If current used superblock is full, we should erase, migrate data and use another one.
 */
int eHNFFS_superblock_change(eHNFFS_t *eHNFFS, eHNFFS_superblock_ram_t *super)
{
    int err = eHNFFS_ERR_OK;

    // The basic message of the other superblock.
    super->sector = (super->sector + 1) % 2;
    super->free_off = 0;
    eHNFFS_head_t head;
    err = eHNFFS->cfg->read(eHNFFS->cfg, super->sector, 0,
                            &head, sizeof(eHNFFS_head_t));
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    // Erase it.
    err = eHNFFS_sector_erase(eHNFFS, super->sector);
    if (err)
    {
        return err;
    }

    // Prog basic sector head message.
    eHNFFS_head_t new_head = eHNFFS_MKSHEAD(0, eHNFFS_SECTOR_USING, eHNFFS_SECTOR_SUPER,
                                            eHNFFS_shead_extend(head) + 2,
                                            eHNFFS_shead_etimes(head) + 1);
    err = eHNFFS_direct_shead_prog(eHNFFS, super->sector, &super->free_off,
                                   sizeof(eHNFFS_size_t), &new_head);
    if (err)
    {
        return err;
    }

    // Prog basic superblock messages.
    err = eHNFFS_superblock_prog(eHNFFS, super, eHNFFS->pcache);
    if (err)
    {
        return err;
    }

    // superblock(0 and 1) doesn't need to change sector map message.
    return err;
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * -----------------------------------------------------------    Dir gc functions    ------------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

static int eHNFFS_file_delete_set(eHNFFS_t *eHNFFS, eHNFFS_file_list_ram_t *flist, eHNFFS_size_t father_id)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_file_ram_t *file = flist->file;
    for (int i = 0; i < flist->count; i++)
    {
        // If the file belongs to father id, has something written to flash(i.e not eHNFFS_NULL)
        // and mode is valid(flag tells us has change).
        if (file->father_id == father_id &&
            file->file_cache.sector != eHNFFS_NULL &&
            file->file_cache.mode)
        {
            eHNFFS_head_t *head = (eHNFFS_head_t *)file->file_cache.buffer;
            err = eHNFFS_data_delete(eHNFFS, file->father_id, file->file_cache.sector,
                                     file->file_cache.off, eHNFFS_dhead_dsize(*head));
            if (err)
            {
                return err;
            }
        }
    }

    return err;
}

/**
 * Flush all in-ram file that belongs to dir to flash.
 */
int eHNFFS_dir_file_flush(eHNFFS_t *eHNFFS, eHNFFS_file_list_ram_t *flist, eHNFFS_dir_ram_t *dir)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_file_ram_t *file = flist->file;
    for (int i = 0; i < flist->count; i++)
    {
        // Flushing all in-ram file that belongs to dir to flash.
        // mode is a flag tells us it need to flush.
        if (file->father_id == dir->id && file->file_cache.mode)
        {
            eHNFFS_head_t old_head = *(eHNFFS_head_t *)file->file_cache.buffer;
            eHNFFS_head_t *head = (eHNFFS_head_t *)file->file_cache.buffer;
            *head = eHNFFS_MKDHEAD(0, 1, eHNFFS_dhead_id(old_head), eHNFFS_dhead_type(old_head),
                                   file->file_cache.size);
            err = eHNFFS_dir_prog(eHNFFS, dir, file->file_cache.buffer, file->file_cache.size, eHNFFS_BACKWARD);
            if (err)
            {
                return err;
            }

            // Update file cache message.
            file->file_cache.sector = dir->tail;
            file->file_cache.off = dir->backward;
            file->file_cache.mode = 0;
        }
    }

    return err;
}

/**
 * Garbage collection function for dir.
 * Region and map are only used for region migration operations.
 */
int eHNFFS_dir_gc(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_size_t region,
                  uint32_t *map_buffer)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_size_t old_tail = dir->tail;

    // If in-ram file has changed and not flush to flash, we should
    // turn in-flash data to delete first.
    err = eHNFFS_file_delete_set(eHNFFS, eHNFFS->file_list, dir->id);
    if (err)
    {
        return err;
    }

    // How many sector number the dir has.
    int snum = 1;
    if (dir->backward_list != NULL)
    {
        eHNFFS_list_ram_t *p = dir->backward_list;
        while (p->next != NULL)
        {
            snum++;
            p = p->next;
        }
    }
    snum -= dir->old_space / eHNFFS->cfg->sector_size;
    eHNFFS_ASSERT(snum <= eHNFFS->manager->region_size);

    // Find a region that can store all sectors.
    while (eHNFFS->manager->dir_map->free_num < snum)
    {
        err = eHNFFS_sector_nextsmap(eHNFFS, eHNFFS->manager, eHNFFS_SECTOR_DIR);
        if (err)
        {
            return err;
        }
    }

    // Flush data in pcache to flash first, for we need to use now.
    err = eHNFFS_cache_flush(eHNFFS, eHNFFS->pcache, NULL, eHNFFS_VALIDATE);
    if (err)
    {
        return err;
    }

    // Starting gc.
    err = eHNFFS_dtraverse_gc(eHNFFS, dir, region, map_buffer);
    if (err)
    {
        return err;
    }

    // Flushing file to flash.
    err = eHNFFS_dir_file_flush(eHNFFS, eHNFFS->file_list, dir);
    if (err)
    {
        return err;
    }

    // TODO in the future, it's better to merge it with dtraverse_gc function.
    // Turn state of old sectors to old so we can reuse.
    // Including sector head and erase map.
    err = eHNFFS_dir_old(eHNFFS, old_tail);
    if (err)
    {
        return err;
    }

    // Update dir name and tail message in its father dir.
    err = eHNFFS_dir_update(eHNFFS, dir);
    if (err)
    {
        return err;
    }

    return err;
}

eHNFFS_size_t eHNFFS_sector_num(eHNFFS_t *eHNFFS, eHNFFS_off_t off, eHNFFS_size_t len)
{
    eHNFFS_size_t num = 0;
    while (len > 0)
    {
        eHNFFS_size_t size = eHNFFS_min(len, eHNFFS->cfg->sector_size - off);

        num++;
        len -= size;
        off += size;
        if (off == eHNFFS->cfg->sector_size)
        {
            off = sizeof(eHNFFS_bfile_sector_flash_t);
        }
    }

    return num;
}

void eHNFFS_map_buffer_set(eHNFFS_t *eHNFFS, eHNFFS_bfile_index_ram_t *index, eHNFFS_size_t num,
                           uint32_t *map_buffer)
{
    for (int i = 0; i < num; i++)
    {
        eHNFFS_size_t cnt = eHNFFS_sector_num(eHNFFS, index[i].off, index[i].size);
        int f1 = index[i].sector % eHNFFS->manager->region_size / eHNFFS_SIZE_T_NUM;
        int f2 = index[i].sector % eHNFFS->manager->region_size % eHNFFS_SIZE_T_NUM;

        for (int j = 0; j < cnt; j++)
        {
            map_buffer[f1] |= (1U << f2);
            f2++;
            if (f2 == eHNFFS_SIZE_T_NUM)
            {
                f1++;
                f2 = 0;
            }
        }
    }
}

/**
 * Migrate data of big file in region to other regions.
 */
int eHNFFS_bfile_migration(eHNFFS_t *eHNFFS, eHNFFS_size_t sector,
                           eHNFFS_size_t region, uint32_t *map_buffer)
{
    int err = eHNFFS_ERR_OK;

    // Read basic data in sector to know what id it belonngs to.
    eHNFFS_bfile_sector_flash_t bfile_sector;
    err = eHNFFS->cfg->read(eHNFFS->cfg, sector, 0, &bfile_sector,
                            sizeof(eHNFFS_bfile_sector_flash_t));
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    // Find file's father dir through father id.
    eHNFFS_dir_ram_t *father_dir = NULL;
    err = eHNFFS_father_id_find(eHNFFS, bfile_sector.father_id, eHNFFS_MIGRATE,
                                &father_dir);
    if (err)
    {
        return err;
    }

    // Open the file to get its whole index message.
    // Notice that open file for migration don't need to delete file name in
    // father dir, so (sector, off) of file name could be nothing.
    eHNFFS_file_ram_t *file = NULL;
    err = eHNFFS_file_lowopen(eHNFFS, father_dir, bfile_sector.id, eHNFFS_NULL,
                              eHNFFS_NULL, eHNFFS_NULL, &file, eHNFFS_MIGRATE);
    if (err)
    {
        return err;
    }

    // Calculate the number of index in the file.
    eHNFFS_bfile_index_flash_t *bfile_index = (eHNFFS_bfile_index_flash_t *)file->file_cache.buffer;
    eHNFFS_size_t num = (eHNFFS_dhead_dsize(bfile_index->head) -
                         sizeof(eHNFFS_head_t)) /
                        sizeof(eHNFFS_bfile_index_ram_t);

    eHNFFS_size_t first = eHNFFS_NULL;
    eHNFFS_size_t current = eHNFFS_NULL;
    for (int i = 0; i < num; i++)
    {
        // Find index that in the region and ready to migrate data.
        // We should make sure that a index just includes one region.
        if (bfile_index->index[i].sector / eHNFFS->manager->region_size == region)
        {
            current = i;
            if (first == eHNFFS_NULL)
            {
                first = current;
            }
        }

        // Do migrate.
        if ((current != i && first != eHNFFS_NULL) ||
            (i == num - 1))
        {
            // Set bits in map_buffer to 1 first(means that they are already usefule)
            eHNFFS_map_buffer_set(eHNFFS, &bfile_index->index[i], current - first + 1,
                                  map_buffer);

            eHNFFS_size_t len = 0;
            for (int i = first; i <= current; i++)
            {
                len += bfile_index->index[i].size;
            }

            // Migrate this part of data.
            err = eHNFFS_bfile_part_gc(eHNFFS, file, first, current, len, num,
                                       eHNFFS_sector_num(eHNFFS,
                                                         sizeof(eHNFFS_bfile_sector_flash_t),
                                                         len));
            if (err)
            {
                return err;
            }

            // Change basic index number message.
            i = first;
            num -= (current - first);
            first = eHNFFS_NULL;
        }
    }

    return err;
}

/**
 * Free opened file and dir during migration.
 */
int eHNFFS_dir_file_free(eHNFFS_t *eHNFFS)
{
    int err = eHNFFS_ERR_OK;

    // Close opened file in file list.
    eHNFFS_file_list_ram_t *file_list = eHNFFS->file_list;
    while (file_list->file->type == eHNFFS_MIGRATE)
    {
        err = eHNFFS_file_lowclose(eHNFFS, file_list->file);
        if (err)
        {
            return err;
        }
    }

    // Close opened dir in dir list.
    eHNFFS_dir_list_ram_t *dir_list = eHNFFS->dir_list;
    while (dir_list->dir->type == eHNFFS_MIGRATE)
    {
        err = eHNFFS_dir_lowclose(eHNFFS, dir_list->dir);
        if (err)
        {
            return err;
        }
    }

    return err;
}

/**
 * Migrate all useful data in region to other place in flash.
 */
int eHNFFS_bfile_region_migration(eHNFFS_t *eHNFFS, eHNFFS_size_t region, uint32_t *map_buffer)
{
    int err = eHNFFS_ERR_OK;

    // Calculate basic bit map message.
    eHNFFS_size_t threshold = eHNFFS->manager->region_size / eHNFFS_SIZE_T_NUM;
    int i = 0;
    int j = 0;
    for (i = 0; i < threshold; i++)
    {
        for (j = 0; j < eHNFFS_SIZE_T_NUM; j++)
        {
            // If we find a sector is in-use, we are ready to migrate it.
            if (((map_buffer[i] >> j)) & 1U == 0)
            {
                eHNFFS_size_t sector = region * eHNFFS->manager->region_size +
                                       i * eHNFFS_SIZE_T_NUM + j;
                err = eHNFFS_bfile_migration(eHNFFS, sector, region, map_buffer);
                if (err)
                {
                    return err;
                }
            }
        }
    }

    // Free all opened dir and file during migration.
    err = eHNFFS_dir_file_free(eHNFFS);
    return err;
}

/**
 * Garbage collection function for big file.
 * Notice that the main purpose of it to reduce fragmentation.
 */
int eHNFFS_bfile_gc(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t len = eHNFFS->cfg->sector_size - sizeof(eHNFFS_bfile_sector_flash_t);
    eHNFFS_size_t num = eHNFFS_alignup(file->file_size, len) / len;

    if (num <= eHNFFS->manager->region_size)
    {
        // Start gc.
        err = eHNFFS_ftraverse_gc(eHNFFS, file, num);
        if (err)
        {
            return err;
        }
    }
    else if (num > eHNFFS->manager->region_size)
    {
        err = eHNFFS_part_ftraverse_gc(eHNFFS, file);
        if (err)
        {
            return err;
        }
    }

    return err;
}

/**
 * Do migration to region.
 */
int eHNFFS_region_migration(eHNFFS_t *eHNFFS, eHNFFS_size_t region, int type)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_flash_manage_ram_t *manager = eHNFFS->manager;
    eHNFFS_size_t reserve = manager->region_map->reserve;

    // Add reserve to corresponding map.
    err = eHNFFS_reserve_region_use(manager->region_map, type);
    if (err)
    {
        return err;
    }

    // Change the corresponding map region to reserve region. So we don't
    // need to erase all old data in reserve region in advance.
    eHNFFS_map_ram_t *map = (type == eHNFFS_SECTOR_DIR) ? manager->dir_map
                                                        : manager->bfile_map;
    err = eHNFFS_ram_map_change(eHNFFS, reserve, manager->region_size, map);
    if (err)
    {
        return err;
    }

    // Remove candidate region from corresponding region map.
    err = eHNFFS_remove_region(eHNFFS, eHNFFS->manager->region_map, type, region);
    if (err)
    {
        return err;
    }

    // Read the sectors' using message of region.
    // Notice that we should need to read both of the two maps.
    uint32_t map_buffer[manager->region_size / eHNFFS_SIZE_T_NUM];
    err = eHNFFS_region_read(eHNFFS, region, map_buffer);
    if (err)
    {
        return err;
    }

    if (type == eHNFFS_SECTOR_BFILE)
    {
        err = eHNFFS_bfile_region_migration(eHNFFS, region, map_buffer);
        if (err)
        {
            return err;
        }
    }
    else
    {
        err = eHNFFS_dir_region_migration(eHNFFS, region, map_buffer);
        if (err)
        {
            return err;
        }
    }

    manager->region_map->reserve = region;
    return err;
}

// Now not useful, we don't need to do it.
int eHNFFS_reserve_region_init(eHNFFS_t *eHNFFS, eHNFFS_size_t region)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t region_size = eHNFFS->manager->region_size;
    eHNFFS_size_t end = (region + 1) * region_size;
    eHNFFS_head_t head;
    for (int i = end - region_size; i < end; i++)
    {
        // Read all sectors' head in the region sequencialy.
        err = eHNFFS->cfg->read(eHNFFS->cfg, i, 0, &head, sizeof(eHNFFS_head_t));
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            return err;
        }

        switch (eHNFFS_shead_state(head))
        {
        case eHNFFS_SECTOR_OLD:
            // If head type is old, we should erase it and reprog etimes message.
            // Normally we don't need to directly erase
            head = eHNFFS_MKSHEAD(1, eHNFFS_SECTOR_FREE, eHNFFS_SECTOR_NOTSURE,
                                  0xff, eHNFFS_shead_etimes(head) + 1);
            err = eHNFFS->cfg->erase(eHNFFS->cfg, i);
            eHNFFS_ASSERT(err <= 0);
            if (err)
            {
                return err;
            }

            err = eHNFFS->cfg->prog(eHNFFS->cfg, i, 0, &head, sizeof(eHNFFS_head_t));
            eHNFFS_ASSERT(err <= 0);
            if (err)
            {
                return err;
            }

            err = eHNFFS_cache_cmp(eHNFFS, i, 0, &head, sizeof(eHNFFS_head_t));
            if (err < 0)
            {
                return err;
            }

            if (err != eHNFFS_CMP_EQ)
            {
                return eHNFFS_ERR_WRONGPROG;
            }
            break;

        case eHNFFS_SECTOR_FREE:
            // It's ok.
            break;

        default:
            // After migration, the region should have no valid data.
            return eHNFFS_ERR_WRONGHEAD;
        }
    }

    return err;
}
