/**
 * The middle level operations of eHNFFS
 */
#include "eHNFFS_middle.h"
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
 * --------------------------------------    FS level operations    --------------------------------------
 * -------------------------------------------------------------------------------------------------------
 */

/**
 * Format basic metadata of nor flash when first mount.
 */
int eHNFFS_rawformat(eHNFFS_t *eHNFFS, const struct eHNFFS_config *cfg)
{
    int err = eHNFFS_ERR_OK;
    int err2 = eHNFFS_ERR_OK;
    eHNFFS_mapaddr_flash_t *map = NULL;
    eHNFFS_region_map_flash_t *flash_region_map = NULL;
    eHNFFS_tree_entry_flash_t *root_entry = NULL;

    eHNFFS_size_t new_sector;
    eHNFFS_superblock_ram_t *super;
    eHNFFS_head_t head;
    eHNFFS_size_t size;
    eHNFFS_supermessage_flash_t super_message;
    eHNFFS_size_t cost;
    eHNFFS_treeaddr_flash_t tree_addr;
    eHNFFS_size_t shead;
    eHNFFS_region_map_ram_t *ram_region_map;
    eHNFFS_dir_list_ram_t *dir_list;
    eHNFFS_commit_flash_t commit;
    eHNFFS_magic_flash_t magic;
    eHNFFS_map_ram_t *smap;

    // Allocate ram memory for all structure we need.
    err = eHNFFS_init(eHNFFS, cfg);
    if (err)
    {
        goto cleanup;
    }

    // Initialize super block message.
    new_sector = 0;
    super = eHNFFS->superblock;
    super->sector = 0;
    super->free_off = 0;

    // Prog sector head of super block.
    head = eHNFFS_MKSHEAD(0, eHNFFS_SECTOR_USING,
                          eHNFFS_SECTOR_SUPER, 0, 0);
    err = eHNFFS_direct_shead_prog(eHNFFS, super->sector, &super->free_off,
                                   sizeof(eHNFFS_head_t), &head);
    if (err)
    {
        goto cleanup;
    }

    // In-flash super message of superblock
    size = sizeof(eHNFFS_supermessage_flash_t);
    super_message.head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_SUPER_MESSAGE, size);
    memcpy(super_message.fs_name, &"eHNFFS", strlen("eHNFFS"));
    super_message.version = eHNFFS_VERSION;
    super_message.sector_size = eHNFFS->cfg->sector_size;
    super_message.sector_count = eHNFFS->cfg->sector_count;
    super_message.name_max = eHNFFS_min(eHNFFS->cfg->name_max, eHNFFS_NAME_MAX);
    super_message.file_max = eHNFFS_min(eHNFFS->cfg->file_max, eHNFFS_FILE_MAX_SIZE);
    super_message.region_cnt = eHNFFS->cfg->region_cnt;
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, eHNFFS->pcache, NULL,
                            eHNFFS_VALIDATE, super->sector, super->free_off,
                            &super_message, size);
    if (err)
    {
        goto cleanup;
    }
    super->free_off += size;
    new_sector += 2;

    // In-flash id map address message.
    cost = eHNFFS_alignup(eHNFFS_ID_MAX * 2 / 8, eHNFFS->cfg->sector_size) /
           eHNFFS->cfg->sector_size;
    size = sizeof(eHNFFS_mapaddr_flash_t) + cost * sizeof(eHNFFS_size_t);
    map = eHNFFS_malloc(size);
    if (!map)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }
    map->head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_ID_MAP, size);
    map->begin = new_sector;
    map->off = 0;
    for (int i = 0; i < cost; i++)
        map->erase_times[i] = 0;
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, eHNFFS->pcache, NULL,
                            eHNFFS_VALIDATE, super->sector, super->free_off,
                            map, size);
    if (err)
    {
        goto cleanup;
    }
    super->free_off += size;
    new_sector += cost;

    // Assign in-ram idmap's basic message.
    err = eHNFFS_idmap_assign(eHNFFS, eHNFFS->id_map, map, cost);
    if (err)
    {
        goto cleanup;
    }
    eHNFFS->id_map->free_map->free_num = eHNFFS->id_map->bits_in_buffer;
    eHNFFS->id_map->free_map->index = 0;
    eHNFFS->id_map->free_map->region = 0;
    eHNFFS->id_map->remove_map->index = 0;
    eHNFFS_free(map);
    map = NULL;

    // In-flash sector map address message.
    cost = eHNFFS_alignup(eHNFFS->cfg->sector_count * 2 / 8, eHNFFS->cfg->sector_size) /
           eHNFFS->cfg->sector_size;
    size = sizeof(eHNFFS_mapaddr_flash_t) + cost * sizeof(eHNFFS_size_t);
    map = eHNFFS_malloc(size);
    if (!map)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }
    map->head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_SECTOR_MAP, size);
    map->begin = new_sector;
    map->off = 0;
    for (int i = 0; i < cost; i++)
        map->erase_times[i] = 0;
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, eHNFFS->pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, map, size);
    if (err)
    {
        goto cleanup;
    }
    super->free_off += size;
    new_sector += cost;

    // Assign in ram sector map's basic message.
    err = eHNFFS_sectormap_assign(eHNFFS, eHNFFS->manager, map, cost);
    if (err)
    {
        goto cleanup;
    }
    eHNFFS_free(map);
    map = NULL;

    // In-flash hash tree address message.
    size = sizeof(eHNFFS_treeaddr_flash_t);
    tree_addr.head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_TREE_ADDR, size);
    tree_addr.begin = new_sector;
    shead = eHNFFS_MKSHEAD(0, eHNFFS_SECTOR_USING, eHNFFS_SECTOR_TREE, 0, 0);

    for (int i = 0; i < eHNFFS_TREE_SECTOR_NUM; i++)
    {
        // Prog basic sector head of hash tree sectors.
        eHNFFS_off_t off = 0;
        err = eHNFFS_direct_shead_prog(eHNFFS, tree_addr.begin, &off,
                                       sizeof(eHNFFS_head_t), &shead);
        if (err)
        {
            goto cleanup;
        }
    }
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, eHNFFS->pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, &tree_addr, size);
    if (err)
    {
        goto cleanup;
    }
    super->free_off += size;
    new_sector += eHNFFS_TREE_SECTOR_NUM;

    // In-ram hash tree message.
    eHNFFS->hash_tree->begin = tree_addr.begin;
    eHNFFS->hash_tree->free_off = sizeof(eHNFFS_size_t);
    eHNFFS->hash_tree->num = 0;

    // In-flash region map.
    size = sizeof(eHNFFS_region_map_flash_t) + eHNFFS->cfg->region_cnt * 3 / 8;
    flash_region_map = eHNFFS_malloc(size);
    ram_region_map = eHNFFS->manager->region_map;
    if (!flash_region_map)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }
    flash_region_map->head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_REGION_MAP, size);
    memset(flash_region_map->map, eHNFFS_NULL, eHNFFS->cfg->region_cnt * 3 / 8);

    // Ram region map message.
    err = eHNFFS_regionmap_assign(eHNFFS, ram_region_map, flash_region_map, super->sector, super->free_off);
    if (err)
    {
        goto cleanup;
    }

    // Index in erase map is change flag, turn it to 0 first.
    eHNFFS->manager->erase_map->index = 0;

    // Select region 1 as the first dir region.
    ram_region_map->dir_index = 1;
    ram_region_map->dir_region_num = 1;
    ram_region_map->dir_region[0] &= ~(1U << 1);
    ram_region_map->free_region[0] &= !(1U << 1);
    ram_region_map->reserve = 1;
    smap = eHNFFS->manager->dir_map;
    err = eHNFFS_ram_map_change(eHNFFS, 1, eHNFFS->manager->region_size, smap);
    if (err)
    {
        goto cleanup;
    }

    // Select region 0 as the first big file region.
    ram_region_map->bfile_index = 0;
    ram_region_map->bfile_region_num = 1;
    ram_region_map->bfile_region[0] &= ~1U;
    ram_region_map->free_region[0] &= ~1U;
    ram_region_map->reserve++;
    smap = eHNFFS->manager->bfile_map;
    err = eHNFFS_ram_map_change(eHNFFS, 0, eHNFFS->manager->region_size, smap);
    if (err)
    {
        goto cleanup;
    }

    // Change other region map message.
    flash_region_map->map[0] &= ~1U;
    flash_region_map->map[0] &= ~(1U << 1);
    flash_region_map->map[eHNFFS->cfg->region_cnt / 8] &= ~(1U << 1);
    flash_region_map->map[2 * eHNFFS->cfg->region_cnt / 8] &= ~1U;
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, eHNFFS->pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, flash_region_map, size);
    if (err)
    {
        goto cleanup;
    }
    super->free_off += size;
    eHNFFS_free(flash_region_map);
    flash_region_map = NULL;

    // Change big file map message.
    for (int i = 0; i < new_sector; i++)
    {
        smap->buffer[0] &= ~(1U << i);
    }
    smap->free_num -= new_sector;
    smap->index += new_sector;

    // Change id map message.
    eHNFFS->id_map->free_map->buffer[0] &= ~1U;
    eHNFFS->id_map->free_map->buffer[0] &= ~(1U << 1);
    eHNFFS->id_map->free_map->free_num -= 2;
    eHNFFS->id_map->free_map->index = 2;

    // Create root dir.
    err = eHNFFS_sector_alloc(eHNFFS, eHNFFS->manager, eHNFFS_SECTOR_DIR, 1, eHNFFS_NULL,
                              eHNFFS_ID_ROOT, eHNFFS_ID_SUPER, &new_sector, NULL);
    if (err)
    {
        goto cleanup;
    }

    // Add root entry to hash tree.
    size = sizeof(eHNFFS_tree_entry_ram_t);
    eHNFFS->hash_tree->root_hash = eHNFFS_malloc(size);
    if (!eHNFFS->hash_tree->root_hash)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }
    eHNFFS->hash_tree->num++;
    eHNFFS->hash_tree->root_hash->id = eHNFFS_ID_ROOT;
    eHNFFS->hash_tree->root_hash->sector = eHNFFS_NULL;
    eHNFFS->hash_tree->root_hash->off = eHNFFS_NULL;
    eHNFFS->hash_tree->root_hash->tail = new_sector;
    eHNFFS->hash_tree->root_hash->num_of_son = 0;
    eHNFFS->hash_tree->root_hash->data_type = eHNFFS_DATA_TREE_NAME;
    eHNFFS->hash_tree->root_hash->if_change = false;
    eHNFFS->hash_tree->root_hash->son_list = NULL;
    memset(eHNFFS->hash_tree->root_hash->data.name, 0, 8);
    memcpy(eHNFFS->hash_tree->root_hash->data.name, eHNFFS->cfg->root_dir_name,
           strlen((const char *)eHNFFS->cfg->root_dir_name));

    // Prog root entry to flash.
    size = sizeof(eHNFFS_tree_entry_flash_t) + strlen((const char *)eHNFFS->cfg->root_dir_name);
    root_entry = eHNFFS_malloc(size);
    if (!root_entry)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }
    root_entry->head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_ROOT, eHNFFS_DATA_TREE_NAME, size);
    root_entry->father_id = eHNFFS_ID_SUPER;
    root_entry->sector = eHNFFS_NULL;
    root_entry->off = eHNFFS_NULL;
    root_entry->tail = new_sector;
    memcpy(root_entry->data, eHNFFS->cfg->root_dir_name, strlen((const char *)eHNFFS->cfg->root_dir_name));
    err = eHNFFS_direct_data_prog(eHNFFS, eHNFFS->hash_tree->begin, &eHNFFS->hash_tree->free_off,
                                  size, root_entry);
    if (err)
    {
        goto cleanup;
    }
    eHNFFS_free(root_entry);
    root_entry = NULL;

    // Add root dir to in-ram dir list.
    dir_list = eHNFFS->dir_list;
    dir_list->dir = eHNFFS_malloc(sizeof(eHNFFS_dir_ram_t));
    if (!dir_list->dir)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }
    dir_list->count++;
    dir_list->dir->id = eHNFFS_ID_ROOT;
    dir_list->dir->father_id = eHNFFS_ID_SUPER;
    dir_list->dir->old_space = 0;
    dir_list->dir->type = eHNFFS_SECTOR_DIR;
    dir_list->dir->sector = eHNFFS_NULL;
    dir_list->dir->namelen = strlen((const char *)eHNFFS->cfg->root_dir_name);
    dir_list->dir->off = eHNFFS_NULL;

    dir_list->dir->pos_sector = eHNFFS_NULL;
    dir_list->dir->pos_off = eHNFFS_NULL;
    dir_list->dir->pos_presector = eHNFFS_NULL;

    dir_list->dir->tail = new_sector;
    dir_list->dir->forward = sizeof(eHNFFS_dir_sector_flash_t);
    dir_list->dir->backward = eHNFFS->cfg->sector_size;
    dir_list->dir->rest_space = dir_list->dir->backward - dir_list->dir->forward;
    dir_list->dir->backward_list = NULL;
    dir_list->dir->next_dir = NULL;

    // In-flash commit message.
    size = sizeof(eHNFFS_commit_flash_t);
    commit.head = head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_COMMIT, size);
    commit.next_id = eHNFFS->id_map->free_map->index;
    commit.scan_times = 0;
    commit.next_dir_sector = eHNFFS->manager->dir_map->region *
                                 eHNFFS->manager->region_size +
                             eHNFFS->manager->dir_map->index;
    commit.next_bfile_sector = eHNFFS->manager->bfile_map->region *
                                   eHNFFS->manager->region_size +
                               eHNFFS->manager->bfile_map->index;
    commit.reserve = eHNFFS->manager->region_map->reserve;
    commit.wl_index = eHNFFS_NULL;
    commit.wl_bfile_index = eHNFFS_NULL;
    commit.wl_free_off = eHNFFS_NULL;
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, eHNFFS->pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, &commit, size);
    if (err)
    {
        goto cleanup;
    }
    super->commit_off = super->free_off;
    super->free_off += size;

    // In-flash magic message.
    size = sizeof(eHNFFS_magic_flash_t);
    magic.head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_MAGIC, size);
    magic.maigc = 0;
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, eHNFFS->pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, &magic, size);
    if (err)
    {
        goto cleanup;
    }
    super->free_off += size;

    // Flush all meesage to flash.
    err = eHNFFS_cache_flush(eHNFFS, eHNFFS->pcache, NULL, eHNFFS_VALIDATE);
    if (err)
    {
        goto cleanup;
    }

    return err;

cleanup:
    err2 = eHNFFS_deinit(eHNFFS);
    if (err2)
        return err2;
    if (map)
        eHNFFS_free(map);
    if (flash_region_map)
        eHNFFS_free(flash_region_map);
    if (root_entry)
        eHNFFS_free(root_entry);
    return err;
}

/**
 * Construct all in-ram structure when mounting.
 */
int eHNFFS_rawmount(eHNFFS_t *eHNFFS, const struct eHNFFS_config *cfg)
{
    int err = eHNFFS_ERR_OK;
    int err2 = eHNFFS_ERR_OK;
	
		uint8_t *data;

    // Initialize function, mainly allocate space for rcache and pcache.
    err = eHNFFS_init(eHNFFS, cfg);
    if (err)
    {
        return err;
    }

    eHNFFS_superblock_ram_t *super = eHNFFS->superblock;
    super->free_off = sizeof(eHNFFS_head_t);
    err = eHNFFS_select_supersector(eHNFFS, &super->sector);
    if (err)
    {
        goto cleanup;
    }

    // Read basic data in superblock to construct in ram metadata.
    eHNFFS_size_t size, len;
    eHNFFS_head_t head;
    data = eHNFFS->rcache->buffer;
    while (true)
    {

        // Read to rcache firstly.
        size = eHNFFS_min(eHNFFS->cfg->cache_size,
                          eHNFFS->cfg->sector_size - super->free_off);
        err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, eHNFFS->rcache,
                                   super->sector, super->free_off, size);
        if (err)
        {
            goto cleanup;
        }

        while (true)
        {
            head = *(eHNFFS_head_t *)data;
            len = eHNFFS_dhead_dsize(head);

            if (eHNFFS_dhead_novalid(head) || eHNFFS_dhead_nowritten(head))
            {
                // Check whether or not the data head is valid.
                err = eHNFFS_ERR_CORRUPT;
                goto cleanup;
            }
            else if (len + super->free_off >
                     eHNFFS->rcache->off + eHNFFS->rcache->size)
            {
                // Check whether or not rcache stores the whole data head and data behind.
                break;
            }
            else if (head == eHNFFS_NULL)
            {
                // If all bits in data head is 1, it means we haven't wroten a real magic
                // number during last unmount, so corrupt has happened.
                err = eHNFFS_ERR_CORRUPT;
                goto cleanup;
            }

            // Check the data head type.
            switch (eHNFFS_dhead_type(head))
            {
            case eHNFFS_DATA_SUPER_MESSAGE:
            {
                // SUPER MESSAGE, mainly used to checkout whether or not cfg message is
                // the same as recorded in nor flash.
                eHNFFS_supermessage_flash_t *message = (eHNFFS_supermessage_flash_t *)data;
                if (!memcpy(message->fs_name, &eHNFFS_FS_NAME, strlen(eHNFFS_FS_NAME)) ||
                    eHNFFS_VERSION != message->version ||
                    eHNFFS->cfg->sector_size != message->sector_size ||
                    eHNFFS->cfg->sector_count != message->sector_count ||
                    eHNFFS->cfg->name_max != message->name_max ||
                    eHNFFS->cfg->file_max != message->file_max ||
                    eHNFFS->cfg->region_cnt != message->region_cnt)
                {
                    err = eHNFFS_ERR_WRONGCFG;
                    goto cleanup;
                }
                break;
            }

            case eHNFFS_DATA_ID_MAP:
            {
                // ID map, only initialize begin sector and off.
                // The buffer is initialized behind.
                eHNFFS_mapaddr_flash_t *flash_map = (eHNFFS_mapaddr_flash_t *)data;
                eHNFFS_size_t num = eHNFFS_alignup(eHNFFS_ID_MAX * 2 / 8, eHNFFS->cfg->sector_size) /
                                    eHNFFS->cfg->sector_size;
                err = eHNFFS_idmap_assign(eHNFFS, eHNFFS->id_map, flash_map, num);
                if (err)
                {
                    goto cleanup;
                }
                break;
            }

            case eHNFFS_DATA_SECTOR_MAP:
            {
                // Sector map, similar to ID map.
                eHNFFS_mapaddr_flash_t *flash_map = (eHNFFS_mapaddr_flash_t *)data;
                eHNFFS_size_t num = eHNFFS_alignup(eHNFFS->cfg->sector_count * 2 / 8,
                                                   eHNFFS->cfg->sector_size) /
                                    eHNFFS->cfg->sector_size;
                err = eHNFFS_sectormap_assign(eHNFFS, eHNFFS->manager, flash_map, num);
                if (err)
                {
                    goto cleanup;
                }
                break;
            }

            case eHNFFS_DATA_WL_ADDR:
            {
                // WL module, when we find the type of data head, it means that we
                // need to do wear leveling.
                eHNFFS_wladdr_flash_t *wladdr = (eHNFFS_wladdr_flash_t *)data;
                err = eHNFFS_wl_malloc(eHNFFS, eHNFFS->manager->wl, wladdr);
                if (err)
                {
                    goto cleanup;
                }
                break;
            }

            case eHNFFS_DATA_TREE_ADDR:
            {

                // DIR hash used to execute name resolution.
                eHNFFS_treeaddr_flash_t *treeaddr = (eHNFFS_treeaddr_flash_t *)data;
                eHNFFS_hash_tree_ram_t *ram_tree = eHNFFS->hash_tree;
                ram_tree->begin = treeaddr->begin;
                ram_tree->free_off = 0;
                ram_tree->num = 0;
                ram_tree->root_hash = NULL;
                err = eHNFFS_construct_tree(eHNFFS, ram_tree, eHNFFS->pcache);
                if (err)
                {
                    goto cleanup;
                }

                break;
            }

            case eHNFFS_DATA_REGION_MAP:
            {
                eHNFFS_region_map_flash_t *region_map = (eHNFFS_region_map_flash_t *)data;
                err = eHNFFS_regionmap_assign(eHNFFS, eHNFFS->manager->region_map, region_map,
                                              super->sector, super->free_off);
                if (err)
                {
                    goto cleanup;
                }
                break;
            }

            case eHNFFS_DATA_COMMIT:
            {
                // Commit message of eHNFFS. When unmount, we should record some message to
                // effectively construct right in ram structure at next mount.
                eHNFFS_commit_flash_t *commit = (eHNFFS_commit_flash_t *)data;
                err = eHNFFS_commit_update(eHNFFS, commit, eHNFFS->pcache);
                if (err)
                {
                    goto cleanup;
                }

                super->commit_off = super->free_off;
                break;
            }

            case eHNFFS_DATA_MAGIC:
            {

                // Checkout the magic number. If it's right, then finish mounting.
                // When unmount, we write a new magic number,
                // When mount, we checkout the magic number and then turn it to 0.
                eHNFFS_magic_flash_t *magic = (eHNFFS_magic_flash_t *)data;

                if (magic->maigc == 0)
                {
                    // Not the newest magic number we write during last unmount.
                    break;
                }
                else if (magic->maigc == eHNFFS_MAGIC)
                {
                    err = eHNFFS_data_delete(eHNFFS, eHNFFS_ID_SUPER, super->sector, super->free_off, len);
                    if (err)
                    {
                        goto cleanup;
                    }
                    super->free_off += len;
                    return err;
                }
                else
                {
                    err = eHNFFS_ERR_CORRUPT;
                    goto cleanup;
                }
            }

            case eHNFFS_DATA_DELETE:
                // Just skip, no need to do anything.
                break;

            default:
                // Other type of data head shouldn't be read, so there has been some problem.
                err = eHNFFS_ERR_CORRUPT;
                goto cleanup;
            }

            super->free_off += len;
            data += len;
        }
    }

cleanup:
    err2 = eHNFFS_deinit(eHNFFS);
    // printf("err and err2 are %d, %d\r\n", err, err2);
    if (err2)
        return err2;
    return err;
}

/**
 * Unmount function of eHNFFS.
 */
int eHNFFS_rawunmount(eHNFFS_t *eHNFFS)
{
    int err = eHNFFS_ERR_OK;

    // Flush changed file to flash.
    eHNFFS_file_ram_t *file = eHNFFS->file_list->file;
    while (file != NULL)
    {
        if (file->file_cache.mode)
        {
            // Change flag.
            err = eHNFFS_file_flush(eHNFFS, file);
            if (err)
            {
                return err;
            }
        }
        file = file->next_file;
    }

    eHNFFS_dir_ram_t *dir = eHNFFS->dir_list->dir;
    while (dir != NULL)
    {
        eHNFFS_skip_flash_t skip = {
            .head = eHNFFS_MKDHEAD(0, 1, dir->id, eHNFFS_DATA_SKIP, sizeof(eHNFFS_skip_flash_t)),
            .len = dir->rest_space,
        };

        err = eHNFFS_dir_prog(eHNFFS, dir, &skip, sizeof(eHNFFS_skip_flash_t), eHNFFS_FORWARD);
        if (err)
        {
            return err;
        }
        dir = dir->next_dir;
    }

    // Set old commit data type to delete.
    eHNFFS_superblock_ram_t *super = eHNFFS->superblock;
    err = eHNFFS_data_delete(eHNFFS, eHNFFS_ID_SUPER, super->sector, super->commit_off,
                             sizeof(eHNFFS_commit_flash_t));
    if (err)
    {
        return err;
    }

    // Flush region map to flash.
    if (eHNFFS->manager->region_map->change_flag)
    {
        err = eHNFFS_region_map_flush(eHNFFS, eHNFFS->manager->region_map);
        if (err)
        {
            return err;
        }
    }

    // Prog new commit message.
    eHNFFS_size_t size = sizeof(eHNFFS_commit_flash_t);
    eHNFFS_commit_flash_t commit = {
        .head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_COMMIT, size),
        .next_id = eHNFFS->id_map->free_map->region * eHNFFS->id_map->bits_in_buffer +
                   eHNFFS->id_map->free_map->index,
        .scan_times = eHNFFS->manager->scan_times,
        .next_dir_sector = eHNFFS->manager->dir_map->region * eHNFFS->manager->region_size +
                           eHNFFS->manager->dir_map->index,
        .next_bfile_sector = eHNFFS->manager->bfile_map->region * eHNFFS->manager->region_size +
                             eHNFFS->manager->bfile_map->index,
        .reserve = eHNFFS->manager->region_map->reserve,
        .wl_index = (eHNFFS->manager->wl == NULL) ? eHNFFS_NULL
                                                  : eHNFFS->manager->wl->index,
        .wl_bfile_index = (eHNFFS->manager->wl == NULL) ? eHNFFS_NULL
                                                        : eHNFFS->manager->wl->bfile_index,
        .wl_free_off = (eHNFFS->manager->wl == NULL) ? eHNFFS_NULL
                                                     : eHNFFS->manager->wl->free_off,
    };
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, eHNFFS->pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, &commit, size);
    if (err)
    {
        return err;
    }
    super->commit_off = super->free_off;
    super->free_off += size;

    // In-flash magic message.
    size = sizeof(eHNFFS_magic_flash_t);
    eHNFFS_magic_flash_t magic = {
        .head = eHNFFS_MKDHEAD(0, 1, eHNFFS_ID_SUPER, eHNFFS_DATA_MAGIC, size),
        .maigc = eHNFFS_MAGIC,
    };
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, eHNFFS->pcache, NULL, eHNFFS_VALIDATE,
                            super->sector, super->free_off, &magic, size);
    if (err)
    {
        return err;
    }
    super->free_off += size;

    // Flush wl add message to flash.
    if (eHNFFS->manager->wl)
    {
        err = eHNFFS_wl_flush(eHNFFS, eHNFFS->manager->wl);
        if (err)
        {
            return err;
        }
    }

    // Flush changed hash tree entry to flash.
    err = eHNFFS_entry_flush(eHNFFS, eHNFFS->hash_tree, eHNFFS_CHANGED_ENTRY);
    if (err)
    {
        return err;
    }

    // Flush free id map to flash.
    eHNFFS_map_ram_t *map = eHNFFS->id_map->free_map;
    eHNFFS_size_t len = eHNFFS->id_map->bits_in_buffer / 8;
    err = eHNFFS_map_flush(eHNFFS, map, len);
    if (err)
    {
        return err;
    }

    // Flush remove id map to flash.
    map = eHNFFS->id_map->remove_map;
    if (map->index)
    {
        err = eHNFFS_map_flush(eHNFFS, map, len);
        if (err)
        {
            return err;
        }
    }

    // Flush free dir map to flash.
    map = eHNFFS->manager->dir_map;
    len = eHNFFS->manager->region_size / 8;
    err = eHNFFS_map_flush(eHNFFS, map, len);
    if (err)
    {
        return err;
    }

    // Flush free big file map to flash.
    map = eHNFFS->manager->bfile_map;
    err = eHNFFS_map_flush(eHNFFS, map, len);
    if (err)
    {
        return err;
    }

    // Flush erase map to flash.
    map = eHNFFS->manager->erase_map;
    if (map->index)
    {
        err = eHNFFS_map_flush(eHNFFS, map, len);
        if (err)
        {
            return err;
        }
    }

    // Flush data in pcache to flash.
    err = eHNFFS_cache_flush(eHNFFS, eHNFFS->pcache, NULL, eHNFFS_VALIDATE);
    if (err)
    {
        return err;
    }

    // Free all in-ram structre's memory.
    err = eHNFFS_deinit(eHNFFS);
    if (err)
    {
        return err;
    }

    return err;
}

/**
 * Get the size we have used in nor flash.
 *
 * The approach is to traverse the free sector map and erase sector map.
 * So if formating without erase the whole nor flash at first use, It may not accurate.
 */
eHNFFS_size_t eHNFFS_fs_rawsize(eHNFFS_t *eHNFFS)
{
    int err = eHNFFS_ERR_OK;
    eHNFFS_ssize_t num = 0;
    eHNFFS_ssize_t temp = 0;
    eHNFFS_ssize_t map_len = eHNFFS->cfg->sector_count / 8;
    int littlelen = eHNFFS->manager->region_size / 8;
    err = eHNFFS_map_flush(eHNFFS, eHNFFS->manager->dir_map, littlelen);
    if (err)
    {
        return err;
    }

    err = eHNFFS_map_flush(eHNFFS, eHNFFS->manager->bfile_map, littlelen);
    if (err)
    {
        return err;
    }

    err = eHNFFS_map_flush(eHNFFS, eHNFFS->manager->erase_map, littlelen);
    if (err)
    {
        return err;
    }

    // Calculate used sectors in free map(In flash, dir map and big file map are the same.)
    eHNFFS_map_ram_t *map = eHNFFS->manager->dir_map;
    temp = eHNFFS_valid_num(eHNFFS, map->begin, map->off, map_len, eHNFFS->rcache);
    if (temp < 0)
    {
        return temp;
    }
    num += temp;

    // Calculate free sectors in above used sectors.
    map = eHNFFS->manager->erase_map;
    temp = eHNFFS_valid_num(eHNFFS, map->begin, map->off, map_len, eHNFFS->rcache);
    if (temp < 0)
    {
        return temp;
    }
    num -= temp;

    return num;
}

/**
 * Find if file with id is opened and stored in file list.
 */
bool eHNFFS_file_isopen(eHNFFS_file_list_ram_t *file_list, eHNFFS_size_t id)
{
    eHNFFS_file_ram_t *file = file_list->file;
    while (file != NULL)
    {
        if (id == file->id)
        {
            return true;
        }
        file = file->next_file;
    }

    return false;
}

/**
 * Find if dir with id is opened and stored in dir list.
 */
bool eHNFFS_dir_isopen(eHNFFS_dir_list_ram_t *dir_list, eHNFFS_size_t id)
{
    eHNFFS_dir_ram_t *dir = dir_list->dir;
    while (dir != NULL)
    {
        if (id == dir->id)
        {
            return true;
        }
        dir = dir->next_dir;
    }
    return false;
}

/**
 * -------------------------------------------------------------------------------------------------------
 * -------------------------------------    File level operations    -------------------------------------
 * -------------------------------------------------------------------------------------------------------
 */

/**
 * Open file function.
 * Flags is used to choose the open type of file, but in our demo is no use now.
 */
int eHNFFS_file_rawopen(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t **file,
                        char *path, int flags)
{
    int err = eHNFFS_ERR_OK;

    // If opened file is too much, we can't open anymore.
    // The purpose is to save ram memory.
    if (eHNFFS->file_list->count >= eHNFFS_FILE_LIST_MAX)
    {
        printf("list num is no %d, %d\r\n", eHNFFS->file_list->count, eHNFFS->dir_list->count);
        err = eHNFFS_ERR_MUCHOPEN;
        return err;
    }

    err = eHNFFS_cache_flush(eHNFFS, eHNFFS->pcache, eHNFFS->rcache, eHNFFS_VALIDATE);
    if (err)
    {
        return err;
    }

    // find function, return a head/err message(find the name is enough.)
    eHNFFS_size_t tail = eHNFFS_NULL;
    eHNFFS_tree_entry_ram_t *entry = NULL;
    eHNFFS_size_t father_id = eHNFFS_NULL;
    err = eHNFFS_father_find(eHNFFS, path, &tail, &entry, &father_id);
    if (err)
    {
        return err;
    }

    // TODO in the future.
    // Now we have all dir entry in ram, so we can ignore how to construct
    // ram-dir structure only with tail of dir.
    eHNFFS_dir_ram_t *dir = NULL;
    if (entry != NULL)
    {
        tail = entry->tail;
    }

    // TODO, something has changed.
    // Find The true name of file in path.
    char *name = eHNFFS_father_path(path);
    if (*name == '/')
        name++;

    // Open father dir of file.
    err = eHNFFS_dir_lowopen(eHNFFS, entry->tail, entry->id, father_id, entry->sector,
                             entry->off, strlen(name), eHNFFS_SECTOR_DIR, &dir);
    if (err)
    {
        return err;
    }

    // Traverse dir to find whether or not the file is already in dir.
    bool if_find = false;
    eHNFFS_size_t id = eHNFFS_NULL;
    eHNFFS_size_t name_sector = eHNFFS_NULL;
    eHNFFS_off_t name_off = eHNFFS_NULL;
    err = eHNFFS_dtraverse_name(eHNFFS, dir->tail, name, strlen(name), eHNFFS_DATA_REG,
                                &if_find, NULL, &id, &name_sector, &name_off);
    if (err)
    {
        return err;
    }

    if (if_find)
    {
        // If we find, then read data directly
        err = eHNFFS_file_lowopen(eHNFFS, dir, id, name_sector, name_off, strlen(name), file, eHNFFS_DATA_REG);
        if (err)
        {
            return err;
        }
    }
    else
    {
        // If not, we should create a new file.
        err = eHNFFS_create_file(eHNFFS, dir, file, name, strlen(name));
        if (err)
        {
            return err;
        }
    }

    return err;
}

/**
 * Close file function.
 */
int eHNFFS_file_rawclose(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file)
{
    return eHNFFS_file_lowclose(eHNFFS, file);
}

/**
 * Change the position of current file.
 */
eHNFFS_soff_t eHNFFS_file_rawseek(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                                  eHNFFS_soff_t off, int whence)
{
    eHNFFS_off_t npos = file->pos;
    switch (whence)
    {
    case eHNFFS_SEEK_SET:
        // Absolute position, i.e purly off.
        npos = off;
        break;

    case eHNFFS_SEEK_CUR:
        // Current + off;
        if ((eHNFFS_soff_t)file->pos + off < 0)
            return eHNFFS_ERR_INVAL;
        else
            npos = file->pos + off;

        break;

    case eHNFFS_SEEK_END:
    {
        // May be change in the future,
        // When just open, we don't know the file size in current version.
        eHNFFS_soff_t res = file->file_size + off;
        if (res < 0)
            return eHNFFS_ERR_INVAL;
        else
            npos = res;
        break;
    }

    default:
        return eHNFFS_ERR_INVAL;
    }

    // If after seeking, position is larger than file size,
    // then it's error.
    if (npos > eHNFFS->cfg->file_max)
    {
        return eHNFFS_ERR_INVAL;
    }

    // don't need to do too much
    // If it's small file, we noly need to visit small cache.
    // If it's big file, we use index in cache and read/prog cache.
    file->pos = npos;
    return npos;
}

/**
 * File read function.
 */
eHNFFS_ssize_t eHNFFS_file_rawread(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                                   void *buffer, eHNFFS_size_t size)
{
    // Judge if we read is large than file size.
    eHNFFS_size_t nsize = size;

    if (file->pos + nsize > file->file_size)
    {
        printf("???, %d\n", file->file_size);
        return eHNFFS_ERR_INVAL;
    }

    if (file->file_size <= eHNFFS_FILE_CACHE_SIZE - sizeof(eHNFFS_head_t))
    {
        // Small file read function.
        return eHNFFS_small_file_read(eHNFFS, file, buffer, size);
    }
    else
    {
        // big file read function
        return eHNFFS_big_file_read(eHNFFS, file, buffer, size);
    }
}

/**
 * File write function.
 */
eHNFFS_ssize_t eHNFFS_file_rawwrite(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                                    void *buffer, eHNFFS_size_t size)
{
    // If after writing, the file is too big, we refuse the operation.
    if (file->pos + size > eHNFFS->cfg->file_max)
    {
        return eHNFFS_ERR_FBIG;
    }

    eHNFFS_size_t threshold = eHNFFS_FILE_CACHE_SIZE - sizeof(eHNFFS_head_t);
    if (file->file_size == 0)
    {
        if (size <= eHNFFS_FILE_CACHE_SIZE - sizeof(eHNFFS_head_t))
            // Prog small file.
            return eHNFFS_small_file_write(eHNFFS, file, buffer, size);
        else
        {
            // Prog big file.
            return eHNFFS_s2b_file_write(eHNFFS, file, buffer, size);
        }
    }
    else if (file->file_size <= threshold && file->pos + size <= threshold)
    {
        // If it's always small file.
        return eHNFFS_small_file_write(eHNFFS, file, buffer, size);
    }
    else if (file->file_size <= threshold && file->file_size + size > threshold)
    {
        // If after writing, small file turns to big file.
        return eHNFFS_s2b_file_write(eHNFFS, file, buffer, size);
    }
    else if (file->file_size > threshold)
    {
        // If it's always big file.
        return eHNFFS_big_file_write(eHNFFS, file, buffer, size);
    }

    return eHNFFS_ERR_INVAL;
}

/**
 * File delete function.
 */
int eHNFFS_file_rawdelete(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file)
{
    int err = eHNFFS_ERR_OK;

    // Find in-ram structure of file's dir.
    eHNFFS_dir_ram_t *dir = NULL;
    err = eHNFFS_fdir_file_find(eHNFFS, file, &dir);
    if (err)
    {
        return err;
    }

    // If it's big file, we should set all sectors that stores its data to old.
    eHNFFS_head_t head = *(eHNFFS_head_t *)file->file_cache.buffer;
    if (eHNFFS_dhead_type(head) == eHNFFS_DATA_BFILE_INDEX)
    {
        // Because head in buffer may be old, we use size in file cache.
        eHNFFS_bfile_index_flash_t *index = (eHNFFS_bfile_index_flash_t *)file->file_cache.buffer;
        eHNFFS_size_t num = (file->file_cache.size - sizeof(eHNFFS_head_t)) /
                            sizeof(eHNFFS_bfile_index_ram_t);
        err = eHNFFS_bfile_sector_old(eHNFFS, &index->index[0], num);
        if (err)
        {
            return err;
        }
    }

    // Delete small file's data or big file's index.
    err = eHNFFS_data_delete(eHNFFS, file->father_id, file->file_cache.sector,
                             file->file_cache.off, eHNFFS_dhead_dsize(head));
    if (err)
    {
        return err;
    }

    // Delete file's name in its father dir.
    err = eHNFFS_data_delete(eHNFFS, file->father_id, file->sector, file->off, file->namelen);
    if (err)
    {
        return err;
    }

    // Free id that the file belongs to.
    err = eHNFFS_id_free(eHNFFS, eHNFFS->id_map, file->id);
    if (err)
    {
        return err;
    }

    // Free in-ram memory of the file.
    err = eHNFFS_file_free(eHNFFS->file_list, file);
    return err;
}

/**
 * -------------------------------------------------------------------------------------------------------
 * -------------------------------------    Dir level operations    --------------------------------------
 * -------------------------------------------------------------------------------------------------------
 */

/**
 * Open function of dir.
 * If dir is not in flash, then we create a new one.
 */
int eHNFFS_dir_rawopen(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t **dir, char *path)
{
    int err = eHNFFS_ERR_OK;

    // If opened dir is too much, then we should close some.
    if (eHNFFS->dir_list->count >= eHNFFS_DIR_LIST_MAX)
    {
        printf("list num is no %d, %d\r\n", eHNFFS->file_list->count, eHNFFS->dir_list->count);
        err = eHNFFS_ERR_MUCHOPEN;
        return err;
    }

    // find function, return a head/err message(find the name is enough.)
    eHNFFS_size_t father_tail = eHNFFS_NULL;
    eHNFFS_tree_entry_ram_t *father_entry = NULL;
    eHNFFS_size_t father_id = eHNFFS_NULL;
    err = eHNFFS_father_find(eHNFFS, path, &father_tail, &father_entry, &father_id);
    if (err)
    {
        return err;
    }

    // TODO in the future.
    // Now we have all dir entry in ram, so we can ignore how to construct
    // ram-dir structure only with tail of dir.
    eHNFFS_dir_ram_t *father_dir = NULL;
    if (father_entry != NULL)
    {
        father_tail = father_entry->tail;
        father_id = father_entry->id;
    }

    // Find The true name of file in path.
    char *name = eHNFFS_father_path(path);
    if (*name == '/')
        name++;
    eHNFFS_size_t namelen = strlen(name);

    bool if_find = false;
    // We find in tree entry first, if not find, then traverse dir.
    // But in our sitiation, it must find.
    int i = 0;
    eHNFFS_tree_entry_ram_t *temp_entry = NULL;
    if (father_entry != NULL)
    {
        for (i = 0; i < father_entry->num_of_son; i++)
        {
            temp_entry = &father_entry->son_list[i];
            if (temp_entry->data_type == eHNFFS_DATA_TREE_HASH)
                return eHNFFS_ERR_DIRHASH;

            if (memcmp(temp_entry->data.name, name, namelen) == 0)
            {
                if_find = true;
                break;
            }
        }
    }
    else if (father_id == eHNFFS_ID_SUPER)
    {
        temp_entry = eHNFFS->hash_tree->root_hash;
        if (memcmp(temp_entry->data.name, name, namelen) == 0)
        {
            if_find = true;
        }
    }

    // zhu shi
    // Now we don't need to use following code, because tree index is always useful.
    /* // Traverse dir to find whether or not the file is already in dir.
    eHNFFS_size_t id = eHNFFS_NULL;
    eHNFFS_size_t name_sector = eHNFFS_NULL;
    eHNFFS_off_t name_off = eHNFFS_NULL;
    eHNFFS_size_t current_tail = eHNFFS_NULL;
    err = eHNFFS_dtraverse_name(eHNFFS, father_dir->tail, name, namelen, eHNFFS_DATA_DIR,
                                &if_find, &current_tail, &id, &name_sector, &name_off);
    if (err)
    {
        return err;
    } */

    if (if_find)
    {
        // Now we don't need to use it.
        // If dir is in flash, open it.
        /* err = eHNFFS_dir_lowopen(eHNFFS, current_tail, id, father_dir->id, name_sector, name_off,
                                 namelen, eHNFFS_DATA_DIR, dir);
        if (err)
        {
            return err;
        } */

        err = eHNFFS_dir_lowopen(eHNFFS, temp_entry->tail, temp_entry->id, father_id, temp_entry->sector,
                                 temp_entry->off, namelen, eHNFFS_DATA_DIR, dir);
        if (err)
        {
            return err;
        }
    }
    else
    {
        // Open father dir of dir we need to open.
        err = eHNFFS_father_id_find(eHNFFS, father_entry->id, eHNFFS_MIGRATE, &father_dir);
        if (err)
        {
            return err;
        }

        // If dir is not in flash, we should create a new one.
        err = eHNFFS_create_dir(eHNFFS, father_dir, dir, name, namelen);
        if (err)
        {
            return err;
        }
    }
    return err;
}

/**
 * Close dir function.
 */
int eHNFFS_dir_rawclose(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir)
{
    return eHNFFS_dir_lowclose(eHNFFS, dir);
}

/**
 * Dir delete function.
 * Notice that now we now we don't achieve deleting dir's son dir.
 */
int eHNFFS_dir_rawdelete(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir)
{
    int err = eHNFFS_ERR_OK;

    // Find dir's father dir structure.
    eHNFFS_dir_ram_t *father_dir = NULL;
    err = eHNFFS_fdir_dir_find(eHNFFS, dir, &father_dir);
    if (err)
    {
        return err;
    }

    // Traverse to find all bfile index, turn relative sectors to old.
    err = eHNFFS_dtraverse_bfile_delete(eHNFFS, dir);
    if (err)
    {
        return err;
    }

    // Set all sectors belongs to dir to old.
    err = eHNFFS_dir_old(eHNFFS, dir->tail);
    if (err)
    {
        return err;
    }

    // Delete dir name entry in father dir.
    eHNFFS_ASSERT(dir->namelen <= eHNFFS_NAME_MAX);
    err = eHNFFS_data_delete(eHNFFS, father_dir->id, dir->sector, dir->off,
                             sizeof(eHNFFS_dir_name_flash_t) + dir->namelen);
    if (err)
    {
        return err;
    }

    // Find father dir's tree entry.
    eHNFFS_tree_entry_ram_t *father_entry = NULL;
    err = eHNFFS_entry_idfind(eHNFFS, father_dir->id, &father_entry);
    if (err)
    {
        return err;
    }

    // Delete the entry both in ram and in flash.
    err = eHNFFS_entry_delete(eHNFFS, father_entry, dir->id);
    if (err)
    {
        return err;
    }

    // Free id that belongs to dir.
    err = eHNFFS_id_free(eHNFFS, eHNFFS->id_map, dir->id);
    if (err)
    {
        return err;
    }

    // Free in-ram dir structure.
    err = eHNFFS_dir_free(eHNFFS->dir_list, dir);
    if (err)
    {
        return err;
    }

    return err;
}

/**
 * Read  a dir entry.
 */
int eHNFFS_dir_rawread(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_info_ram_t *info)
{
    int err = eHNFFS_ERR_OK;
    memset(info, 0, sizeof(*info));

    // If it's not initialized, we should do it.
    if (dir->pos_sector == eHNFFS_NULL)
    {
        dir->pos_sector = dir->tail;
        dir->pos_off = 0;
        dir->pos_presector = eHNFFS_NULL;
    }

    uint8_t *data = NULL;
    eHNFFS_size_t head;
    eHNFFS_size_t len = sizeof(eHNFFS_head_t);
    while (true)
    {
        if (!(eHNFFS->rcache->sector == dir->pos_sector &&
              eHNFFS->rcache->off + eHNFFS->rcache->size >= dir->pos_off + len &&
              eHNFFS->rcache->mode == eHNFFS_FORWARD))
        {
            // If what we need is not in read cache, we should re-read.
            err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, eHNFFS->rcache, dir->pos_sector, dir->pos_off,
                                       eHNFFS_min(eHNFFS->cfg->cache_size, eHNFFS->cfg->sector_size - dir->pos_off));
            if (err)
            {
                return err;
            }

            data = eHNFFS->rcache->buffer;
        }
        else
        {
            // If it is, then we just need to change the position of data pointer.
            data = eHNFFS->rcache->buffer + dir->pos_off - eHNFFS->rcache->off;
        }

        if (dir->pos_off == 0)
        {
            // Read and record the pre-sector of current sector.
            head = *(eHNFFS_head_t *)data;
            err = eHNFFS_shead_check(head, eHNFFS_SECTOR_USING, eHNFFS_SECTOR_DIR);
            if (err)
            {
                return err;
            }

            eHNFFS_dir_sector_flash_t *dir_sector = (eHNFFS_dir_sector_flash_t *)data;
            dir->pos_presector = dir_sector->pre_sector;
            data += sizeof(eHNFFS_dir_sector_flash_t);
            dir->pos_off += sizeof(eHNFFS_dir_sector_flash_t);
            len = sizeof(eHNFFS_head_t);
        }

        bool if_next = false;
        while (true)
        {
            head = *(eHNFFS_head_t *)data;
            // Check if the head is valid.
            err = eHNFFS_dhead_check(head, eHNFFS_NULL, (int)eHNFFS_NULL);
            if (err)
            {
                return err;
            }

            len = eHNFFS_dhead_dsize(head);
            if (dir->pos_off + len > eHNFFS->rcache->off + eHNFFS->rcache->size)
            {
                if (head == eHNFFS_NULL && dir->pos_presector != eHNFFS_NULL)
                {
                    // If there's no valid data in current sector, we should traverse pre-sector.
                    dir->pos_sector = dir->pos_presector;
                    dir->pos_off = 0;
                    len = sizeof(eHNFFS_head_t);
                    break;
                }
                else if (head == eHNFFS_NULL && dir->pos_presector == eHNFFS_NULL)
                {
                    // If we don't have pre-sector, then end.
                    return err;
                }

                // We have read whole data in cache.
                break;
            }

            switch (eHNFFS_dhead_type(head))
            {
            case eHNFFS_DATA_DIR_NAME:
            {
                eHNFFS_dir_name_flash_t *dname = (eHNFFS_dir_name_flash_t *)data;
                info->type = eHNFFS_DATA_DIR;
                memcpy(info->name, dname->name, len - sizeof(eHNFFS_dir_name_flash_t));
                info->name[len - sizeof(eHNFFS_dir_name_flash_t)] = '\0';
                dir->pos_off += len;
                return err;
            }

            case eHNFFS_DATA_FILE_NAME:
            {
                eHNFFS_file_name_flash_t *fname = (eHNFFS_file_name_flash_t *)data;
                info->type = eHNFFS_DATA_REG;
                memcpy(info->name, fname->name, len - sizeof(eHNFFS_file_name_flash_t));
                info->name[len - sizeof(eHNFFS_file_name_flash_t)] = '\0';
                dir->pos_off += len;
                return err;
            }

            case eHNFFS_DATA_DELETE:
                break;

            case eHNFFS_DATA_SKIP:
                if_next = true;
                break;

            default:
                break;
            }

            if (if_next)
            {
                if (dir->pos_presector == eHNFFS_NULL)
                    return err;
                dir->pos_sector = dir->pos_presector;
                dir->pos_off = 0;
                break;
            }
            else
            {
                dir->pos_off += len;
                data += len;
            }
        }
    }
}
