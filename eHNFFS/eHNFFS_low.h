/**
 * The low level operations of eHNFFS
 */

#ifndef eHNFFS_LOW_H
#define eHNFFS_LOW_H

#include "eHNFFS.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * -------------------------------------------------------------------------------------------------------
     * --------------------------------    read/prg/erase related operations    ------------------------------
     * -------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_sync(eHNFFS_t *eHNFFS, eHNFFS_cache_ram_t *pcache,
                    eHNFFS_cache_ram_t *rcache, bool validate);

    int eHNFFS_erase(eHNFFS_t *eHNFFS, eHNFFS_size_t sector);

    /**
     * -------------------------------------------------------------------------------------------------------
     * ----------------------------------    Initialize related operations    --------------------------------
     * -------------------------------------------------------------------------------------------------------
     */

    int eHNFFS_init(eHNFFS_t *eHNFFS, const struct eHNFFS_config *cfg);

    int eHNFFS_deinit(eHNFFS_t *eHNFFS);

    int eHNFFS_select_supersector(eHNFFS_t *eHNFFS, eHNFFS_size_t *sector);

    eHNFFS_ssize_t eHNFFS_valid_num(eHNFFS_t *eHNFFS, eHNFFS_size_t sector, eHNFFS_off_t off,
                                    eHNFFS_ssize_t map_len, eHNFFS_cache_ram_t *rcache);

#ifdef __cplusplus
}
#endif

#endif
