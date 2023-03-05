/**
 * Big related related operations.
 */
#ifndef eHNFFS_FILE_H
#define eHNFFS_FILE_H

#include "eHNFFS.h"

#ifdef __cplusplus
extern "C"
{
#endif

    int eHNFFS_file_list_initialization(eHNFFS_t *eHNFFS, eHNFFS_file_list_ram_t **list_addr);

    void eHNFFS_file_list_free(eHNFFS_t *eHNFFS, eHNFFS_file_list_ram_t *list);

    int eHNFFS_file_free(eHNFFS_file_list_ram_t *list, eHNFFS_file_ram_t *file);

    int eHNFFS_bfile_prog(eHNFFS_t *eHNFFS, eHNFFS_size_t *sector, eHNFFS_off_t *off,
                          const void *buffer, eHNFFS_size_t len);

    int eHNFFS_ftraverse_gc(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file, eHNFFS_size_t num);

    int eHNFFS_bfile_part_gc(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file, eHNFFS_size_t start,
                             eHNFFS_size_t end, eHNFFS_size_t len, eHNFFS_size_t index_num,
                             eHNFFS_size_t sector_num);

    int eHNFFS_file_lowopen(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_size_t id,
                            eHNFFS_size_t sector, eHNFFS_off_t off, eHNFFS_size_t namelen,
                            eHNFFS_file_ram_t **file_addr, int type);

    int eHNFFS_file_flush(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file);

    int eHNFFS_file_lowclose(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file);

    int eHNFFS_create_file(eHNFFS_t *eHNFFS, eHNFFS_dir_ram_t *dir, eHNFFS_file_ram_t **file_addr,
                           char *name, eHNFFS_size_t namelen);

    int eHNFFS_part_ftraverse_gc(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file);

    int eHNFFS_small_file_read(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file, void *buffer,
                               eHNFFS_size_t size);

    int eHNFFS_big_file_read(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file, void *buffer,
                             eHNFFS_size_t size);

    int eHNFFS_small_file_write(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                                const void *buffer, eHNFFS_size_t size);

    int eHNFFS_s2b_file_write(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file,
                              const void *buffer, eHNFFS_size_t size);

    void eHNFFS_index_jump(eHNFFS_t *eHNFFS, eHNFFS_bfile_index_ram_t *index, eHNFFS_size_t jump_size);

    int eHNFFS_big_file_write(eHNFFS_t *eHNFFS, eHNFFS_file_ram_t *file, void *buffer,
                              eHNFFS_size_t size);

#ifdef __cplusplus
}
#endif

#endif
