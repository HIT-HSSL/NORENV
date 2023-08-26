/**
 * Hash tree related operations.
 */
#include "eHNFFS_tree.h"
#include "eHNFFS_head.h"
#include "eHNFFS_cache.h"
#include "eHNFFS_dir.h"
#include "eHNFFS_manage.h"

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * ----------------------------------------------------------    Hash queue operations    --------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Initialize the in-ram queue structure.
 * The queue is a circular queue, and we allocate all space needed in it.
 */
int eHNFFS_hash_queue_init(eHNFFS_hash_queue_ram_t **queue_addr)
{
    int err = eHNFFS_ERR_OK;

    // we should make sure that dir_hash in ram <= 128.
    eHNFFS_hash_queue_ram_t *queue = eHNFFS_malloc(sizeof(eHNFFS_hash_queue_ram_t));
    if (!queue)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }
    queue->num = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->array = eHNFFS_malloc(20 * sizeof(eHNFFS_tree_entry_ram_t *));
    if (!queue->array)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }
    *queue_addr = queue;
    return eHNFFS_ERR_OK;

cleanup:
    if (queue)
    {
        if (queue->array)
            eHNFFS_free(queue->array);
        eHNFFS_free(queue);
    }
    return err;
}

/**
 * Delete the in-ram queue structure.
 */
void eHNFFS_hash_queue_delete(eHNFFS_hash_queue_ram_t *queue)
{
    eHNFFS_free(queue->array);
    eHNFFS_free(queue);
}

/**
 * Add a member into the queue.
 */
void eHNFFS_hash_enqueue(eHNFFS_hash_queue_ram_t *queue, eHNFFS_tree_entry_ram_t *entry)
{
    if (queue->num > 128)
    {
        return;
    }
    queue->num += 1;
    queue->array[queue->tail] = entry;
    queue->tail = (queue->tail + 1) % 128;
}

/**
 * Delete a member from queue.
 */
eHNFFS_tree_entry_ram_t *eHNFFS_hash_dequeue(eHNFFS_hash_queue_ram_t *queue)
{
    if (queue->num <= 0)
    {
        return NULL;
    }
    queue->num -= 1;
    queue->head = (queue->head + 1) % 128;
    return queue->array[queue->head - 1];
}
/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * ------------------------------------------------------------    Hash calculation    -----------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * A easy hash function from:
 * https://github.com/bloominstituteoftechnology/C-Hash-Tables/blob/master/full_hashtable/hashtables.c
 */
static eHNFFS_size_t eHNFFS_hash_temp(void *buffer, int16_t len)
{
    eHNFFS_size_t hash = 5381;
    uint8_t *data = (uint8_t *)buffer;

    while (len > 0)
    {
        hash = ((hash << 5) + hash) + *data;
        data++;
    }

    return hash;
}

/**
 * The API exposed to other module.
 */
eHNFFS_hash_t eHNFFS_hash(uint8_t *buffer, eHNFFS_size_t len)
{
    return eHNFFS_hash_temp(buffer, (int16_t)len);
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * ----------------------------------------------------------    Hash tree operations    ---------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Initialization hash tree structure.
 */
int eHNFFS_hash_tree_initialization(eHNFFS_t *eHNFFS, eHNFFS_hash_tree_ram_t **tree_addr)
{
    int err = eHNFFS_ERR_OK;

    // Allocate memory for it.
    eHNFFS_hash_tree_ram_t *tree = eHNFFS_malloc(sizeof(eHNFFS_hash_tree_ram_t));
    if (!tree)
    {
        err = eHNFFS_ERR_NOMEM;
        goto cleanup;
    }

    // Initialize basic data.
    tree->begin = eHNFFS_NULL;
    tree->num = 0;
    tree->free_off = eHNFFS_NULL;
    tree->root_hash = NULL;
    *tree_addr = tree;
    return err;

cleanup:
    if (tree)
        eHNFFS_free(tree);
    return err;
}

/**
 * Free memory of in-ram dir hash tree started at tree_entry.
 * Notice that when deleting, we consider tree_entry as root of tree that we delete.
 */
int eHNFFS_hash_tree_free(eHNFFS_tree_entry_ram_t *tree_entry)
{
    int err = eHNFFS_ERR_OK;

    // Initialize queue and add root entry to queue.
    eHNFFS_hash_queue_ram_t *queue, *delete_queque;
    err = eHNFFS_hash_queue_init(&queue);
    if (err)
    {
        return err;
    }
    eHNFFS_hash_enqueue(queue, tree_entry);

    // Initialize delete queue and add root entry to it.
    err = eHNFFS_hash_queue_init(&delete_queque);
    if (err)
    {
        return err;
    }
    eHNFFS_hash_enqueue(delete_queque, tree_entry);

    // Find all first position of entry's son.
    eHNFFS_tree_entry_ram_t *temp_entry;
    while (queue->num > 0)
    {
        temp_entry = eHNFFS_hash_dequeue(queue);
        for (int i = 0; i < temp_entry->num_of_son; i++)
        {
            eHNFFS_hash_enqueue(queue, &temp_entry->son_list[i]);
        }

        if (temp_entry->num_of_son > 0)
            eHNFFS_hash_enqueue(delete_queque, temp_entry->son_list);
    }

    // Delete all.
    while (delete_queque->num > 0)
    {
        temp_entry = eHNFFS_hash_dequeue(delete_queque);
        eHNFFS_free(temp_entry);
    }

    eHNFFS_hash_queue_delete(queue);
    eHNFFS_hash_queue_delete(delete_queque);
    return err;
}

/**
 * Find the father tree entry by father id.
 *  1. Only used in hash tree construction.
 *  2. Last_father parameter is the father of last hash entry, we utilize the locality principle
 *     to reduce search time.
 */
static int eHNFFS_father_in_tree(eHNFFS_tree_entry_ram_t *root, eHNFFS_tree_entry_ram_t *last_father,
                                 eHNFFS_size_t father_id, eHNFFS_tree_entry_ram_t **result)
{
    int err = eHNFFS_ERR_OK;

    if (last_father != NULL && last_father->id == father_id)
    {
        *result = last_father;
        return err;
    }

    eHNFFS_hash_queue_ram_t *queue;
    err = eHNFFS_hash_queue_init(&queue);
    if (err)
    {
        return err;
    }
    eHNFFS_hash_enqueue(queue, root);

    eHNFFS_tree_entry_ram_t *temp_entry = NULL;
    while (queue->num > 0)
    {
        temp_entry = eHNFFS_hash_dequeue(queue);

        if (temp_entry->id == father_id)
        {
            *result = temp_entry;
            goto cleanup;
        }

        for (int i = 0; i < temp_entry->num_of_son; i++)
        {
            eHNFFS_hash_enqueue(queue, &temp_entry->son_list[i]);
        }
    }

    *result = NULL;
    err = eHNFFS_ERR_NOFATHER;

cleanup:
    eHNFFS_hash_queue_delete(queue);
    return err;
}

/**
 * Delete a sub-tree that consider entry of id is the root.
 */
static int eHNFFS_delete_tree(eHNFFS_t *eHNFFS, eHNFFS_tree_entry_ram_t *father_entry, eHNFFS_size_t id)
{
    eHNFFS_tree_entry_ram_t *entry = NULL;
    int16_t index = -1;
    for (int i = 0; i < father_entry->num_of_son; i++)
    {
        if (father_entry->son_list[i].id == id)
        {
            entry = &father_entry->son_list[i];
            index = i;
            break;
        }
    }

    if (entry == NULL || father_entry->num_of_son == 0)
    {
        return eHNFFS_ERR_NOFATHER;
    }

    // Now we can't delete its son dir.
    if (entry->num_of_son != 0)
        return eHNFFS_ERR_CANTDELETE;

    // Now it's no use.
    /* int err = eHNFFS_hash_tree_free(entry);
    if (err)
    {
        return err;
    } */

    eHNFFS->hash_tree->num -= 1;
    father_entry->num_of_son -= 1;
    memcpy(&father_entry->son_list[index], &father_entry->son_list[index + 1],
           (father_entry->num_of_son - index) * sizeof(eHNFFS_tree_entry_ram_t));

    // TODO, ADD
    if (father_entry->num_of_son == 0)
    {
        eHNFFS_free(father_entry->son_list);
        father_entry->son_list = NULL;
    }

    return eHNFFS_ERR_OK;
}

/**
 * Initialize the structure of hash tree entry.
 */
void eHNFFS_entry_initialize(eHNFFS_tree_entry_ram_t *tree_entry)
{
    tree_entry->id = eHNFFS_NULL;
    tree_entry->sector = eHNFFS_NULL;
    tree_entry->off = eHNFFS_NULL;
    tree_entry->tail = eHNFFS_NULL;
    tree_entry->num_of_son = 0;
    tree_entry->data_type = eHNFFS_NULL;
    tree_entry->son_list = NULL;
    tree_entry->data_type = eHNFFS_NULL;
    memset(tree_entry->data.name, 0, eHNFFS_ENTRY_NAME_LEN);
}

/**
 * Assign in-ram hash tree entry data of in-flash hash tree entry.
 */
void eHNFFS_ram_entry_init(eHNFFS_tree_entry_ram_t *ram_entry, eHNFFS_tree_entry_flash_t *flash_entry)
{
    ram_entry->id = eHNFFS_dhead_id(flash_entry->head);
    ram_entry->sector = flash_entry->sector;
    ram_entry->off = flash_entry->off;
    ram_entry->tail = flash_entry->tail;
    ram_entry->data_type = eHNFFS_dhead_type(flash_entry->head);
    if (ram_entry->data_type == eHNFFS_DATA_TREE_HASH)
    {
        ram_entry->data.hash = *(eHNFFS_size_t *)flash_entry->data;
    }
    else if (ram_entry->data_type == eHNFFS_DATA_TREE_NAME)
    {
        memcpy(ram_entry->data.name, flash_entry->data,
               eHNFFS_dhead_dsize(flash_entry->head) - sizeof(eHNFFS_tree_entry_flash_t));
    }
}

/**
 * Construct hash tree structure.
 */
int eHNFFS_construct_tree(eHNFFS_t *eHNFFS, eHNFFS_hash_tree_ram_t *hash_tree, eHNFFS_cache_ram_t *cache)
{
    int err = eHNFFS_ERR_OK;

    // We need to use a cache.
    eHNFFS_cache_one(eHNFFS, cache);

    eHNFFS_tree_entry_ram_t *last_father = NULL;
    eHNFFS_tree_entry_ram_t *current_father = NULL;
    eHNFFS_head_t head;
    eHNFFS_size_t sector = hash_tree->begin;
    eHNFFS_off_t off = 0;
    eHNFFS_size_t size = 0;
    uint8_t *data = cache->buffer;
    while (sector < hash_tree->begin + eHNFFS_TREE_SECTOR_NUM)
    {

        // Read data to cache.
        size = eHNFFS_min(eHNFFS->cfg->cache_size, eHNFFS->cfg->sector_size - off);
        err = eHNFFS->cfg->read(eHNFFS->cfg, sector, off, cache->buffer, size);
        cache->sector = sector;
        cache->off = off;
        cache->size = size;
        eHNFFS_ASSERT(err <= 0);
        if (err)
        {
            goto cleanup;
        }

        // Process the sector head structure.
        if (off == 0)
        {
            head = *(eHNFFS_head_t *)cache->buffer;
            err = eHNFFS_shead_check(head, eHNFFS_SECTOR_USING, eHNFFS_SECTOR_TREE);
            if (err)
            {
                goto cleanup;
            }

            off += sizeof(eHNFFS_head_t);
            data += sizeof(eHNFFS_head_t);
        }

        // Read in-flash tree entry circulately.
        while (true)
        {
            head = *(eHNFFS_head_t *)data;

            // eHNFFS_NULL means that there is nothing after, we should try to read another sector.
            if (head == eHNFFS_NULL &&
                off + sizeof(eHNFFS_tree_entry_flash_t) + 8 < eHNFFS->cfg->sector_size)
            {
                goto end;
            }
            else if (head == eHNFFS_NULL)
            {
                sector++;
                off = 0;
                data = cache->buffer;
                break;
            }

            // We can't read a whole hash tree entry in cache.
            if (eHNFFS_dhead_dsize(head) + off > cache->off + cache->size)
            {
                if (eHNFFS_dhead_dsize(head + off > eHNFFS->cfg->sector_size))
                {
                    err = eHNFFS_ERR_WRONGCAL;
                    goto cleanup;
                }
                break;
            }

            // Check data head.
            err = eHNFFS_dhead_check(head, eHNFFS_NULL, (int)eHNFFS_NULL);
            if (err)
            {
                goto cleanup;
            }

            // Doing different operations according to different head types.
            eHNFFS_delete_entry_flash_t *delete_entry;
            switch (eHNFFS_dhead_type(head))
            {
            case eHNFFS_DATA_TREE_HASH:
            case eHNFFS_DATA_TREE_NAME:
            {
                // Normal type, cread a in-ram entry.
                eHNFFS_tree_entry_flash_t *tree_entry = (eHNFFS_tree_entry_flash_t *)data;
                if (tree_entry->father_id == eHNFFS_ID_SUPER)
                {
                    // Root entry.
                    if (hash_tree->root_hash == NULL)
                    {
                        hash_tree->root_hash = eHNFFS_malloc(sizeof(eHNFFS_tree_entry_ram_t));
                        eHNFFS_entry_initialize(hash_tree->root_hash);
                        eHNFFS_ram_entry_init(hash_tree->root_hash, tree_entry);
                        last_father = hash_tree->root_hash;
                        hash_tree->num++;
                    }
                    hash_tree->root_hash->sector = tree_entry->sector;
                    hash_tree->root_hash->off = tree_entry->off;
                    hash_tree->root_hash->tail = tree_entry->tail;
                    hash_tree->root_hash->if_change = 0;
                }
                else
                {
                    // Normal entry, find its father first.
                    err = eHNFFS_father_in_tree(eHNFFS->hash_tree->root_hash, last_father,
                                                tree_entry->father_id, &current_father);
                    if (err)
                    {
                        goto cleanup;
                    }

                    // Add new entry to hash tree.
                    eHNFFS_size_t namelen = eHNFFS_dhead_dsize(tree_entry->head) - sizeof(eHNFFS_tree_entry_flash_t);
                    eHNFFS_size_t id = eHNFFS_dhead_id(tree_entry->head);
                    err = eHNFFS_entry_add(eHNFFS, current_father, id, tree_entry->sector, tree_entry->off,
                                           tree_entry->tail, (char *)tree_entry->data, namelen, false);
                    if (err)
                    {
                        goto cleanup;
                    }

                    last_father = current_father;
                }

                off += eHNFFS_dhead_dsize(head);
                data += eHNFFS_dhead_dsize(head);
                break;
            }

            // Only means entry and sub-entry that should be deleted.
            // We do nothing to the covered old entry but just write a new one.
            case eHNFFS_DATA_DELETE:
            {
                delete_entry = (eHNFFS_delete_entry_flash_t *)data;
                err = eHNFFS_father_in_tree(hash_tree->root_hash, last_father,
                                            delete_entry->father_id, &current_father);
                if (err)
                {
                    goto cleanup;
                }

                err = eHNFFS_delete_tree(eHNFFS, current_father, eHNFFS_dhead_id(delete_entry->head));
                if (err)
                {
                    goto cleanup;
                }

                off += eHNFFS_dhead_dsize(head);
                data += eHNFFS_dhead_dsize(head);
                break;

            default:
                err = eHNFFS_ERR_CORRUPT;
                goto cleanup;
            }
            }
        }
    }

end:
    hash_tree->free_off = (sector - hash_tree->begin) * eHNFFS->cfg->sector_size + off;

cleanup:
    eHNFFS_cache_one(eHNFFS, cache);
    return err;
}

/**
 * Compare name in flash with name in path.
 */
int eHNFFS_fname_cmp(eHNFFS_t *eHNFFS, eHNFFS_size_t sector, eHNFFS_size_t off,
                     char *name, eHNFFS_size_t namelen, bool *if_same)
{
    int err = eHNFFS_ERR_OK;

    err = eHNFFS_read_to_cache(eHNFFS, eHNFFS_FORWARD, eHNFFS->rcache, sector, off,
                               sizeof(eHNFFS_dir_name_flash_t) + namelen + 1);
    if (err)
    {
        return err;
    }
    eHNFFS_dir_name_flash_t *dsector = (eHNFFS_dir_name_flash_t *)eHNFFS->rcache->buffer;

    err = eHNFFS_shead_check(dsector->head, eHNFFS_SECTOR_USING, eHNFFS_SECTOR_DIR);
    if (err)
    {
        return err;
    }

    int flag = memcmp(name, dsector->name, namelen);
    *if_same = flag ? false : true;
    return err;
}

/**
 * Compare name in tree entry with name in path.
 */
int eHNFFS_ename_cmp(eHNFFS_t *eHNFFS, eHNFFS_tree_entry_ram_t *entry,
                     char *name, eHNFFS_size_t namelen, bool *if_same)
{
    int err = eHNFFS_ERR_OK;

    int flag;
    if (namelen <= eHNFFS_ENTRY_NAME_LEN)
    {
        flag = memcmp(name, entry->data.name, namelen);
        *if_same = flag ? false : true;
        return err;
    }

    eHNFFS_hash_t hash = eHNFFS_hash((uint8_t *)name, namelen);
    if (hash == entry->data.hash)
    {
        err = eHNFFS_fname_cmp(eHNFFS, entry->sector, entry->off, name, namelen, if_same);
        if (err)
        {
            return err;
        }
    }

    *if_same = false;
    return err;
}

char *eHNFFS_father_path(char *path)
{
    // The end of the father path.
    eHNFFS_size_t len = strlen(path);
    char *fend = path + len - 1;
    while (*fend != '/' && len > 0)
    {
        fend--;
        len--;
    }

    if (len == 0)
        fend++;

    return fend;
}

/**
 * Find father of the path by hash tree,
 * return the end of the find path(may not the destination) and
 * the corresponded hash tree entry.
 */
int eHNFFS_father_tree_find(eHNFFS_t *eHNFFS, char *path, char **end_path,
                            eHNFFS_tree_entry_ram_t **entry, eHNFFS_size_t *father_id)
{
    int err = eHNFFS_ERR_OK;

    char *name = path;
    char *father_end = eHNFFS_father_path(name);
    eHNFFS_tree_entry_ram_t *fentry = eHNFFS->hash_tree->root_hash;
    bool if_same = false;
    eHNFFS_size_t temp_id[2] = {eHNFFS_NULL, eHNFFS_NULL}; // 1 is new, 0 is old.
    while (name != father_end)
    {
        name += strspn(name, "/");
        eHNFFS_size_t namelen = strcspn(name, "/");

        if (name == (path + strspn(path, "/")))
        {
            err = eHNFFS_ename_cmp(eHNFFS, fentry, name, namelen, &if_same);
            if (err)
            {
                return err;
            }

            if (if_same)
            {
                temp_id[0] = temp_id[1];
                temp_id[1] = fentry->id;
                name += namelen;
                continue;
            }
            else
            {
                err = eHNFFS_ERR_NOENT;
                return err;
            }
        }
        else
        {
            for (int i = 0; i < fentry->num_of_son; i++)
            {
                err = eHNFFS_ename_cmp(eHNFFS, &fentry->son_list[i], name,
                                       namelen, &if_same);
                if (err)
                {
                    return err;
                }

                if (if_same)
                {
                    temp_id[0] = temp_id[1];
                    temp_id[1] = fentry->id;
                    name += namelen;
                    fentry = &fentry->son_list[i];
                    break;
                }
            }

            if (!if_same)
            {
                return eHNFFS_ERR_NOENT;
            }
        }
    }

    if (temp_id[0] == eHNFFS_NULL)
        *father_id = temp_id[1];
    else
        *father_id = temp_id[0];
    *end_path = name;
    *entry = fentry;
    return err;
}

/**
 * Find father of the path by traversing flash started at start,
 * return sector to tell us the position of father dir.
 */
int eHNFFS_father_flash_find(eHNFFS_t *eHNFFS, char *path, eHNFFS_size_t tail,
                             eHNFFS_size_t *sector, eHNFFS_size_t *father_id)
{
    int err = eHNFFS_ERR_OK;

    char *name = path;
    char *father_end = eHNFFS_father_path(name);
    eHNFFS_size_t csector = tail;
    bool if_find = false;
    eHNFFS_size_t temp_id[2] = {eHNFFS_NULL, eHNFFS_NULL};
    while (name != father_end)
    {
        name += strspn(name, "/");
        eHNFFS_size_t namelen = strcspn(name, "/");

        eHNFFS_size_t next = eHNFFS_NULL;
        temp_id[0] = temp_id[1];
        err = eHNFFS_dtraverse_name(eHNFFS, csector, name, namelen,
                                    eHNFFS_DATA_DIR, &if_find, &next, &temp_id[1], NULL, NULL);
        if (err)
        {
            return err;
        }

        if (!if_find)
        {
            return eHNFFS_ERR_NOFATHER;
        }

        csector = next;
    }

    if (temp_id[0] == eHNFFS_NULL)
        *father_id = eHNFFS_NULL;
    else
        *father_id = temp_id[0];
    *sector = csector;
    return eHNFFS_ERR_OK;
}

/**
 * Find father of the path.
 * Because the path may be file(small/big), and the data/index of them
 * are stored in dir, so we should find father first.
 */
int eHNFFS_father_find(eHNFFS_t *eHNFFS, char *path, eHNFFS_size_t *tail,
                       eHNFFS_tree_entry_ram_t **entry, eHNFFS_size_t *father_id)
{
    int err = eHNFFS_ERR_OK;

    // We need a full path, not support . and ..
    char *name = path;
    char *fend = eHNFFS_father_path(name); // father end.

    // If it's same, the path is only the root path, so it has no father,
    // we consider the father is super.
    if (fend == name)
    {
        *father_id = eHNFFS_ID_SUPER;
        return err;
    }

    // Now it maybe it's no use.
    // eHNFFS_tree_entry_ram_t *fentry = eHNFFS->hash_tree->root_hash;
    // bool if_same = false;

    char *epath;                     // end path.
    eHNFFS_tree_entry_ram_t *eentry; // end entry.
    err = eHNFFS_father_tree_find(eHNFFS, path, &epath, &eentry, father_id);
    if (err)
    {
        return err;
    }

    if (fend == epath)
    {
        *tail = eentry->tail;
        *entry = eentry;
        return eHNFFS_ERR_OK;
    }

    // TODO in the future.
    // Because now father dir must in ram, so we just need father id, no father
    // dir's name address(i.e father's father dir).
    err = eHNFFS_father_flash_find(eHNFFS, path, eentry->tail, tail, father_id);
    if (err)
    {
        return err;
    }

    if (*father_id == eHNFFS_NULL)
        *father_id = eentry->id;

    return err;
}

/**
 * Add a new tree entry in-ram.
 * We set changing flag to true, so we can flush it in the future.
 */
int eHNFFS_entry_add(eHNFFS_t *eHNFFS, eHNFFS_tree_entry_ram_t *father,
                     eHNFFS_size_t id, eHNFFS_size_t sector, eHNFFS_size_t off,
                     eHNFFS_size_t tail, char *name, int namelen, bool if_change)
{
    int err = eHNFFS_ERR_OK;

    // If the entry is in the tree, we don't need to create a new one.
    for (int i = 0; i < father->num_of_son; i++)
    {
        if (father->son_list[i].id == id)
        {
            father->son_list[i].sector = sector;
            father->son_list[i].off = off;
            father->son_list[i].tail = tail;
            father->son_list[i].if_change = if_change;
            return err;
        }
    }

    eHNFFS->hash_tree->num++;

    // Malloc space if we need.
    if (father->num_of_son % eHNFFS_HASH_BUCKET_SIZE == 0)
    {
        eHNFFS_size_t size = (father->num_of_son + eHNFFS_HASH_BUCKET_SIZE) *
                             sizeof(eHNFFS_tree_entry_ram_t);
        if (father->num_of_son == 0)
        {
            father->son_list = eHNFFS_malloc(size);
            if (!father->son_list)
            {
                return eHNFFS_ERR_NOMEM;
            }
            memset(father->son_list, 0, size);
        }
        else
        {
            eHNFFS_tree_entry_ram_t *temp = eHNFFS_malloc(size);
            if (!temp)
            {
                return eHNFFS_ERR_NOMEM;
            }
            memset(temp, 0, size);
            memcpy(temp, father->son_list,
                   father->num_of_son * sizeof(eHNFFS_tree_entry_ram_t));

            eHNFFS_free(father->son_list);
            father->son_list = temp;
        }
    }

    // Give basic message.
    father->son_list[father->num_of_son].id = id;
    father->son_list[father->num_of_son].sector = sector;
    father->son_list[father->num_of_son].off = off;
    father->son_list[father->num_of_son].tail = tail;
    father->son_list[father->num_of_son].num_of_son = 0;
    father->son_list[father->num_of_son].son_list = NULL;

    if (namelen <= eHNFFS_ENTRY_NAME_LEN)
    {
        father->son_list[father->num_of_son].data_type = eHNFFS_DATA_TREE_NAME;
        memcpy(father->son_list[father->num_of_son].data.name, name, namelen);
    }
    else
    {
        father->son_list[father->num_of_son].data_type = eHNFFS_DATA_TREE_HASH;
        father->son_list[father->num_of_son].data.hash = eHNFFS_hash((uint8_t *)name, namelen);
    }

    // Turn change flag to true.
    father->son_list[father->num_of_son].if_change = if_change;
    father->num_of_son++;

    return eHNFFS_ERR_OK;
}

/**
 * Get new hash tree sectors in flash.
 */
int eHNFFS_new_hashtree_sector(eHNFFS_t *eHNFFS, eHNFFS_hash_tree_ram_t *tree)
{
    int err = eHNFFS_ERR_OK;

    // Get new sectors.
    eHNFFS_size_t new_begin = eHNFFS_NULL;
    err = eHNFFS_sector_alloc(eHNFFS, eHNFFS->manager, eHNFFS_SECTOR_TREE, eHNFFS_TREE_SECTOR_NUM,
                              eHNFFS_NULL, eHNFFS_NULL, eHNFFS_NULL, &new_begin, NULL);
    if (err)
    {
        return err;
    }

    // Turn state of old sectors to old.
    err = eHNFFS_sector_old(eHNFFS, tree->begin, eHNFFS_TREE_SECTOR_NUM);
    if (err)
    {
        return err;
    }

    // New begin, flush all new hash tree entry to flash.
    tree->begin = new_begin;
    tree->free_off = sizeof(eHNFFS_head_t);
    err = eHNFFS_entry_flush(eHNFFS, tree, eHNFFS_ALL_ENTRY);
    return err;
}

/**
 * Prog a new delete entry.
 * We only prog delete data for tree entry.
 * For dir/file name or dir sector, we can just turn to delete type or old.
 */
int eHNFFS_delete_entry_prog(eHNFFS_t *eHNFFS, eHNFFS_size_t id, eHNFFS_size_t father_id)
{
    int err = eHNFFS_ERR_OK;

    // Make delete data structure.
    eHNFFS_size_t len = sizeof(eHNFFS_delete_entry_flash_t);
    eHNFFS_delete_entry_flash_t dentry = {
        .head = eHNFFS_MKDHEAD(0, 1, id, eHNFFS_DATA_DELETE, len),
        .father_id = father_id,
    };

    // Get true (sector, off) address.
    eHNFFS_size_t sector = eHNFFS->hash_tree->begin;
    eHNFFS_off_t off = eHNFFS->hash_tree->free_off;
    while (off >= eHNFFS->cfg->sector_size)
    {
        sector++;
        off -= eHNFFS->cfg->sector_size;
    }

    // Flash space is full, need to gc.
    if (off + len >= eHNFFS->cfg->sector_size &&
        sector - eHNFFS->hash_tree->begin + 1 == eHNFFS_TREE_SECTOR_NUM)
    {
        err = eHNFFS_new_hashtree_sector(eHNFFS, eHNFFS->hash_tree);
        if (err)
        {
            return err;
        }
    }

    // If current sector doesn't have enough memory, turn to the next one.
    if (off + len >= eHNFFS->cfg->sector_size)
    {
        off = sizeof(eHNFFS_head_t);
        sector++;
        eHNFFS->hash_tree->free_off = off + eHNFFS->cfg->sector_size *
                                                (sector - eHNFFS->hash_tree->begin);
    }

    // Prog it.
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, eHNFFS->pcache, NULL, eHNFFS_VALIDATE,
                            sector, off, &dentry, len);
    if (err)
    {
        return err;
    }
    eHNFFS->hash_tree->free_off += len;
    return err;
}

/**
 * Prog a new hash tree entry to flash.
 */
int eHNFFS_entry_prog(eHNFFS_t *eHNFFS, eHNFFS_tree_entry_ram_t *entry,
                      eHNFFS_size_t father_id)
{
    int err = eHNFFS_ERR_OK;

    eHNFFS_size_t sector;
    eHNFFS_off_t off;

    // TODO, maybe change in the future
    // Now we only has name, no hash.
    int len = sizeof(eHNFFS_tree_entry_flash_t) + strlen((const char *)entry->data.name);
    eHNFFS_tree_entry_flash_t *nentry = eHNFFS_malloc(len);
    if (!nentry)
    {
        return err;
    }

    // Assign basic data.
    nentry->head = eHNFFS_MKDHEAD(0, 1, entry->id, entry->data_type, len);
    nentry->father_id = father_id;
    nentry->sector = entry->sector;
    nentry->off = entry->off;
    nentry->tail = entry->tail;
    if (entry->data_type == eHNFFS_DATA_TREE_HASH)
    {
        memcpy(nentry->data, &entry->data.hash, sizeof(eHNFFS_size_t));
    }
    else if (entry->data_type == eHNFFS_DATA_TREE_NAME)
    {
        memcpy(nentry->data, entry->data.name, strlen((const char *)entry->data.name));
    }
    else
    {
        err = eHNFFS_ERR_INVAL;
        goto cleanup;
    }

    // Get true (sector, off) address.
    sector = eHNFFS->hash_tree->begin;
    off = eHNFFS->hash_tree->free_off;
    while (off >= eHNFFS->cfg->sector_size)
    {
        sector++;
        off -= eHNFFS->cfg->sector_size;
    }

    // Flash space is full, need to gc.
    if (off + len >= eHNFFS->cfg->sector_size &&
        sector - eHNFFS->hash_tree->begin + 1 == eHNFFS_TREE_SECTOR_NUM)
    {
        err = eHNFFS_new_hashtree_sector(eHNFFS, eHNFFS->hash_tree);
        if (err)
        {
            return err;
        }
    }

    // If current sector doesn't have enough memory, turn to the next one.
    if (off + len >= eHNFFS->cfg->sector_size)
    {
        off = sizeof(eHNFFS_head_t);
        sector++;
        eHNFFS->hash_tree->free_off = off + eHNFFS->cfg->sector_size *
                                                (sector - eHNFFS->hash_tree->begin);
    }

    // Prog it.
    err = eHNFFS_cache_prog(eHNFFS, eHNFFS_FORWARD, eHNFFS->pcache, NULL, eHNFFS_VALIDATE,
                            sector, off, nentry, len);
    if (err)
    {
        goto cleanup;
    }

    entry->if_change = false;
    eHNFFS->hash_tree->free_off += len;

cleanup:
    eHNFFS_free(nentry);
    return err;
}

/**
 * Flush all changed hash tree entry to flash.
 */
int eHNFFS_entry_flush(eHNFFS_t *eHNFFS, eHNFFS_hash_tree_ram_t *tree, int type)
{
    int err = eHNFFS_ERR_OK;

    // id array is used to store father id.
    // Because when prog entry, we need its father id.
    eHNFFS_size_t idarr[eHNFFS_TREE_MAXNUM][2];
    int i = 1;
    int index = 0;
    idarr[0][1] = eHNFFS_ID_SUPER;
    idarr[0][0] = 1;

    // Use queue to traverse hash tree.
    eHNFFS_hash_queue_ram_t *queue;
    err = eHNFFS_hash_queue_init(&queue);
    if (err)
    {
        return err;
    }
    eHNFFS_hash_enqueue(queue, tree->root_hash);
    idarr[i][1] = tree->root_hash->id;
    idarr[i][0] = tree->root_hash->num_of_son;
    i++;

    // Begin to traverse.
    eHNFFS_tree_entry_ram_t *temp_entry = NULL;
    while (queue->num > 0)
    {
        temp_entry = eHNFFS_hash_dequeue(queue);
        while (idarr[index][0] == 0)
        {
            index++;
        }
        idarr[index][0]--;

        if ((type == eHNFFS_CHANGED_ENTRY && temp_entry->if_change == true) ||
            type == eHNFFS_ALL_ENTRY)
        {
            err = eHNFFS_entry_prog(eHNFFS, temp_entry, idarr[index][1]);
            if (err)
            {
                goto cleanup;
            }
        }

        for (int p = 0; p < temp_entry->num_of_son; p++)
        {
            eHNFFS_hash_enqueue(queue, &temp_entry->son_list[p]);
            idarr[i][1] = temp_entry->son_list[p].id;
            idarr[i][0] = temp_entry->son_list[p].num_of_son;
            i++;
        }
    }

cleanup:
    eHNFFS_hash_queue_delete(queue);
    return err;
}

/**
 * Delete an entry both in-ram an in-flash.
 */
int eHNFFS_entry_delete(eHNFFS_t *eHNFFS, eHNFFS_tree_entry_ram_t *father_entry, eHNFFS_size_t id)
{
    int err = eHNFFS_ERR_OK;

    // Find the position of id in hash tree.
    eHNFFS_tree_entry_ram_t *entry = NULL;
    int16_t index = -1;
    for (int i = 0; i < father_entry->num_of_son; i++)
    {
        if (father_entry->son_list[i].id == id)
        {
            entry = &father_entry->son_list[i];
            index = i;
            break;
        }
    }

    // If father entry doesn't have it, then it's wrong.
    if (entry == NULL || father_entry->num_of_son == 0)
    {
        return eHNFFS_ERR_NOFATHER;
    }

    // TODO
    /* // Maybe improve in the future.
    // If the dir has son, we can't delete it now.
    if (entry->num_of_son != 0)
        return eHNFFS_ERR_CANTDELETE; */

    // Prog a new delete head.
    err = eHNFFS_delete_entry_prog(eHNFFS, father_entry->son_list[index].id,
                                   father_entry->id);
    if (err)
    {
        return err;
    }

    // remove it in-ram.
    father_entry->num_of_son -= 1;
    memcpy(&father_entry->son_list[index], &father_entry->son_list[index + 1],
           (father_entry->num_of_son - index) * sizeof(eHNFFS_tree_entry_ram_t));
    return err;
}

/**
 * Father dir find function with father id.
 * Type is only eHNFFS_MIGRATE.
 */
int eHNFFS_father_id_find(eHNFFS_t *eHNFFS, eHNFFS_size_t father_id, int type,
                          eHNFFS_dir_ram_t **father_dir)
{
    int err = eHNFFS_ERR_OK;

    if (father_dir == 0)
        return err;

    // Find if dir is in dir list first.
    eHNFFS_dir_ram_t *dir = eHNFFS->dir_list->dir;
    while (dir != NULL)
    {
        if (dir->id == father_id)
        {
            *father_dir = dir;
            return err;
        }
        dir = dir->next_dir;
    }

    // id array is used to store father id.
    // Because when prog entry, we need its father id.
    eHNFFS_size_t idarr[eHNFFS_TREE_MAXNUM][2];
    int i = 1;
    int index = 0;
    idarr[0][1] = eHNFFS_ID_SUPER;
    idarr[0][0] = 1;

    // Init queue for the convenience of traverse hash tree.
    eHNFFS_hash_queue_ram_t *queue;
    err = eHNFFS_hash_queue_init(&queue);
    if (err)
    {
        return err;
    }
    eHNFFS_hash_enqueue(queue, eHNFFS->hash_tree->root_hash);
    idarr[i][1] = eHNFFS->hash_tree->root_hash->id;
    idarr[i][0] = eHNFFS->hash_tree->root_hash->num_of_son;
    i++;

    // Traverse hash tree to find father dir with father id.
    // We assume that father tree entry is in-ram.
    eHNFFS_tree_entry_ram_t *temp_entry;
    while (queue->num > 0)
    {
        // Dequeue.
        temp_entry = eHNFFS_hash_dequeue(queue);
        while (idarr[index][0] == 0)
        {
            index++;
        }
        idarr[index][0]--;

        // Enqueue its son.
        for (int p = 0; p < temp_entry->num_of_son; p++)
        {
            eHNFFS_hash_enqueue(queue, &temp_entry->son_list[p]);
            idarr[i][1] = temp_entry->son_list[p].id;
            idarr[i][0] = temp_entry->son_list[p].num_of_son;
            i++;
        }

        // If current entry is what we find.
        if (temp_entry->id == father_id)
        {
            // Open dir.
            // Because it usually used in migration, so no need to record name len.
            // Namelen is only used in deleted function.
            err = eHNFFS_dir_lowopen(eHNFFS, temp_entry->tail, temp_entry->id, idarr[index][1],
                                     temp_entry->sector, temp_entry->off, eHNFFS_NULL, type, father_dir);
            goto cleanup;
        }
    }

    // If we not find finally, it means that there is no father
    // entry in ram.
    err = eHNFFS_ERR_NOENT;
cleanup:
    eHNFFS_hash_queue_delete(queue);
    return err;
}

/**
 * Father entry find with id(not path) function.
 */
int eHNFFS_entry_idfind(eHNFFS_t *eHNFFS, eHNFFS_size_t father_id,
                        eHNFFS_tree_entry_ram_t **father_entry)
{
    int err = eHNFFS_ERR_OK;

    // id array is used to store father id.
    // Because when prog entry, we need its father id.
    eHNFFS_size_t idarr[eHNFFS_TREE_MAXNUM][2];
    int i = 1;
    int index = 0;
    idarr[0][1] = eHNFFS_ID_SUPER;
    idarr[0][0] = 1;

    // Init queue for the convenience of traverse hash tree.
    eHNFFS_hash_queue_ram_t *queue;
    err = eHNFFS_hash_queue_init(&queue);
    if (err)
    {
        return err;
    }
    eHNFFS_hash_enqueue(queue, eHNFFS->hash_tree->root_hash);
    idarr[i][1] = eHNFFS->hash_tree->root_hash->id;
    idarr[i][0] = eHNFFS->hash_tree->root_hash->num_of_son;
    i++;

    // Traverse hash tree to find father dir with father id.
    // We assume that father tree entry is in-ram.
    eHNFFS_tree_entry_ram_t *temp_entry;
    while (queue->num > 0)
    {
        // Dequeue.
        temp_entry = eHNFFS_hash_dequeue(queue);
        while (idarr[index][0] == 0)
        {
            index++;
        }
        idarr[index][0]--;

        // Enqueue its son.
        for (int p = 0; p < temp_entry->num_of_son; p++)
        {
            eHNFFS_hash_enqueue(queue, &temp_entry->son_list[p]);
            idarr[i][1] = temp_entry->son_list[p].id;
            idarr[i][0] = temp_entry->son_list[p].num_of_son;
            i++;
        }

        // If current entry is what we find, return.
        if (temp_entry->id == father_id)
        {
            *father_entry = temp_entry;
            err = eHNFFS_ERR_OK;
            goto cleanup;
        }
    }

    // If we not find finally, it means that there is no father
    // entry in ram.
    err = eHNFFS_ERR_NOENT;
cleanup:
    eHNFFS_hash_queue_delete(queue);
    return err;
}
