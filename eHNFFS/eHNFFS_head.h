/**
 * The basic head operations of eHNFFS
 */

#ifndef eHNFFS_HEAD_H
#define eHNFFS_HEAD_H

#include "eHNFFS.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * ----------------------------------------------------------    Sector head operations    -------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Make sector head with the following message.
 */
#define eHNFFS_MKSHEAD(valid, state, type, extend, erase_times)                                      \
    (((eHNFFS_head_t)(valid) << 31) | (eHNFFS_head_t)(state) << 27 | ((eHNFFS_head_t)(type) << 24) | \
     ((eHNFFS_head_t)(extend) << 18) | (eHNFFS_head_t)(erase_times))

     bool eHNFFS_shead_novalid(eHNFFS_head_t shead);

     eHNFFS_size_t eHNFFS_shead_state(eHNFFS_head_t shead);

     eHNFFS_size_t eHNFFS_shead_type(eHNFFS_head_t shead);

     eHNFFS_size_t eHNFFS_shead_extend(eHNFFS_head_t shead);

     eHNFFS_size_t eHNFFS_shead_etimes(eHNFFS_head_t shead);

     int eHNFFS_shead_check(eHNFFS_head_t shead, eHNFFS_size_t state, int type);

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * -----------------------------------------------------------    Data head operations    --------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * Make data head with the following message.
 */
#define eHNFFS_MKDHEAD(valid, writen, id, type, len)                                                  \
    (((eHNFFS_head_t)(valid) << 31) | ((eHNFFS_head_t)(writen) << 30) | ((eHNFFS_head_t)(id) << 17) | \
     ((eHNFFS_head_t)(type) << 12) | (eHNFFS_head_t)(len))

    // inline bool eHNFFS_dhead_one(eHNFFS_head_t dhead);

     bool eHNFFS_dhead_novalid(eHNFFS_head_t dhead);

     bool eHNFFS_dhead_nowritten(eHNFFS_head_t dhead);

    // inline bool eHNFFS_dhead_isdelete(eHNFFS_head_t dhead);

     eHNFFS_size_t eHNFFS_dhead_id(eHNFFS_head_t dhead);

     eHNFFS_size_t eHNFFS_dhead_type(eHNFFS_head_t dhead);

    // inline eHNFFS_size_t eHNFFS_dhead_size(eHNFFS_head_t dhead);

     eHNFFS_size_t eHNFFS_dhead_dsize(eHNFFS_head_t dhead);

    // inline eHNFFS_size_t eHNFFS_dhead_none(eHNFFS_head_t dhead);

     int eHNFFS_dhead_check(eHNFFS_head_t dhead, eHNFFS_size_t id, int type);

#ifdef __cplusplus
}
#endif

#endif
