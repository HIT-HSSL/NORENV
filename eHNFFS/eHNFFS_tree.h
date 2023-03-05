/**
 * Hash tree related operations.
 */
#ifndef eHNFFS_TREE_H
#define eHNFFS_TREE_H

#include "eHNFFS.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ----------------------------------------------------------    Hash queue operations    --------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */
    int eHNFFS_hash_queue_init(eHNFFS_hash_queue_ram_t **queue_addr);

    void eHNFFS_hash_queue_delete(eHNFFS_hash_queue_ram_t *queue);

    void eHNFFS_hash_enqueue(eHNFFS_hash_queue_ram_t *queue, eHNFFS_tree_entry_ram_t *entry);

    eHNFFS_tree_entry_ram_t *eHNFFS_hash_dequeue(eHNFFS_hash_queue_ram_t *queue);

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ------------------------------------------------------------    Hash calculation    -----------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */
    eHNFFS_size_t eHNFFS_hash(uint8_t *buffer, eHNFFS_size_t len);

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ----------------------------------------------------------    Hash tree operations    ---------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_hash_tree_initialization(eHNFFS_t *eHNFFS, eHNFFS_hash_tree_ram_t **tree_addr);

    int eHNFFS_hash_tree_free(eHNFFS_tree_entry_ram_t *tree_entry);

    void eHNFFS_entry_initialize(eHNFFS_tree_entry_ram_t *tree_entry);

    void eHNFFS_ram_entry_init(eHNFFS_tree_entry_ram_t *ram_entry, eHNFFS_tree_entry_flash_t *flash_entry);

    int eHNFFS_construct_tree(eHNFFS_t *eHNFFS, eHNFFS_hash_tree_ram_t *hash_tree, eHNFFS_cache_ram_t *cache);

    int eHNFFS_father_find(eHNFFS_t *eHNFFS, char *path, eHNFFS_size_t *tail,
                           eHNFFS_tree_entry_ram_t **entry, eHNFFS_size_t *father_id);

    int eHNFFS_entry_add(eHNFFS_t *eHNFFS, eHNFFS_tree_entry_ram_t *father,
                         eHNFFS_size_t id, eHNFFS_size_t sector, eHNFFS_size_t off,
                         eHNFFS_size_t tail, char *name, int namelen, bool if_change);

    int eHNFFS_new_hashtree_sector(eHNFFS_t *eHNFFS, eHNFFS_hash_tree_ram_t *tree);

    int eHNFFS_entry_prog(eHNFFS_t *eHNFFS, eHNFFS_tree_entry_ram_t *entry,
                          eHNFFS_size_t father_id);

    int eHNFFS_entry_flush(eHNFFS_t *eHNFFS, eHNFFS_hash_tree_ram_t *tree, int type);

    int eHNFFS_entry_delete(eHNFFS_t *eHNFFS, eHNFFS_tree_entry_ram_t *father_entry, eHNFFS_size_t id);

    int eHNFFS_father_id_find(eHNFFS_t *eHNFFS, eHNFFS_size_t father_id, int type,
                              eHNFFS_dir_ram_t **father_dir);

    int eHNFFS_entry_idfind(eHNFFS_t *eHNFFS, eHNFFS_size_t father_id,
                            eHNFFS_tree_entry_ram_t **father_entry);

    char *eHNFFS_father_path(char *path);

#ifdef __cplusplus
}
#endif

#endif
