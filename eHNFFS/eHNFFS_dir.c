/**
 * Dir related operations.
 */

#include "eHNFFS_dir.h"
#include "eHNFFS_head.h"
#include "eHNFFS_cache.h"
#include "eHNFFS_tree.h"
#include "eHNFFS_gc.h"
#include "eHNFFS_manage.h"

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * ------------------------------------------------------------    List operations    ------------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

int eHNFFS_list_insert(eHNFFS_dir_ram_t *dir, eHNFFS_off_t off)
{
    // eHNFFS_list_ram_t *list = dir->backward_list;
    if (dir->backward_list == NULL)
    {
        dir->backward_list = eHNFFS_malloc(sizeof(eHNFFS_list_ram_t));
        if (!dir->backward_list)
        {
            return eHNFFS_ERR_NOMEM;
        }
        dir->backward_list->off = off;
        dir->backward_list->next = NULL;
        return eHNFFS_ERR_OK;
    }

    // tail insert
    // while (list->next != NULL)
    // {
    //     list = list->next;
    // }

    // eHNFFS_list_ram_t *entry = eHNFFS_malloc(sizeof(eHNFFS_list_ram_t));
    // if (!entry)
    // {
    //     return eHNFFS_ERR_NOMEM;
    // }

    // list->next = entry;
    // entry->off = off;
    // entry->next = NULL;

    // head insert
    eHNFFS_list_ram_t *entry = eHNFFS_malloc(sizeof(eHNFFS_list_ram_t));
    if (!entry)
    {
        return eHNFFS_ERR_NOMEM;
    }

    entry->off = off;
    entry->next = dir->backward_list;
    dir->backward_list = entry;

    return eHNFFS_ERR_OK;
}

void eHNFFS_list_delete(eHNFFS_list_ram_t *list)
{

    if (list == NULL)
        return;

    eHNFFS_list_ram_t *next;
    while (list != NULL)
    {
        next = list->next;
        eHNFFS_free(list);
        list = next;
    }
    return;
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * --------------------------------------------------------    Dir Traversing operations    ------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Initialize dir list structure.
 */
int eHNFFS_dir_list_initialization(eHNFFS_t *eHNFFS, eHNFFS_dir_list_ram_t **list_addr)
{
    int err = eHNFFS_ERR_OK;

    // Allocate memory for dir list.
    eHNFFS_dir_list_ram_t *list = eHNFFS_malloc(sizeof(eHNFFS_dir_list_ram_t));
    if (!list)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    // Initialize basic message.
    list->count = 0;
    list->dir = NULL;
    *list_addr = list;
    return err;

cleanup:
    if (list)
        eHNFFS_free(list);

    return err;
}

/**
 * Free dir list.
 */
void eHNFFS_dir_list_free(eHNFFS_t *eHNFFS, eHNFFS_dir_list_ram_t *list)
{
    eHNFFS_dir_ram_t *dir = list->dir;
    eHNFFS_dir_ram_t *next = NULL;
    while (dir != NULL)
    {
        if (dir->backward_list)
            eHNFFS_free(dir->backward_list);

        next = dir->next_dir;
        eHNFFS_free(dir);
        dir = next;
    }
    eHNFFS_free(list);
}

/**
 * Free specific dir in dir list.
 */
int eHNFFS_dir_free(eHNFFS_dir_list_ram_t *list, eHNFFS_dir_ram_t *dir)
{
    eHNFFS_dir_ram_t *list_dir = list->dir;

    eHNFFS_list_delete(dir->backward_list);

    // If file is at the begin of list.
    if (list_dir->id == dir->id)
    {
        list->dir = dir->next_dir;
        eHNFFS_free(dir);
        list->count--;
        return eHNFFS_ERR_OK;
    }

    // Find file's previous file in file list.
    while (list_dir->next_dir != NULL)
    {
        if (list_dir->next_dir->id == dir->id)
            break;
        list_dir = list_dir->next_dir;
    }

    if (list_dir->next_dir == NULL)
        return eHNFFS_ERR_NODIROPEN;
    else
    {
        list_dir->next_dir = dir->next_dir;
        eHNFFS_free(dir);
        list->count--;
        return eHNFFS_ERR_OK;
    }
}

/**
 * Find the name in dir.
 */
int eHNFFS_dtraverse_name(eHNFFS_t *eHNFFS, eHNFFS_size_t tail, char *name, eHNFFS_size_t namelen,
                          int type, bool *if_find, eHNFFS_size_t *sector, eHNFFS_size_t *id,
                          eHNFFS_size_t *name_sector, eHNFFS_off_t *name_off)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_size_t next = eHNFFS_NULL;
    eHNFFS_size_t current_sector = tail;
    eHNFFS_size_t off = 0;

    while (true)
    {
        // Read data of dir to cache first.
        eHNFFS_size_t size = eHNFFS_min(eHNFFS->cfg->cache_size,
                                        eHNFFS->cfg->sector_size - off);
        err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, eHNFFS->rcache,
                                   current_sector, off, size);
        if (err)
        {
            return err;
        }
        uint8_t *data = eHNFFS->rcache->buffer;

        // Each sector head of dir stores the position of next sector.
        if (off == 0)
        {
            eHNFFS_dir_sector_flash_t *shead = (eHNFFS_dir_sector_flash_t *)data;
            err = eHNFFS_shead_check(shead->head, eHNFFS_SECTOR_USING, eHNFFS_SECTOR_DIR);
            if (err)
            {
                return err;
            }
            next = shead->pre_sector;
            data += sizeof(eHNFFS_dir_sector_flash_t);
            off += sizeof(eHNFFS_dir_sector_flash_t);
        }

        eHNFFS_head_t head;
        while (true)
        {
            head = *(eHNFFS_head_t *)data;

            // Check if the head is valid.
            err = eHNFFS_dhead_check(head, eHNFFS_NULL, (int)eHNFFS_NULL);
            if (err)
            {
                return err;
            }

            if (off + eHNFFS_dhead_dsize(head) > eHNFFS->rcache->off + size)
            {
                if (head == eHNFFS_NULL && next != eHNFFS_NULL)
                {
                    current_sector = next;
                    off = 0;
                    break;
                }
                else if (head == eHNFFS_NULL)
                {
                    *if_find = false;
                    return err;
                }

                // We have read whole data in cache.
                break;
            }

            eHNFFS_size_t len;
            bool if_change = false;
            switch (eHNFFS_dhead_type(head))
            {
            case eHNFFS_DATA_DIR_NAME:
                len = eHNFFS_dhead_dsize(head);
                if (type == eHNFFS_DATA_DIR)
                {
                    eHNFFS_dir_name_flash_t *fname = (eHNFFS_dir_name_flash_t *)data;
                    if (eHNFFS_dhead_dsize(fname->head) - sizeof(eHNFFS_dir_name_flash_t) !=
                        namelen)
                    {
                        break;
                    }

                    if (!memcmp(name, fname->name, namelen))
                    {
                        *if_find = true;
                        *sector = fname->tail;
                        *id = eHNFFS_dhead_id(head);
                        if (name_sector != NULL)
                        {
                            *name_sector = current_sector;
                            *name_off = off;
                        }
                        return err;
                    }
                }
                break;

            case eHNFFS_DATA_FILE_NAME:
                len = eHNFFS_dhead_dsize(head);
                if (type == eHNFFS_DATA_REG)
                {
                    eHNFFS_file_name_flash_t *fname = (eHNFFS_file_name_flash_t *)data;
                    if (eHNFFS_dhead_dsize(fname->head) - sizeof(eHNFFS_file_name_flash_t) !=
                        namelen)
                    {
                        break;
                    }

                    if (!memcmp(name, fname->name, namelen))
                    {
                        *if_find = true;
                        *id = eHNFFS_dhead_id(head);
                        if (name_sector != NULL)
                        {
                            *name_sector = current_sector;
                            *name_off = off;
                        }
                        return err;
                    }
                }
                break;

            case eHNFFS_DATA_SKIP:
                if_change = true;
                len = eHNFFS_dhead_dsize(head);
                break;

            case eHNFFS_DATA_FREE:
                if (head == eHNFFS_NULL &&
                    current_sector == tail)
                    if_change = true;
                else
                {
                    err = eHNFFS_ERR_WRONGCAL;
                    return err;
                }
                len = 0;
                break;

            case eHNFFS_DATA_DELETE:
                len = eHNFFS_dhead_dsize(head);
                break;

            default:
                err = eHNFFS_ERR_WRONGCAL;
                return err;
            }

            off += len;
            data += len;

            if (off - eHNFFS->rcache->off >= eHNFFS->cfg->cache_size)
            {
                break;
            }

            if (if_change && next != eHNFFS_NULL)
            {
                // Traverse the next sector.
                if_change = false;
                current_sector = next;
                off = 0;
                break;
            }
            else if (if_change && next == eHNFFS_NULL)
            {
                *if_find = false;
                return err;
            }
        }
    }
}

int eHNFFS_dtraverse_data(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_size_t id,
                          void *buffer, eHNFFS_size_t *index_sector, eHNFFS_off_t *index_off)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t sector = dir->tail;
    eHNFFS_size_t off = dir->backward;
    eHNFFS_size_t next = eHNFFS_NULL;
    eHNFFS_list_ram_t *list = dir->backward_list;

    while (true)
    {
        // Read data of dir to cache first.
        eHNFFS_size_t size = eHNFFS_min(eHNFFS->cfg->cache_size,
                                        eHNFFS->cfg->sector_size - off);
        err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, eHNFFS->rcache,
                                   sector, off, size);
        if (err)
        {
            return err;
        }
        uint8_t *data = eHNFFS->rcache->buffer;

        eHNFFS_head_t head;
        while (true)
        {
            head = *(eHNFFS_head_t *)data;

            // Check if the head is valid.
            err = eHNFFS_dhead_check(head, eHNFFS_NULL, (int)eHNFFS_NULL);
            if (err)
            {
                return err;
            }

            if (off + eHNFFS_dhead_dsize(head) > eHNFFS->rcache->off + size &&
                head != eHNFFS_NULL)
            {
                // We have read whole data in cache.
                break;
            }

            eHNFFS_size_t len = 0;
            switch (eHNFFS_dhead_type(head))
            {
            case eHNFFS_DATA_BFILE_INDEX:
            case eHNFFS_DATA_SFILE_DATA:
                len = eHNFFS_dhead_dsize(head);
                if (eHNFFS_dhead_id(head) == id)
                {
                    memcpy(buffer, data, eHNFFS_dhead_dsize(head));
                    *index_sector = sector;
                    *index_off = off;
                    return err;
                }
                break;

            case eHNFFS_DATA_DELETE:
                len = eHNFFS_dhead_dsize(head);
                break;

            default:
                err = eHNFFS_ERR_WRONGCAL;
                return err;
            }

            off += len;
            data += len;

            if (off - eHNFFS->rcache->off >= eHNFFS->cfg->cache_size)
            {
                break;
            }

            if (off >= eHNFFS->cfg->sector_size)
            {
                eHNFFS_dir_sector_flash_t shead;
                err = eHNFFS->cfg->read(eHNFFS->cfg, sector, 0, &shead,
                                        sizeof(eHNFFS_dir_sector_flash_t));
                eHNFFS_ASSERT(err <= 0);
                if (err)
                {
                    return err;
                }

                err = eHNFFS_shead_check(shead.head, eHNFFS_SECTOR_USING, eHNFFS_SECTOR_DIR);
                if (err)
                {
                    return err;
                }

                next = shead.pre_sector;
                if (next != eHNFFS_NULL)
                {
                    // Traverse the next sector.
                    sector = next;
                    off = list->off;
                    list = list->next;
                }
                break;
            }
        }

        if (next == eHNFFS_NULL && off >= eHNFFS->cfg->sector_size)
        {
            return eHNFFS_ERR_IO;
        }
    }
}

/**
 * Traverse dir to find all big file index data, set reletive sectors to old.
 * Now it's only used in dir delete function.
 */
int eHNFFS_dtraverse_bfile_delete(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t sector = dir->tail;
    eHNFFS_size_t off = dir->backward;
    eHNFFS_size_t next = eHNFFS_NULL;
    eHNFFS_list_ram_t *list = dir->backward_list;

    while (true)
    {
        // Read data of dir to cache first.
        eHNFFS_size_t size = eHNFFS_min(eHNFFS->cfg->cache_size,
                                        eHNFFS->cfg->sector_size - off);
        err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, eHNFFS->rcache,
                                   sector, off, size);
        if (err)
        {
            return err;
        }
        uint8_t *data = eHNFFS->rcache->buffer;

        // Each sector head of dir stores the position of next sector.
        if (off == 0)
        {
            eHNFFS_dir_sector_flash_t *shead = (eHNFFS_dir_sector_flash_t *)data;
            err = eHNFFS_shead_check(shead->head, eHNFFS_SECTOR_USING, eHNFFS_SECTOR_DIR);
            if (err)
            {
                return err;
            }
            next = shead->pre_sector;
            data += sizeof(eHNFFS_dir_sector_flash_t);
            off += sizeof(eHNFFS_dir_sector_flash_t);
        }

        eHNFFS_head_t head;
        while (true)
        {
            // Check if the head is valid.
            head = *(eHNFFS_head_t *)data;
            err = eHNFFS_dhead_check(head, eHNFFS_NULL, (int)eHNFFS_NULL);
            if (err)
            {
                return err;
            }

            // We should read whole data in cache.
            if (off + eHNFFS_dhead_dsize(head) > eHNFFS->rcache->off + size)
            {
                break;
            }

            eHNFFS_size_t len = 0;
            eHNFFS_bfile_index_flash_t *bfile_index;
            switch (eHNFFS_dhead_type(head))
            {
            case eHNFFS_DATA_BFILE_INDEX:
                // When data type is big file index, set relative sectors to old.
                len = eHNFFS_dhead_dsize(head);
                bfile_index = (eHNFFS_bfile_index_flash_t *)data;
                err = eHNFFS_bfile_sector_old(eHNFFS, bfile_index->index,
                                              (len - sizeof(eHNFFS_head_t)) /
                                                  sizeof(eHNFFS_bfile_index_ram_t));
                if (err)
                {
                    return err;
                }
                break;

            case eHNFFS_DATA_SFILE_DATA:
            case eHNFFS_DATA_DELETE:
                // If it's other type, just skip.
                len = eHNFFS_dhead_dsize(head);
                break;

            default:
                // If it's type we are not supposed to get, it's error.
                err = eHNFFS_ERR_WRONGCAL;
                return err;
            }

            off += len;
            data += len;

            if (off - eHNFFS->rcache->off >= eHNFFS->cfg->cache_size)
            {
                break;
            }

            // Traverse the next sector if dir has.
            if (off >= eHNFFS->cfg->sector_size &&
                next != eHNFFS_NULL)
            {
                sector = next;
                off = list->off;
                list = list->next;
                break;
            }
        }

        // It's over.
        if (next == eHNFFS_NULL)
        {
            return err;
        }
    }
}

/**
 * Traverse dir to find out the number of old space that could be gc.
 */
int eHNFFS_dtraverse_ospace(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_size_t sector)
{
    int err = eHNFFS_ERR_OK;

    dir->tail = sector;
    eHNFFS_size_t ospace = 0;
    eHNFFS_size_t next = eHNFFS_NULL;
    eHNFFS_size_t off = 0;
    while (true)
    {
        // Read data of dir to cache first.
        eHNFFS_size_t size = eHNFFS_min(eHNFFS->cfg->cache_size,
                                        eHNFFS->cfg->sector_size - off);

        err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, eHNFFS->rcache,
                                   sector, off, size);
        if (err)
        {
            return err;
        }
        uint8_t *data = eHNFFS->rcache->buffer;

        // Each sector head of dir stores the position of next sector.
        if (off == 0)
        {
            eHNFFS_dir_sector_flash_t *shead = (eHNFFS_dir_sector_flash_t *)data;
            err = eHNFFS_shead_check(shead->head, eHNFFS_SECTOR_USING, eHNFFS_SECTOR_DIR);
            if (err)
            {
                return err;
            }
            next = shead->pre_sector;
            data += sizeof(eHNFFS_dir_sector_flash_t);
            off += sizeof(eHNFFS_dir_sector_flash_t);
        }

        eHNFFS_head_t head;
        while (true)
        {
            head = *(eHNFFS_head_t *)data;
            if (off + eHNFFS_dhead_dsize(head) > eHNFFS->rcache->off + size)
            {
                // We have read whole data in cache.
                break;
            }

            // Check if the head is valid.
            err = eHNFFS_dhead_check(head, eHNFFS_NULL, (int)eHNFFS_NULL);
            if (err)
            {
                return err;
            }

            eHNFFS_size_t len = 0;
            switch (eHNFFS_dhead_type(head))
            {
            // Execute different operations according to the type of data.
            case eHNFFS_DATA_DELETE:
                // Space of delete data is what we could gc, add to ospace.
                ospace += eHNFFS_dhead_dsize(head);
                len = eHNFFS_dhead_dsize(head);
                break;

            case eHNFFS_DATA_SKIP:
            {
                // Skip to backward.
                eHNFFS_skip_flash_t *skip = (eHNFFS_skip_flash_t *)data;
                len = skip->len;

                if (sector == dir->tail)
                {
                    dir->forward = off + sizeof(eHNFFS_skip_flash_t);
                    dir->backward = off + len;
                    dir->rest_space = dir->backward - dir->forward;

                    // If there is spare space, we could reuse it, so delete the skip data.
                    // But if not, we should find another sector before using.
                    if (dir->rest_space >= 2 * sizeof(eHNFFS_skip_flash_t))
                    {
                        err = eHNFFS_data_delete(eHNFFS, eHNFFS_ID_SUPER, sector, off,
                                                 sizeof(eHNFFS_skip_flash_t));
                        ospace += sizeof(eHNFFS_skip_flash_t);
                        skip->head &= eHNFFS_DELETE_TYPE_SET;
                        if (err)
                        {
                            return err;
                        }
                    }
                }
                else
                {
                    // It's backward list skip(i.e not tail sector).
                    err = eHNFFS_list_insert(dir, off + len);
                    if (err)
                    {
                        return err;
                    }
                }
                break;
            }

            case eHNFFS_DATA_DIR_NAME:
            case eHNFFS_DATA_FILE_NAME:
            case eHNFFS_DATA_BFILE_INDEX:
            case eHNFFS_DATA_SFILE_DATA:
                // Ignore.
                len = eHNFFS_dhead_dsize(head);
                break;

            case eHNFFS_DATA_FREE:
                // Tail doesn't have skip data, should use backward.
                if (head == eHNFFS_NULL &&
                    sector == dir->tail)
                    off = dir->backward;
                else
                    return eHNFFS_ERR_WRONGCAL;

            default:
                err = eHNFFS_ERR_WRONGCAL;
                return err;
            }

            off += len;
            data += len;

            if (off - eHNFFS->rcache->off >= eHNFFS->cfg->cache_size)
            {
                break;
            }
            if (off >= eHNFFS->cfg->sector_size)
            {
                // Traverse the next sector.
                sector = next;
                off = 0;
                break;
            }
        }

        if (next == eHNFFS_NULL)
        {
            // Return the space that could reuse by gc.
            dir->old_space = ospace;
            return err;
        }
    }
}

/**
 * The basic prog function of dir during gc.
 */
int eHNFFS_dir_prog(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, void *buffer, eHNFFS_size_t len, int mode)
{
    int err = eHNFFS_ERR_OK;

    // If current sectors space is not enough, we should find a new one.
    if (dir->rest_space < len + sizeof(eHNFFS_skip_flash_t))
    {
        // Prog skip data to end use the sector.
        eHNFFS_skip_flash_t skip = {
            .head = eHNFFS_MKDHEAD(0, 1, dir->id, eHNFFS_DATA_SKIP, sizeof(eHNFFS_skip_flash_t)),
            .len = dir->rest_space,
        };

        // prog skip data to flash.
        err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, eHNFFS->pcache, eHNFFS->rcache,
                                eHNFFS_VALIDATE, dir->tail, dir->forward, &skip,
                                sizeof(eHNFFS_skip_flash_t));
        if (err)
        {
            return err;
        }

        if (dir->old_space >= eHNFFS->cfg->sector_size)
        {
            // Region and map_buffer reference are no need in this situation.
            err = eHNFFS_dir_gc(eHNFFS, dir, eHNFFS_NULL, NULL);
            if (err)
            {
                return err;
            }
        }
        else
        {
            // Find a new sector and tell it dir->tail is its last sector.
            err = eHNFFS_sector_alloc(eHNFFS, eHNFFS->manager, eHNFFS_SECTOR_DIR, 1, dir->tail,
                                      dir->id, eHNFFS_NULL, &dir->tail, NULL);
            if (err)
            {
                return err;
            }

            // Update in-ram dir structure.
            err = eHNFFS_list_insert(dir, dir->backward);
            if (err)
            {
                return err;
            }
            dir->forward = sizeof(eHNFFS_dir_sector_flash_t);
            dir->backward = eHNFFS->cfg->sector_size;
            dir->rest_space = dir->backward - dir->forward;

            // Update basic message when we change the dir's tail,
            // including tree entry and name in father dir.
            err = eHNFFS_dir_update(eHNFFS, dir);
            if (err)
            {
                return err;
            }
        }
    }

    // Prog data to flash.
    eHNFFS_off_t off = (mode == eHNFFS_FORWARD) ? dir->forward : dir->backward;
    err = eHNFFS_cache_prog(eHNFFS, mode, eHNFFS->pcache, eHNFFS->rcache,
                            eHNFFS_VALIDATE, dir->tail, off, buffer, len);
    if (err)
    {
        return err;
    }

    // Update.
    if (mode == eHNFFS_FORWARD)
    {
        dir->forward += len;
        dir->rest_space -= len;
    }
    else
    {
        dir->backward -= len;
        dir->rest_space -= len;
    }

    return err;
}

/**
 * The traverse function of dir during gc.
 * When traversing, we should prog valid name and data to new sector.
 */
int eHNFFS_dtraverse_gc(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_size_t region,
                        uint32_t *map_buffer)
{
    int err = eHNFFS_ERR_OK;

    // Old sector message.
    eHNFFS_size_t old_sector = dir->tail;
    if (old_sector / eHNFFS->manager->region_size == region &&
        map_buffer != NULL)
    {
        int i = old_sector % eHNFFS->manager->region_size / eHNFFS_SIZE_T_NUM;
        int j = old_sector % eHNFFS->manager->region_size % eHNFFS_SIZE_T_NUM;
        map_buffer[i] |= (1U << j);
    }

    eHNFFS_size_t next = eHNFFS_NULL;
    eHNFFS_off_t old_off = 0;
    eHNFFS_size_t backward = dir->backward;
    bool first_flag = true;

    // New sector message.
    err = eHNFFS_sector_alloc(eHNFFS, eHNFFS->manager, eHNFFS_SECTOR_DIR, 1,
                              eHNFFS_NULL, dir->id, eHNFFS_NULL, &dir->tail, NULL);
    if (err)
    {
        return err;
    }
    dir->forward = sizeof(eHNFFS_dir_sector_flash_t);
    dir->backward = eHNFFS->cfg->sector_size;
    dir->rest_space = dir->backward - dir->forward;
    eHNFFS_list_delete(dir->backward_list); // The list need a new one.
    dir->backward_list = NULL;
    dir->old_space = 0;

    eHNFFS_cache_ram_t my_cache;
    my_cache.buffer = eHNFFS_malloc(eHNFFS->cfg->cache_size);
    if (my_cache.buffer == NULL)
        return eHNFFS_ERR_NOMEM;

    while (true)
    {
        // Read data of old sector to cache first.
        eHNFFS_size_t size = eHNFFS_min(eHNFFS->cfg->cache_size,
                                        eHNFFS->cfg->sector_size - old_off);
        // err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, eHNFFS->rcache,
        //                            old_sector, old_off, size);
        err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, &my_cache,
                                   old_sector, old_off, size);
        if (err)
        {
            goto cleanup;
        }
        // uint8_t *data = eHNFFS->rcache->buffer;
        uint8_t *data = my_cache.buffer;

        // Each sector head of dir stores the position of next sector.
        if (old_off == 0)
        {
            eHNFFS_dir_sector_flash_t *shead = (eHNFFS_dir_sector_flash_t *)data;
            err = eHNFFS_shead_check(shead->head, eHNFFS_SECTOR_USING, eHNFFS_SECTOR_DIR);
            if (err)
            {
                goto cleanup;
            }
            next = shead->pre_sector;
            data += sizeof(eHNFFS_dir_sector_flash_t);
            old_off += sizeof(eHNFFS_dir_sector_flash_t);
        }

        eHNFFS_head_t head;
        while (true)
        {
            head = *(eHNFFS_head_t *)data;

            // Check if the head is valid.
            err = eHNFFS_dhead_check(head, eHNFFS_NULL, (int)eHNFFS_NULL);
            if (err)
            {
                goto cleanup;
            }

            // if (old_off + eHNFFS_dhead_dsize(head) > eHNFFS->rcache->off + size)
            if (old_off + eHNFFS_dhead_dsize(head) > my_cache.off + size)
            {
                // We should read whole data in cache.
                // If not, try reread.
                break;
            }

            eHNFFS_size_t len = 0;
            switch (eHNFFS_dhead_type(head))
            {
            case eHNFFS_DATA_DELETE:
                // Ignore delete data.
                len = eHNFFS_dhead_dsize(head);
                break;

            case eHNFFS_DATA_SKIP:
            {
                // Can't be tail sector, turn to backward data.
                eHNFFS_skip_flash_t *skip = (eHNFFS_skip_flash_t *)data;
                len = skip->len;
                break;
            }

            case eHNFFS_DATA_DIR_NAME:
            case eHNFFS_DATA_FILE_NAME:
                // Move to new sector.
                len = eHNFFS_dhead_dsize(head);
                err = eHNFFS_dir_prog(eHNFFS, dir, data, len, eHNFFS_FORWARD);
                if (err)
                {
                    goto cleanup;
                }

                // For son dir, we should update their tree entry message.
                if (eHNFFS_dhead_type(head) == eHNFFS_DATA_DIR_NAME)
                {
                    eHNFFS_tree_entry_ram_t *entry = NULL;
                    err = eHNFFS_entry_idfind(eHNFFS, eHNFFS_dhead_id(head), &entry);
                    if (err)
                    {
                        goto cleanup;
                    }

                    entry->sector = dir->tail;
                    entry->off = dir->forward - len;
                    entry->if_change = true;
                }
                break;

            case eHNFFS_DATA_BFILE_INDEX:
            case eHNFFS_DATA_SFILE_DATA:
                // Move to new sector.
                len = eHNFFS_dhead_dsize(head);
                err = eHNFFS_dir_prog(eHNFFS, dir, data, len, eHNFFS_BACKWARD);
                if (err)
                {
                    goto cleanup;
                }
                break;

            case eHNFFS_DATA_FREE:
                // It's for the first sector, it may doesn't have skip data.
                if (head == eHNFFS_NULL && first_flag)
                {
                    first_flag = false;
                    old_off = backward;
                }
                else
                {
                    err = eHNFFS_ERR_WRONGCAL;
                    goto cleanup;
                }

            default:
                err = eHNFFS_ERR_WRONGCAL;
                goto cleanup;
            }

            old_off += len;
            data += len;

            if (old_off >= eHNFFS->cfg->sector_size &&
                next != eHNFFS_NULL)
            {
                // Traverse the next sector.
                old_sector = next;
                old_off = 0;
                if (old_sector / eHNFFS->manager->region_size == region &&
                    map_buffer != NULL)
                {
                    int i = old_sector % eHNFFS->manager->region_size / eHNFFS_SIZE_T_NUM;
                    int j = old_sector % eHNFFS->manager->region_size % eHNFFS_SIZE_T_NUM;
                    map_buffer[i] |= (1U << j);
                }
                break;
            }

            // if (old_off - eHNFFS->rcache->off >= eHNFFS->cfg->cache_size)
            if (old_off - my_cache.off >= eHNFFS->cfg->cache_size)
            {
                break;
            }
        }

        // Over.
        if (next == eHNFFS_NULL)
        {
            break;
        }
    }

cleanup:
    if (my_cache.buffer != NULL)
        eHNFFS_free(my_cache.buffer);
    return err;
}

/**
 * Set all sectors belong to old dir to old(can be gc)
 */
int eHNFFS_dir_old(eHNFFS_t *eHNFFS, eHNFFS_size_t tail)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_dir_sector_flash_t dsector;
    while (tail != eHNFFS_NULL)
    {
        // Mainly read pre_sector.
        err = eHNFFS->cfg->read(eHNFFS->cfg, tail, 0, &dsector,
                                sizeof(eHNFFS_dir_sector_flash_t));
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            return err;
        }

        // Turn state of sector head to old.
        dsector.head &= eHNFFS_OLD_SECTOR_SET;
        err = eHNFFS->cfg->prog(eHNFFS->cfg, tail, 0, &dsector,
                                sizeof(eHNFFS_head_t));
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            return err;
        }

        // Turn erase map to reuse.
        err = eHNFFS_emap_set(eHNFFS, eHNFFS->manager, tail, 1);
        if (err)
        {
            return err;
        }

        tail = dsector.pre_sector;
    }

    return err;
}

/**
 * Update dir entry from its father dir.
 */
int eHNFFS_dir_update(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_dir_name_flash_t *dir_name = NULL;
    eHNFFS_dir_ram_t *father_dir = NULL;
    eHNFFS_size_t len;

    // Find dir's corresponding entry.
    eHNFFS_tree_entry_ram_t *entry = NULL;
    err = eHNFFS_entry_idfind(eHNFFS, dir->id, &entry);
    if (err)
    {
        return err;
    }

    // Update basic entry message.
    entry->tail = dir->tail;
    entry->if_change = true;

    // If dir is root dir, It doesn't have father dir, so we don't need to update.
    if (dir->father_id == eHNFFS_ID_SUPER)
    {
        goto cleanup;
    }

    // Find dir's father dir.
    err = eHNFFS_father_id_find(eHNFFS, dir->father_id, eHNFFS_MIGRATE, &father_dir);
    if (err)
    {
        printf("no father id\n");
        return err;
    }

    // Read origin data to read cache.
    len = sizeof(eHNFFS_dir_name_flash_t) + eHNFFS_NAME_MAX;
    err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, eHNFFS->rcache,
                               dir->sector, dir->off, len);
    if (err)
    {
        return err;
    }

    // Change its tail.
    dir_name = (eHNFFS_dir_name_flash_t *)eHNFFS->rcache->buffer;
    dir_name->tail = dir->tail;
    len = eHNFFS_dhead_dsize(dir_name->head);
    dir_name = eHNFFS_malloc(len);
    if (dir_name == NULL)
    {
        return eHNFFS_ERR_NOMEM;
    }
    memcpy(dir_name, eHNFFS->rcache->buffer, len);

    // Set origin data to delete.
    err = eHNFFS_data_delete(eHNFFS, dir->id, dir->sector, dir->off, len);
    if (err)
    {
        goto cleanup;
    }

    // It's no use ！！
    // dir_name->head &= eHNFFS_DELETE_TYPE_SET;

    // Prog new data.
    err = eHNFFS_dir_prog(eHNFFS, father_dir, dir_name, len, eHNFFS_FORWARD);
    if (err)
    {
        goto cleanup;
    }

    // Update address.
    // Name len don't need to change.
    dir->sector = father_dir->tail;
    dir->off = father_dir->forward - len;

cleanup:
    eHNFFS_free(dir_name);
    return err;
}

/**
 * Open function for dir.
 */
int eHNFFS_dir_lowopen(eHNFFS_t *eHNFFS, eHNFFS_size_t tail, eHNFFS_size_t id,
                       eHNFFS_size_t father_id, eHNFFS_size_t sector, eHNFFS_size_t off,
                       eHNFFS_size_t namelen, int type, eHNFFS_dir_ram_t **dir_addr)
{
    int err = eHNFFS_ERR_OK;

    // Try to find dir in dir list, if find, just open it.
    eHNFFS_dir_list_ram_t *list = eHNFFS->dir_list;
    eHNFFS_dir_ram_t *dir = list->dir;
    while (dir != NULL)
    {
        if (dir->id == id)
        {
            *dir_addr = dir;
            return err;
        }
        dir = dir->next_dir;
    }

    // If not find, allocate memory for the dir.
    dir = eHNFFS_malloc(sizeof(eHNFFS_dir_ram_t));
    if (!dir)
    {
        return eHNFFS_ERR_NOSPC;
    }

    // Initialize basic data for the dir.
    dir->type = type;
    dir->id = id;
    dir->father_id = father_id;
    dir->sector = sector;
    dir->off = off;
    dir->namelen = namelen;
    dir->pos_sector = eHNFFS_NULL;
    dir->pos_off = eHNFFS_NULL;
    dir->pos_presector = eHNFFS_NULL;

    dir->backward_list = NULL;

    // Traverse the dir to update some basic message, like old space size.
    err = eHNFFS_dtraverse_ospace(eHNFFS, dir, tail);
    if (err)
    {
        goto cleanup;
    }

    // Add dir to dir list.
    if (list->dir != NULL)
        dir->next_dir = list->dir;
    list->dir = dir;
    list->count++;

    *dir_addr = dir;

    return err;

cleanup:
    eHNFFS_free(dir);
    return err;
}

/**
 * Dir close function.
 */
int eHNFFS_dir_lowclose(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir)
{
    int err = eHNFFS_ERR_OK;

    // Make skip data, it could tell us the skip length between forward and backward in tail
    // sector during next open.
    eHNFFS_skip_flash_t skip = {
        .head = eHNFFS_MKDHEAD(0, 1, dir->id, eHNFFS_DATA_SKIP, sizeof(eHNFFS_skip_flash_t)),
        .len = dir->rest_space,
    };

    // Prog skip data to flash.
    // Notice that every prog we should make sure rest space is larger that skip data size, so
    // now we don't need to judge it.
    /* err = eHNFFS_direct_data_prog(eHNFFS, dir->tail, &dir->forward, sizeof(eHNFFS_skip_flash_t),
                                  &skip);
    if (err)
    {
        return err;
    } */
    err = eHNFFS_dir_prog(eHNFFS, dir, &skip, sizeof(eHNFFS_skip_flash_t), eHNFFS_FORWARD);
    if (err)
    {
        return err;
    }

    err = eHNFFS_cache_flush(eHNFFS, eHNFFS->pcache, eHNFFS->rcache, eHNFFS_VALIDATE);
    if (err)
    {
        return err;
    }

    if (dir->tail == eHNFFS->rcache->sector)
    {
        eHNFFS_cache_one(eHNFFS, eHNFFS->rcache);
    }

    // Find the entry belongs to dir.
    eHNFFS_tree_entry_ram_t *entry = NULL;
    err = eHNFFS_entry_idfind(eHNFFS, dir->id, &entry);
    if (err)
    {
        return err;
    }

    // If change flag is true, then prog entry to flash.
    if (entry->if_change)
    {
        err = eHNFFS_entry_prog(eHNFFS, entry, dir->father_id);
        if (err)
        {
            return err;
        }
    }

    eHNFFS_list_delete(dir->backward_list);

    // Delete it in dir list.
    eHNFFS_dir_ram_t *pre_dir = eHNFFS->dir_list->dir;
    if (pre_dir->id == dir->id)
        eHNFFS->dir_list->dir = pre_dir->next_dir;
    else
    {
        while (pre_dir->next_dir != NULL)
        {
            if (pre_dir->next_dir->id == dir->id)
                break;
            pre_dir = pre_dir->next_dir;
        }

        if (pre_dir == NULL)
            return eHNFFS_ERR_NODIROPEN;
        pre_dir->next_dir = dir->next_dir;
    }
    eHNFFS->dir_list->count--;

    eHNFFS_free(dir);
    return err;
}

int eHNFFS_dir_migration(eHNFFS_t *eHNFFS, eHNFFS_size_t sector, eHNFFS_size_t region,
                         uint32_t *map_buffer)
{
    int err = eHNFFS_ERR_OK;

    // Read basic data in sector to know what id it belonngs to.
    eHNFFS_dir_sector_flash_t dir_sector;
    err = eHNFFS->cfg->read(eHNFFS->cfg, sector, 0, &dir_sector,
                            sizeof(eHNFFS_dir_sector_flash_t));
    eHNFFS_ASSERT(err <= 0);
    if (err)
    {
        return err;
    }

    eHNFFS_dir_ram_t *dir = NULL;
    err = eHNFFS_father_id_find(eHNFFS, dir_sector.id, eHNFFS_MIGRATE, &dir);
    if (err)
    {
        return err;
    }

    err = eHNFFS_dir_gc(eHNFFS, dir, region, map_buffer);
    if (err)
    {
        return err;
    }

    return err;
}

int eHNFFS_dir_region_migration(eHNFFS_t *eHNFFS, eHNFFS_size_t region, uint32_t *map_buffer)
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
            if (((map_buffer[i] >> j) & 1U) == 0)
            {
                eHNFFS_size_t sector = region * eHNFFS->manager->region_size +
                                       i * eHNFFS_SIZE_T_NUM + j;
                err = eHNFFS_dir_migration(eHNFFS, sector, region, map_buffer);
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
 * Find in-ram father dir of file.
 */
int eHNFFS_fdir_file_find(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file, eHNFFS_dir_ram_t **dir_addr)
{
    eHNFFS_dir_ram_t *dir = eHNFFS->dir_list->dir;
    while (dir != NULL && dir->id != file->father_id)
    {
        dir = dir->next_dir;
    }

    if (dir == NULL)
        return eHNFFS_ERR_NODIROPEN;

    *dir_addr = dir;
    return eHNFFS_ERR_OK;
}

/**
 * Find in-ram father dir of dir.
 */
int eHNFFS_fdir_dir_find(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_dir_ram_t **dir_addr)
{
    eHNFFS_dir_ram_t *fdir = eHNFFS->dir_list->dir;
    while (fdir != NULL && fdir->id != dir->father_id)
    {
        fdir = fdir->next_dir;
    }

    if (fdir == NULL)
        return eHNFFS_ERR_NODIROPEN;

    *dir_addr = fdir;
    return eHNFFS_ERR_OK;
}

/**
 * Create a new dir.
 */
int eHNFFS_create_dir(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *father_dir, eHNFFS_dir_ram_t **dir_addr,
                      char *name, eHNFFS_size_t namelen)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t size;

    // Create in-ram dir structure.
    eHNFFS_dir_name_flash_t *dir_name;
    eHNFFS_dir_ram_t *dir = *dir_addr;
    dir = eHNFFS_malloc(sizeof(eHNFFS_dir_ram_t));
    if (dir == NULL)
    {
        return eHNFFS_ERR_NOMEM;
    }

    // problem
    // Allocate id for new dir.
    err = eHNFFS_id_alloc(eHNFFS, &dir->id);
    if (err)
    {
        goto cleanup;
    }

    // Allocate a sector for new dir.
    err = eHNFFS_sector_alloc(eHNFFS, eHNFFS->manager, eHNFFS_SECTOR_DIR, 1, eHNFFS_NULL,
                              dir->id, father_dir->id, &dir->tail, NULL);
    if (err)
    {
        goto cleanup;
    }

    // Allocate memory for in-flash dir name structure.
    size = sizeof(eHNFFS_dir_name_flash_t) + namelen;
    dir_name = eHNFFS_malloc(size);
    if (dir_name == NULL)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    // Initialize data to dir name structure.
    dir_name->head = eHNFFS_MKDHEAD(0, 1, dir->id, eHNFFS_DATA_DIR_NAME, size);
    dir_name->tail = dir->tail;
    memcpy(dir_name->name, name, namelen);

    // Prog it to father dir.
    err = eHNFFS_dir_prog(eHNFFS, father_dir, dir_name, size, eHNFFS_FORWARD);
    if (err)
    {
        goto cleanup;
    }

    // Update in-ram dir structure.
    dir->sector = father_dir->tail;
    dir->off = father_dir->forward - size;
    dir->namelen = namelen;
    dir->pos_sector = eHNFFS_NULL;
    dir->pos_off = eHNFFS_NULL;
    dir->pos_presector = eHNFFS_NULL;

    dir->father_id = father_dir->id;
    dir->old_space = 0;
    dir->type = eHNFFS_DATA_DIR;

    dir->forward = sizeof(eHNFFS_dir_sector_flash_t);
    dir->backward = eHNFFS->cfg->sector_size;
    dir->rest_space = dir->backward - dir->forward;

    // Add dir to dir list.
    dir->backward_list = NULL;
    dir->next_dir = eHNFFS->dir_list->dir;
    eHNFFS->dir_list->dir = dir;
    eHNFFS->dir_list->count++;

    // Find dir's father dir entry.
    eHNFFS_tree_entry_ram_t *father_entry;
    err = eHNFFS_entry_idfind(eHNFFS, father_dir->id, &father_entry);
    if (err)
    {
        goto cleanup;
    }

    // Create new in-ram dir entry.
    err = eHNFFS_entry_add(eHNFFS, father_entry, dir->id, dir->sector, dir->off,
                           dir->tail, name, namelen, true);
    if (err)
    {
        goto cleanup;
    }

    // Find the position of dir entry.
    int i;
    for (i = father_entry->num_of_son - 1; i >= 0; i--)
    {
        if (father_entry->son_list[i].id == dir->id)
            break;
    }

    // If not find, then there is some error happening.
    if (i < 0)
    {
        err = eHNFFS_ERR_NOENT;
        goto cleanup;
    }

    // problem
    // Prog the new entry to flash.
    err = eHNFFS_entry_prog(eHNFFS, &father_entry->son_list[i], father_dir->id);
    if (err)
    {
        goto cleanup;
    }

    // Return the new in-ram dir structure.
    *dir_addr = dir;
    eHNFFS_free(dir_name);
    return err;

cleanup:
    if (dir)
        eHNFFS_free(dir);
    if (dir_name)
        eHNFFS_free(dir_name);
    return err;
}
