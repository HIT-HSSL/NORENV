/**
 * The basic head operations of eHNFFS
 */

#include "eHNFFS_head.h"

/**
 * -------------------------------------------------------------------------------------------------------
 * -------------------------------------    Sector head operations    ------------------------------------
 * -------------------------------------------------------------------------------------------------------
 */

/**
 * Whether or not the sector head is valid.
 */
 bool eHNFFS_shead_novalid(eHNFFS_head_t shead)
{
    return shead & 0x80000000;
}

/**
 * The state of sector head.
 */
 eHNFFS_size_t eHNFFS_shead_state(eHNFFS_head_t shead)
{
    return (shead & 0x78000000) >> 27;
}

/**
 * The use type of sector head.
 */
 eHNFFS_size_t eHNFFS_shead_type(eHNFFS_head_t shead)
{
    return (shead & 0x07000000) >> 24;
}

/**
 * The extend message of sector head.
 * For superblock, it's version number.
 */
 eHNFFS_size_t eHNFFS_shead_extend(eHNFFS_head_t shead)
{
    return (shead & 0x00fc0000) >> 18;
}

/**
 * The erase times of sector head.
 */
 eHNFFS_size_t eHNFFS_shead_etimes(eHNFFS_head_t shead)
{
    return shead & 0x0003ffff;
}

 int eHNFFS_shead_check(eHNFFS_head_t shead, eHNFFS_size_t state, int type)
{
    int err = eHNFFS_ERR_OK;

    if (shead == eHNFFS_NULL)
    {
        return err;
    }

    if (eHNFFS_shead_novalid(shead))
    {
        err = eHNFFS_ERR_WRONGHEAD;
        return err;
    }

    if (state != eHNFFS_NULL && eHNFFS_shead_state(shead) != state)
    {
        err = eHNFFS_ERR_WRONGHEAD;
        return err;
    }

    if (type != eHNFFS_NULL && eHNFFS_shead_type(shead) != type)
    {
        err = eHNFFS_ERR_WRONGHEAD;
        return err;
    }

    return err;
}

/**
 * -------------------------------------------------------------------------------------------------------
 * --------------------------------------    Data head operations    -------------------------------------
 * -------------------------------------------------------------------------------------------------------
 */

/**
 * If all bits in dhead is 1, then the rest space is no used;
 */
/* inline bool eHNFFS_dhead_one(eHNFFS_head_t dhead)
{
    return dhead == (eHNFFS_head_t)~0;
}
 */

/**
 * Whether or not the data head is valid.
 */
 bool eHNFFS_dhead_novalid(eHNFFS_head_t dhead)
{
    return dhead & 0x80000000;
}

/**
 * Whether or not the data head and data behind is finished.
 */
 bool eHNFFS_dhead_nowritten(eHNFFS_head_t dhead)
{
    return dhead & 0x40000000;
}

/**
 * If the low 10 bits of the data head is 0x3ff(i.e signed -1),
 * it means that the data head is deleted(also the corresponding file/dir with the same id)
 */
/* inline bool eHNFFS_dhead_isdelete(eHNFFS_head_t dhead)
{
    return ((int32_t)(dhead << 22) >> 22) == -1;
} */

/**
 * Get the id that the data head belongs to.
 */
 eHNFFS_size_t eHNFFS_dhead_id(eHNFFS_head_t dhead)
{
    return (dhead & 0x3ffe0000) >> 17;
}

/**
 * Get the type of data head and know what it's used to do.
 */
 eHNFFS_size_t eHNFFS_dhead_type(eHNFFS_head_t dhead)
{
    return (dhead & 0x0001f000) >> 12;
}

/**
 * Get the size of data following the data head. 0x3ff means deleted.
 */
/* inline eHNFFS_size_t eHNFFS_dhead_size(eHNFFS_head_t dhead)
{
    return dhead & 0x000003ff;
} */

/**
 * Get the total size of dhead + following data.
 * Notice that dhead + eHNFFS_dhead_isdelete(dhead) = 0x3ff + 1 = 0 when deleted.
 */
 eHNFFS_size_t eHNFFS_dhead_dsize(eHNFFS_head_t dhead)
{
    return dhead & 0x00000fff;
}

/**
 * Checkout if dhead is all bit 1, i.e nothing have writen.
 */
/* inline eHNFFS_size_t eHNFFS_dhead_none(eHNFFS_head_t dhead)
{
    return dhead == 0xffffffff;
} */

 int eHNFFS_dhead_check(eHNFFS_head_t dhead, eHNFFS_size_t id, int type)
{
    int err = eHNFFS_ERR_OK;

    if (dhead == eHNFFS_NULL)
        return err;

    if (eHNFFS_dhead_novalid(dhead) || eHNFFS_dhead_nowritten(dhead))
    {
        err = eHNFFS_ERR_WRONGHEAD;
        return err;
    }

    if (id != eHNFFS_NULL && eHNFFS_dhead_id(dhead) != id)
    {
        err = eHNFFS_ERR_WRONGHEAD;
        return err;
    }

    if (type != eHNFFS_NULL && eHNFFS_dhead_type(dhead) != type)
    {
        err = eHNFFS_ERR_WRONGHEAD;
        return err;
    }

    return err;
}
