#ifndef eHNFFS_H
#define eHNFFS_H

#include <stdint.h>
#include <stdbool.h>
#include "eHNFFS_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * --------------------------------------------------------------    Version info    -------------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * The version of eHNFFS.
 *  1. Major (the top 16 bits), incremented on backwards incompatible changes
 *  2. Minor (the bottom 16 bits), incremented on feature additions
 */
#define eHNFFS_VERSION 0x00010000
#define eHNFFS_VERSION_MAJOR (0xffff & (eHNFFS_VERSION >> 16))
#define eHNFFS_VERSION_MINOR (0xffff & (eHNFFS_VERSION >> 0))

/**
 * The version of data on disk.
 *  1. Major (top-nibble), incremented on backwards incompatible changes
 *  2. Minor (bottom-nibble), incremented on feature additions
 */
#define eHNFFS_DISK_VERSION 0x00010000
#define eHNFFS_DISK_VERSION_MAJOR (0xffff & (eHNFFS_DISK_VERSION >> 16))
#define eHNFFS_DISK_VERSION_MINOR (0xffff & (eHNFFS_DISK_VERSION >> 0))

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * -----------------------------------------------------------    Redefine data type    ----------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    /**
     * The basic type used in eHNFFS.
     *
     *  1.  Size means read/prog/cache/sector/name/... size, i.e all size used in eHNFFS
     *  2.  Off means the address in a sector and other offset.
     *  3.  Head is the basic structure that describes information of data behind.
     *  4.  Hash is used in name resolution when the name is too long.
     *
     * S in ssize(soff) means signed, so the data could be negative.
     */
    typedef uint32_t eHNFFS_size_t;
    typedef int32_t eHNFFS_ssize_t;
    typedef uint32_t eHNFFS_off_t;
    typedef int32_t eHNFFS_soff_t;
    typedef uint32_t eHNFFS_head_t;
    typedef uint32_t eHNFFS_hash_t;

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * -----------------------------------------------------    Defination of some basic value    ----------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/**
 * The name of the filesystem
 */
#ifndef eHNFFS_FS_NAME
#define eHNFFS_FS_NAME "eHNFFS"
#endif

/**
 * The magic number of eHNFFS.
 */
#ifndef eHNFFS_MAGIC
#define eHNFFS_MAGIC (eHNFFS_size_t)0x3627fae5
#endif

/**
 * It indicates that the sector/off/head.... is no use.
 */
#ifndef eHNFFS_NULL
// #define eHNFFS_NULL (eHNFFS_size_t)(-1)
#define eHNFFS_NULL 0xffffffff
#endif

/**
 * Maximum name size in bytes
 * The configuration of users' shouldn't large than it.
 */
#ifndef eHNFFS_NAME_MAX
#define eHNFFS_NAME_MAX 255
#endif

/**
 * Maximum size of a file in bytes
 *
 * The configuration of users' shouldn't large than it(32MB).
 */
#ifndef eHNFFS_FILE_MAX_SIZE
#define eHNFFS_FILE_MAX_SIZE 33554432
#endif

/**
 * Maximum size of id used in eHNFFS
 *
 * All files/dir stored in eHNFFS need a unique id to identify, and eHNFFS_ID_MAX shows
 * the max size of it. The following id used specially:
 *  1. ID 0: Used for superblock message.
 *  2. ID 1: Used for root dir.
 */
#ifndef eHNFFS_ID_MAX
#define eHNFFS_ID_MAX 8192
#endif

/**
 * The beginning of weal leveling when scan time of sector reach eHNFFS_WL_START.
 *
 * It's worth noticed that we don't do wl at first for the convenience of management and
 * the high performance.
 */
#ifndef eHNFFS_WL_START
#define eHNFFS_WL_START 30000
#endif

#ifndef eHNFFS_WL_ING
#define eHNFFS_WL_ING 50000
#endif

/**
 * The max number of file we could open at a time in ram.
 */
#ifndef eHNFFS_FILE_LIST_MAX
#define eHNFFS_FILE_LIST_MAX 5
#endif

/**
 * The max number of dir we could store at a time in ram.
 */
#ifndef eHNFFS_DIR_LIST_MAX
#define eHNFFS_DIR_LIST_MAX 10
#endif

/**
 * The max number of regions.
 */
#ifndef eHNFFS_REGION_NUM_MAX
#define eHNFFS_REGION_NUM_MAX 128
#endif

/**
 * The number of candidate regions we store in ram.
 * It's only used when wl starts.
 */
#ifndef eHNFFS_RAM_REGION_NUM
#define eHNFFS_RAM_REGION_NUM 4
#endif

/**
 * Used for name resolution, for the convenience of constructing tree.
 */
#ifndef eHNFFS_HASH_BUCKET_SIZE
#define eHNFFS_HASH_BUCKET_SIZE 5
#endif

/**
 * The sequential number of sectors allocated to hash tree entry.
 */
#ifndef eHNFFS_TREE_SECTOR_NUM
#define eHNFFS_TREE_SECTOR_NUM 2
#endif

/**
 * The sequential number of sectors allocated to wl array and wl added message.
 */
#ifndef eHNFFS_WL_SECTOR_NUM
#define eHNFFS_WL_SECTOR_NUM 3
#endif

/**
 * The length of space reserved for name in hash tree entry.
 */
#ifndef eHNFFS_ENTRY_NAME_LEN
#define eHNFFS_ENTRY_NAME_LEN 8
#endif

#ifndef eHNFFS_WRITTEN_FLAG_SET
#define eHNFFS_WRITTEN_FLAG_SET 0xbfffffff
#endif

#ifndef eHNFFS_DELETE_TYPE_SET
#define eHNFFS_DELETE_TYPE_SET 0xfffe0fff
#endif

#ifndef eHNFFS_OLD_SECTOR_SET
#define eHNFFS_OLD_SECTOR_SET 0x87ffffff
#endif

#ifndef eHNFFS_SIZE_T_NUM
#define eHNFFS_SIZE_T_NUM 32
#endif

#ifndef eHNFFS_ID_REGIONS
#define eHNFFS_ID_REGIONS 128
#endif

#ifndef eHNFFS_TREE_MAXNUM
#define eHNFFS_TREE_MAXNUM 20
#endif

#ifndef eHNFFS_FILE_CACHE_SIZE
#define eHNFFS_FILE_CACHE_SIZE 512
#endif

#ifndef eHNFFS_FILE_INDEX_LEN
#define eHNFFS_FILE_INDEX_LEN 41
#endif

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ---------------------------------------------------------------    Enum type    ---------------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    /**
     * Possible error types.
     *  1. 0 means no error.
     *  2. Negative data could tell us what's happening.
     */
    enum eHNFFS_error
    {
        eHNFFS_ERR_OK = 0,     // No error
        eHNFFS_ERR_IO = -5,    // Error during device operation
        eHNFFS_ERR_NOSPC = -6, // No space left on device
        eHNFFS_ERR_NOMEM = -7, // No more memory available

        eHNFFS_ERR_NOID = -20,        // No more id to use.
        eHNFFS_ERR_NAMETOOLONG = -21, // File name too long
        eHNFFS_ERR_MUCHOPEN = -22,    // The opened file/dir is too much.
        eHNFFS_ERR_NOFATHER = -23,    // Don't have father dir.
        eHNFFS_ERR_NOENT = -24,       // No directory entry
        eHNFFS_ERR_EXIST = -25,       // Entry already exists
        eHNFFS_ERR_NOTDIR = -26,      // Entry is not a dir
        eHNFFS_ERR_ISDIR = -27,       // Entry is a dir
        eHNFFS_ERR_NOTEMPTY = -28,    // Dir is not empty
        eHNFFS_ERR_BADF = -29,        // Bad file number
        eHNFFS_ERR_FBIG = -30,        // File too large
        eHNFFS_ERR_INVAL = -31,       // Invalid parameter
        eHNFFS_ERR_NODIROPEN = -32,   // Dir is not opened
        eHNFFS_ERR_NOFILEOPEN = -33,  // File is not opened

        eHNFFS_ERR_CORRUPT = -50,   // Corrupted has happened, and the read data is wrong.
        eHNFFS_ERR_WRONGCAL = -51,  // Calculation has something wrong.
        eHNFFS_ERR_WRONGCFG = -52,  // Cfg message is not the same as what stored in nor flash.
        eHNFFS_ERR_WRONGHEAD = -53, // Has a wrong head.
        eHNFFS_ERR_WRONGPROG = -54, // What we prog is wrong.
        eHNFFS_ERR_NOTINLIST = -55, // Not find in the list.

        eHNFFS_ERR_DIRHASH = -56,    // It's not true error, but not we think it is for there is no
                                     // hash we use now.
        eHNFFS_ERR_CANTDELETE = -57, // Now we can't delete on structure if it still has son.
    };

    /**
     * Read/Prog method of a sector.
     * From beginning to the end or from the end to the beginning.
     * For sector belongs to dir, we store name at the beginning and
     * data at the end for efficiently traversing the dir.
     */
    enum
    {
        eHNFFS_FORWARD = 0,
        eHNFFS_BACKWARD = 1,
    };

    /**
     * The valid bit in flash.
     *
     * The origin bit in nor flash is 1, and we can turn it to 0 when program. So the valid
     * bit in flash is 0, which is contrary to the valid bit in ram(1).
     */
    enum
    {
        eHNFFS_FLASH_VALID = 0,
        eHNFFS_FLASH_NOTVALID = 1,
    };

    /**
     * The bit in sector map that tell us whether or not sector is valid.
     *
     * In free map, 1 indicates that the sector is free and it turns to 0 when we use.
     * In erase map, 1 indicates that the sector is used and it turn to 0 when it can be reused.
     */
    enum
    {
        eHNFFS_FREEMAP_VALID = 0,
        eHNFFS_ERASEMAP_VALID = 1,
    };

    /**
     * After flushing data to flash, whether or not we should read it and compare
     * to validata what we prog is right.
     */
    enum
    {
        eHNFFS_NOTVALIDATE = 0,
        eHNFFS_VALIDATE = 1,
    };

    /**
     * The specific use of id.
     */
    enum
    {
        eHNFFS_ID_SUPER = 0,
        eHNFFS_ID_ROOT = 1,
    };

    /**
     * The open type of dir and file.
     */
    enum
    {
        eHNFFS_USER_OPEN = 23,
        eHNFFS_MIGRATE = 54,
    };

    /**
     * Sector head for all sectors in nor flash.
     *
     *      [----            32             ----]
     *      [1|- 4 -|- 3 -|--  6  --|--  18   --]
     *      ^    ^     ^       ^          ^- Erase times
     *      |    |     |       '------------ Extend message, depends on type of sector.
     *      |    |     '-------------------- Type of sector.
     *      |    '-------------------------- State of Sector.
     *      '------------------------------- Valid bit, is eHNFFS_VLAID_FLASH(0).
     *
     *  1. Valid bit(1 bit): Tell us the head is valid.
     *
     *  2. State(4 bits):
     *
     *      1) free:   0xf
     *      2) gc-ing: 0x7, tell us that current sector now is used for data migration diromg gc
     *      3) ready:  0x3, when gc is done and there still remain space(may no need)
     *      4) using:  0x1, the current sector is still using
     *      5) old:    0x0, the sector in no need, could be gc.
     *
     *    By turning 1 to 0, we could change the state of sector. There are two typical changes
     *
     *      1) free -> using -> old.
     *      2) free -> gc-ing -> ready -> using -> old.
     *
     *    Notice that free and ready is the same state.
     *  3. Type(3 bits):
     *
     *      1) super:   0x0, used to store the superblock message, always sector 0 and sector 1.
     *      2) dir:     0x1, used to store dir, small file(like 4B data) of dir is also in it
     *      3) big file:0x2, used to store big file, which is always larger than 1KB(or other size).
     *
     *    Though superblock means sector 0 and 1, we still need to use other sectors, like:
     *
     *      1) id map:      Don't have head, be pointed by pointer in sector 0/1
     *                      If has and map just occupy a sector, because of it, we can't store map in a sector.
     *      2) sector map:  The same as above.
     *      3) dir hash:    Used for name resolution, has head and is pointed by pointer in sector 0/1.
     *
     *  4. Extend message(6 bits):
     *
     *      1) super:   For sector 0 and 1, storing version number. When corrupt happens during gc of
     *                  superblock, it helps us to judge which secter(0 or 1) is now useful,
     *                  i.e choosing the smaller one and then continuing gc. We don't use 0x3f, when
     *                  one sector is 0x3e and the other is 0x00, we think 0x3e is smaller than 0x00.
     *                  For dir hash, store 0x3f to express it is used to store dir hash.
     *      2) dir:     Now no use.
     *      3) big file:Now no use.
     *
     *  5. Erase times(18 bits): Erase time of current sector.
     */

    /**
     * The possible state of sector.
     * The head of eHNFFS_SECTOR_FREE could be 0xffffffff(When first access the sector).
     */
    enum eHNFFS_sector_state
    {
        eHNFFS_SECTOR_FREE = 0xf,
        eHNFFS_SECTOR_GC = 0X7,
        eHNFFS_SECTOR_READY = 0X3,
        eHNFFS_SECTOR_USING = 0X1,
        eHNFFS_SECTOR_OLD = 0X0,
    };

    /**
     * The possible type of sector
     */
    enum eHNFFS_sector_type
    {
        eHNFFS_SECTOR_SUPER = 0x0,
        eHNFFS_SECTOR_TREE = 0x1,
        eHNFFS_SECTOR_WL = 0x2,
        eHNFFS_SECTOR_DIR = 0x3,
        eHNFFS_SECTOR_BFILE = 0x4,
        eHNFFS_SECTOR_MAP = 0x5,

        // Only a in-ram flag for reserve region.
        eHNFFS_SECTOR_RESERVE = 0x7,
        eHNFFS_SECTOR_NOTSURE = 0xf,
    };

    /**
     * Data head for all datas stored in nor flash.
     *
     *      [----            32             ----]
     *      [1|1|--   13   --|- 5 -|--   12   --]
     *      ^ ^        ^        ^         ^- Length of data behind
     *      | |        |        '----------- Type of data
     *      | |        '-------------------- ID of data
     *      | '----------------------------  Writen flag
     *      '------------------------------  Valid bit, is eHNFFS_VLAID_FLASH(0).
     *
     *  1. Valid bit(1 bit):   Tell us the head is valid.
     *
     *  2. Writen flag(1 bit): Turned to 0 if we have already writen data head and data behind.
     *
     *  3. ID(14 bits):        A unique idenfication of file/dir/superblock
     *
     *  4. Type(6 bits):
     *
     *      1) eHNFFS_DATA_DIR_NAME:     The name of dir, in the front of the dir sector.
     *      2) eHNFFS_DATA_FILE_NAME:    The name of file.
     *      3) eHNFFS_DATA_DIR_ID：      The id of sub dir in father dir.
     *      4) eHNFFS_DATA_BIG_FILE:     The index of big file, real data is stored in another sector.
     *      5) eHNFFS_DATA_SMALL_FILE:   The data of small file, data is just behind the head.
     *      6) eHNFFS_DATA_DELETE:       The file/dir has been deleted, all data belongs to it should turn to this.
     *
     *      7) eHNFFS_DATA_WL:           The message about WL, construed when we loop the sector map more than a number.
     *      8) eHNFFS_DATA_DIR_HASH:     Helping us achiving name resolution quickly to find out the id of dir.
     *                                   Only contains dir, no file, could be considered as dir index structure.
     *      9) eHNFFS_DATA_SECTOR_MAP:   Tell us which sector could be used: free, ready and old. Old needs erase.
     *      10)eHNFFS_DATA_ERASE_MAP:    Tell us which sector could be erased. When a loop of sector map finished,
     *                                   can be the next sector map(just turn 0x33 to 0x31, i.e a bit 1 to bit 0).
     *      11)eHNFFS_DATA_ID_MAP:       Tell us which id could be used.
     *      12)eHNFFS_DATA_REMOVE_MAP:   Tell us which file/dir is removed, so we could reuse the id. When a loop of
     *                                   id map finished, can be the next id map(similar to erase map).
     *
     *      13)eHNFFS_DATA_SUPER_MESSAGE:Basic message stored in sector 0/1, and it's in the front of it.
     *      14)eHNFFS_DATA_MOUNT_MESSAGE:Stored message like where in sector(id) map we are scaning now.
     *      15)eHNFFS_DATA_MAGIC:        Tell us whether or not corrupt happens. When mount, the data behind
     *                                   turned to 0; When umount, write a new one.
     *
     *  5. Length(10 bits):    Length of data behind, 0x3ff is not used and sometimes means something.
     */

    /**
     * The possible type of data.
     */
    enum eHNFFS_data_type
    {
        eHNFFS_DATA_REG = 0x55,
        eHNFFS_DATA_DIR = 0x22,

        eHNFFS_DATA_FREE = 0x1f,
        eHNFFS_DATA_SUPER_MESSAGE = 0X1e,
        eHNFFS_DATA_COMMIT = 0X1d,
        eHNFFS_DATA_MAGIC = 0X1c,

        eHNFFS_DATA_SECTOR_MAP = 0x19,
        eHNFFS_DATA_ID_MAP = 0x18,
        eHNFFS_DATA_REGION_MAP = 0x17,
        eHNFFS_DATA_WL_ADDR = 0X16,
        eHNFFS_DATA_TREE_ADDR = 0x15,

        eHNFFS_DATA_WL_ARRAY = 0x14,
        eHNFFS_DATA_WL_ADD = 0x13,
        eHNFFS_DATA_TREE_HASH = 0X12, // When name is larger than 8 bytes, store hash
        eHNFFS_DATA_TREE_NAME = 0x11, // When name is smaller than 8 bytes, store name

        eHNFFS_DATA_DIR_NAME = 0x0e,
        eHNFFS_DATA_FILE_NAME = 0x0c,
        eHNFFS_DATA_BFILE_INDEX = 0x0b,
        eHNFFS_DATA_SFILE_DATA = 0x0a,
        eHNFFS_DATA_SKIP = 0x09, // For dir sector, between name and data.

        eHNFFS_DATA_DELETE = 0x00,
    };

    /**
     * File seek flags, used in eHNFFS_seek function.
     */
    enum eHNFFS_whence_flags
    {
        eHNFFS_SEEK_SET = 0, // Seek relative to an absolute position
        eHNFFS_SEEK_CUR = 1, // Seek relative to the current file position
        eHNFFS_SEEK_END = 2, // Seek relative to the end of the file
    };

    /**
     * The comparasion result, i.e equal, less, greater.
     */
    enum
    {
        eHNFFS_CMP_EQ = 0,
        eHNFFS_CMP_LT = 1,
        eHNFFS_CMP_GT = 2,
    };

    /**
     * In hash tree, if name is too long, we use hash instead.
     */
    enum
    {
        eHNFFS_TREE_HASH = 0,
        eHNFFS_TREE_NAME = 1,
    };

    /**
     * The type of flushing hash tree to flash.
     *  1. eHNFFS_CHANGED_ENTRY only flush changed entry to flash.
     *  2. eHNFFS_ALL_ENTRY flush all entrys to flash, used when we
     *     should find some new sectors in flash to stroe these.
     */
    enum
    {
        eHNFFS_CHANGED_ENTRY = 0,
        eHNFFS_ALL_ENTRY = 1,
    };

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ----------------------------------------------------------    Configure structure    ----------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    /**
     * Basic configure message provided by user during mount.
     */
    typedef struct eHNFFS_config
    {
        // Read data in (sector, off) to buffer.
        // Negative error codes are propagated to user, also for the following functions.
        int (*read)(const struct eHNFFS_config *c, eHNFFS_size_t sector,
                    eHNFFS_off_t off, void *buffer, eHNFFS_size_t size);

        // write(program) data in (sector, off) to buffer.
        // May return eHNFFS_ERR_CORRUPT if the sector should be considered bad.
        int (*prog)(const struct eHNFFS_config *c, eHNFFS_size_t sector,
                    eHNFFS_off_t off, void *buffer, eHNFFS_size_t size);

        // Erase a sector.
        // A sector must be erased before being programmed.
        // May return eHNFFS_ERR_CORRUPT if the sector should be considered bad.
        int (*erase)(const struct eHNFFS_config *c, eHNFFS_size_t sector);

        // Sync the state of the underlying block device.
        int (*sync)(const struct eHNFFS_config *c);

#ifdef eHNFFS_THREADSAFE
        // Lock the underlying sector device.
        int (*lock)(const struct eHNFFS_config *c);

        // Unlock the underlying sector device.
        int (*unlock)(const struct eHNFFS_config *c);
#endif

        // Minimum size of a sector read in bytes.
        // All read operations will be a multiple of this value.
        eHNFFS_size_t read_size;

        // Minimum size of a sector program in bytes.
        // All program operations will be a multiple of this value.
        eHNFFS_size_t prog_size;

        // Size of an erasable sector in bytes.
        // This does not impact ram consumption and may be larger than the physical erase size.
        // However, non-inlined files take up at minimum one sector. Must be a multiple of the
        // read and program sizes.
        eHNFFS_size_t sector_size;

        // Number of erasable sectors on the device.
        eHNFFS_size_t sector_count;

        // Number of erase cycles before littlefs evicts metadata logs and moves
        // the metadata to another sector. Suggested values are in the
        // range 100-1000, with large values having better performance at the cost
        // of less consistent wear distribution.
        // Set to -1 to disable sector-level wear-leveling.
        // ----how to use? may not needed----
        // eHNFFS_ssize_t sector_cycles;

        // Size of sector caches in bytes. Each cache buffers a portion of a sector in
        // RAM.Larger caches can improve performance by storing moredata and reducing
        // the number of disk accesses. Must be a multiple of theread and program
        // sizes, and a factor of the sector size.
        eHNFFS_size_t cache_size;

        // We split Sectors into several regions. Every time we choose a region with
        // least P/E to guarantee WL, and then scan the region to build a bit map in DRAM
        eHNFFS_size_t region_cnt;

        // Optional upper limit on length of file names in bytes. No downside for
        // larger names except the size of the info struct which is controlled by
        // the eHNFFS_NAME_MAX define. Defaults to eHNFFS_NAME_MAX when zero. Stored in
        // superblock and must be respected by other littlefs drivers.
        eHNFFS_size_t name_max;

        // Optional upper limit on files in bytes. No downside for larger files
        // but must be <= eHNFFS_FILE_MAX. Defaults to eHNFFS_FILE_MAX when zero. Stored
        // in superblock and must be respected by other littlefs drivers.
        eHNFFS_size_t file_max;

        // when writing new file or something in a dir, the old one is no need, and
        // when current sector is full, we should judge if compact or allocate a new
        // sector, so the dir_change_cnt could be used as reference
        // eHNFFS_size_t dir_change_cnt;

        // the name of root dir
        uint8_t root_dir_name[20];
    } eHNFFS_config_t;

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ----------------------------------------------------------    Sector head structure    --------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    /**
     * The common head structure for all sectors belong to dir.
     *  1. head describes the basic message of sector.
     *  2. pre_sector links sectors if they belong to the same dir.
     */
    typedef struct eHNFFS_dir_sector_flash
    {
        eHNFFS_head_t head;
        eHNFFS_size_t pre_sector;
        eHNFFS_size_t id;
    } eHNFFS_dir_sector_flash_t;

    /**
     * The common head structure for all sectors belong to big file.
     * We only need a head, because these sectors will be organized by a file index structure.
     */
    typedef struct eHNFFS_bfile_sector_flash
    {
        eHNFFS_head_t head;
        eHNFFS_size_t id;
        eHNFFS_size_t father_id;
    } eHNFFS_bfile_sector_flash_t;

    /**
     * The common head structure for sectors belong to all types of super message.
     * Similar to big file sector.
     */
    typedef struct eHNFFS_super_sector_flash
    {
        eHNFFS_head_t head;
    } eHNFFS_super_sector_flash_t;

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ----------------------------------------------------------    Superblock structure    ---------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    /**
     * The in ram structure that describes the baisc information of superblock that used.
     */
    typedef struct eHNFFS_superblock_ram
    {
        // eHNFFS_size_t id;
        eHNFFS_size_t sector;
        eHNFFS_off_t free_off;
        eHNFFS_off_t commit_off;
    } eHNFFS_superblock_ram_t;

    /**
     * The basic message about eHNFFS.
     *
     *  1. fs_name stores "eHNFFS", tells us if nor flash could be used by eHNFFS.
     *
     *  2. If the version in nor flash is different from eHNFFS we use now, there
     *     might be some problem.
     *
     *  3. We divide sectors into some regions(like 256) to easy to manage, and
     *     save ram space.
     */
    typedef struct eHNFFS_supermessage_flash
    {
        eHNFFS_head_t head;
        uint8_t fs_name[7];
        uint32_t version;
        eHNFFS_size_t sector_size;
        eHNFFS_size_t sector_count;
        eHNFFS_size_t name_max;
        eHNFFS_size_t file_max; // the max file size
        eHNFFS_size_t region_cnt;
    } eHNFFS_supermessage_flash_t;

    /**
     * The map of all regions.
     * Because the max size of the map is just 128 / 8 = 16B,
     * we could just store it in super sector 0/1.
     */
    typedef struct eHNFFS_region_map_flash
    {
        eHNFFS_head_t head;
        uint8_t map[];
    } eHNFFS_region_map_flash_t;

    /**
     * The position of bit map in nor flash.
     * We should also record erase times of the sector.
     */
    typedef struct eHNFFS_mapaddr_flash
    {
        eHNFFS_head_t head;
        eHNFFS_size_t begin;
        eHNFFS_off_t off;
        eHNFFS_size_t erase_times[];
    } eHNFFS_mapaddr_flash_t;

    /**
     * The position of wl message in nor flash.
     * We allocate some whole sectors at a time, so we no need off.
     */
    typedef struct eHNFFS_wladdr_flash
    {
        eHNFFS_head_t head;
        eHNFFS_size_t begin;
    } eHNFFS_wladdr_flash_t;

    /**
     * The position of hash tree in nor flash.
     */
    typedef struct eHNFFS_treeaddr_flash
    {
        eHNFFS_head_t head;
        eHNFFS_size_t begin;
    } eHNFFS_treeaddr_flash_t;

    /**
     * Every time we umount or commit(maybe have), we should write this.
     *
     * wl_index and wl_free_off only used when wl works.
     *  1) Index is the index for wl array ordered by erase times.
     *  2) Wl_free_off is the free space for added wl message.
     *
     * Whether or not record free off:
     *  1) For all maps, we don't need to append write, so free off is no need.
     *  2) For hash tree, we should traverse all hash tree entry to construct
     *     in-ram hash structure, so don't need to record the free off.
     *  3) But we don't want to traverse wl array and added message behind, so we
     *     should record free off.
     */
    typedef struct eHNFFS_commit_flash
    {
        eHNFFS_head_t head;
        eHNFFS_size_t next_id;
        eHNFFS_size_t scan_times;
        eHNFFS_size_t next_dir_sector;
        eHNFFS_size_t next_bfile_sector;
        eHNFFS_size_t reserve;
        eHNFFS_size_t wl_index;
        eHNFFS_size_t wl_bfile_index;
        eHNFFS_size_t wl_free_off; // It seems that it's no use.
    } eHNFFS_commit_flash_t;

    /**
     * Magic number that tells us whether or not corrupt has happened.
     */
    typedef struct eHNFFS_magic_flash
    {
        eHNFFS_head_t head;
        eHNFFS_size_t maigc;
    } eHNFFS_magic_flash_t;

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ------------------------------------------------------------    Cache structure    ------------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    /**
     * In ram structure.
     * Store nor flash data of (sector, off) in buffer, the size is size.
     * The mode tells us data in buffer is writen forward or backward.
     */
    typedef struct eHNFFS_cache_ram
    {
        eHNFFS_size_t sector;
        eHNFFS_off_t off; // For file, it's true address, not backward.
        eHNFFS_size_t size;
        eHNFFS_size_t mode; // for in-ram file cache could be changing flag.
        uint8_t *buffer;
    } eHNFFS_cache_ram_t;

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ----------------------------------------------------------    Map and wl structure    ---------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    /**
     * The management structure of regions.
     *
     *  1) We reserve a region for gc and data migration.
     *  2) free_region only used when we first scan nor flash.
     *  3) dir_region and bfile_region tells us which region belongs
     *     to dir or big file.
     *  4) The three regions are all map structure, and for small size
     *     nor flash like 1MB, we may only use 8 bits of a uint32_t structure.
     */
    typedef struct eHNFFS_region_map_ram
    {
        eHNFFS_size_t begin;
        eHNFFS_off_t off;
        uint32_t change_flag;

        eHNFFS_size_t reserve;
        uint32_t *free_region;

        eHNFFS_size_t dir_region_num;
        eHNFFS_off_t dir_index;
        uint32_t *dir_region;

        eHNFFS_size_t bfile_region_num;
        eHNFFS_off_t bfile_index;
        uint32_t *bfile_region;
    } eHNFFS_region_map_ram_t;

    /**
     * The basic bit map structure for free sector/erase sector/free id/remove id map.
     *
     *  1. (begin, off) is the map address in nor flash.
     *  2. For sector map
     *      1) region is which region's bit map that stroed in buffer.
     *      2) Index is the position now we scan in buffer.
     *  3. For id map
     *      1) region tells us whether or not we should update ram map to flash.
     *      2) We configure buffer size that is bits_in_buffer.
     *  4. For region map
     *      1) region is the number of bits in buffer.(may no use)
     *         Because of the max number of region is 128, it's not so big.
     */
    typedef struct eHNFFS_map_ram
    {
        eHNFFS_size_t begin;
        eHNFFS_off_t off;
        eHNFFS_size_t *etimes;

        eHNFFS_size_t region;
        eHNFFS_size_t index; // In erase map tells us there are changes.
        eHNFFS_size_t free_num;
        uint32_t buffer[];
    } eHNFFS_map_ram_t;

    /**
     * Record the total erase times of sectors in the region.
     */
    typedef struct eHNFFS_wl_message
    {
        eHNFFS_size_t region;
        eHNFFS_size_t erase_times;
    } eHNFFS_wl_message_t;

    /**
     * The in ram wl structure.
     *
     * Index tells us now we choose eHNFFS_wl_entry_flash_t[index % 5] of sorted array.
     * When index larger that threshold, turn it back to 0.
     *
     * num is the sequential number of sectors that wl has.
     * free_off is the offset for free space in wl sector, can larger than 4KB.
     *
     * We not only use regions to record canditate region, but also record erase times
     * it increased.
     *
     *
     */
    typedef struct eHNFFS_wl_ram
    {
        eHNFFS_size_t begin;
        // eHNFFS_size_t num;
        eHNFFS_off_t free_off;

        eHNFFS_size_t threshold; // Only dir region need it, because it's hot data.
        eHNFFS_size_t index;     // different from other index
        eHNFFS_size_t bfile_index;
        eHNFFS_wl_message_t regions[eHNFFS_RAM_REGION_NUM];
        eHNFFS_wl_message_t bfile_regions[eHNFFS_RAM_REGION_NUM];
        eHNFFS_wl_message_t exten_regions[eHNFFS_RAM_REGION_NUM];
    } eHNFFS_wl_ram_t;

    /**
     * The management structure of nor flash.
     *
     *  1) Sectors in dir map only allocate 1 at a time.
     *  2) Sectors in bfile map can sequentially allocate 2 or more at a time.
     *     So we also store sector map、id map、wl message、hash tree in it.
     */
    typedef struct eHNFFS_flash_manage_ram
    {
        eHNFFS_size_t region_num;
        eHNFFS_size_t region_size;
        eHNFFS_size_t scan_times;

        eHNFFS_region_map_ram_t *region_map;
        eHNFFS_map_ram_t *dir_map;
        eHNFFS_map_ram_t *bfile_map;
        eHNFFS_map_ram_t *erase_map;
        eHNFFS_wl_ram_t *wl;
    } eHNFFS_flash_manage_ram_t;

    /**
     * The bit map of id.
     */
    typedef struct eHNFFS_idmap_ram
    {
        eHNFFS_size_t max_id;
        eHNFFS_size_t bits_in_buffer;
        eHNFFS_map_ram_t *free_map;
        eHNFFS_map_ram_t *remove_map;
    } eHNFFS_idmap_ram_t;

    /**
     * The in-ram heap structure used to sort all regions by erase times.
     */
    typedef struct eHNFFS_wl_heap_ram
    {
        eHNFFS_wl_message_t wl;
    } eHNFFS_wl_heap_ram_t;

    /**
     * The region array sorted by erase times.
     */
    typedef struct eHNFFS_wlarray_flash
    {
        eHNFFS_head_t head;
        eHNFFS_wl_message_t array[];
    } eHNFFS_wlarray_flash_t;

    /**
     * The add message of region.
     *
     * We only resort a region after several operations, so we should record changes
     * during two sort operations.
     */
    typedef struct eHNFFS_wladd_flash
    {
        eHNFFS_head_t head;
        eHNFFS_wl_message_t add[];
    } eHNFFS_wladd_flash_t;

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ----------------------------------------------------------    Hash tree structure    ----------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    /**
     * The basic entry of hash tree.
     * num_of_son tells us the number of son_list.
     * data_type is the type of data, could be name or hash.
     */
    typedef struct eHNFFS_tree_entry_ram
    {
        eHNFFS_size_t id;
        eHNFFS_size_t sector;
        eHNFFS_size_t off;
        eHNFFS_size_t tail;

        eHNFFS_size_t num_of_son;
        eHNFFS_size_t data_type;
        bool if_change;

        struct eHNFFS_tree_entry_ram *son_list;
        union data
        {
            eHNFFS_hash_t hash;
            uint8_t name[eHNFFS_ENTRY_NAME_LEN];
        } data;
    } eHNFFS_tree_entry_ram_t;

    /**
     * The in ram hash tree structure of dir.
     *
     * num is the sequential number of sectors that hash tree has.
     * free_off is the start of free space in the hash sector, can larger than 4KB.
     */
    typedef struct eHNFFS_hash_tree_ram
    {
        eHNFFS_size_t begin;
        eHNFFS_size_t num;
        eHNFFS_off_t free_off;
        eHNFFS_tree_entry_ram_t *root_hash;
    } eHNFFS_hash_tree_ram_t;

    /**
     * The basic queue structure, used in hash tree construction and so on.
     * We store the pointer of in-ram dir hash  in array.
     */
    typedef struct eHNFFS_hash_queue_ram
    {
        int16_t num;
        eHNFFS_size_t head;
        eHNFFS_size_t tail;
        eHNFFS_tree_entry_ram_t **array;
    } eHNFFS_hash_queue_ram_t;

    /**
     * The basic in flash structure of dir hash.
     *
     *  1. Father id helps us to construct in ram structure when mounting.
     *  2. tail is the tail sector of dir.
     *     All sectors that belonged to a dir are linked as reverse linked list.
     *  3. There are two types of data we may store in data[].
     *          hash: When the length of name is larger than 8 bytes, we use
     *                hash + name comparasion to do name resolution.
     *          name: When the length of name is smaller than 8 bytes.
     */
    typedef struct eHNFFS_tree_entry_flash
    {
        eHNFFS_head_t head;
        eHNFFS_size_t father_id;
        eHNFFS_size_t sector;
        eHNFFS_size_t off;
        eHNFFS_size_t tail;
        uint8_t data[];
    } eHNFFS_tree_entry_flash_t;

    typedef struct eHNFFS_delete_entry_flash
    {
        eHNFFS_head_t head;
        eHNFFS_size_t father_id;
    } eHNFFS_delete_entry_flash_t;

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * ------------------------------------------------------------    File structure    -------------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    /**
     * When we write it to father dir, a file is created.
     */
    typedef struct eHNFFS_file_name_flash
    {
        eHNFFS_head_t head;
        uint8_t name[];
    } eHNFFS_file_name_flash_t;

    /**
     * The basic index structure for big file.
     * It's the same both in ram and in nor flash, and for small file, it's only in ram.
     */
    typedef struct eHNFFS_bfile_index_ram
    {
        eHNFFS_size_t sector;
        eHNFFS_off_t off;
        eHNFFS_size_t size; // Not include sector head message.
    } eHNFFS_bfile_index_ram_t;

    /**
     * The basic index structure for big file.
     * It's stored in dir.
     */
    typedef struct eHNFFS_bfile_index_flash
    {
        eHNFFS_head_t head;
        eHNFFS_bfile_index_ram_t index[];
    } eHNFFS_bfile_index_flash_t;

    /**
     * The basic data structure structure of small file.
     * It's stored in dir.
     */
    typedef struct eHNFFS_sfile_data_flash
    {
        eHNFFS_head_t head;
        uint8_t data[];
    } eHNFFS_sfile_data_flash_t;

    /**
     * The in ram structure of file.
     *
     *  1. Type tells us it's big or small file;
     *
     *  2. position is the logical position of file where we start to read/write(program).
     *     Could be changed by lseek function.
     *
     *  3. File cache has different usage for big / small file.
     *     For big file, it stores index in buffer.
     *     For small file, it stores all datas in nor flash.
     *     and (Sector, off) belongs to old data, size belongs to new message in buffer.
     *
     *  4. cfg offer basic read/program function. (now is no need)
     *
     *  5. All file we stored in ram are linked by the next_file, and are sorted by
     *     the last time we use. If there is no more space, could delete file at the
     *     tail of the link list.
     */
    typedef struct eHNFFS_file_ram
    {
        eHNFFS_size_t id;
        eHNFFS_size_t father_id;
        int type;
        eHNFFS_size_t file_size;
        eHNFFS_off_t pos;
        eHNFFS_size_t sector;
        eHNFFS_off_t off;
        eHNFFS_size_t namelen;
        eHNFFS_cache_ram_t file_cache;

        struct eHNFFS_file_ram *next_file;
    } eHNFFS_file_ram_t;

    /**
     * The list of opened file we store in ram.
     *
     *  1. Count is the length of link list.
     *
     *  2. Max is the max length of link list, if large than it, we should delete tail.
     *
     *  3. used_space is the total space we use for file in ram.
     *     Because of the cache, we can't just calculate it.
     *
     *  4. File is the most recently used file.
     */
    typedef struct eHNFFS_file_list_ram
    {
        eHNFFS_size_t count;
        eHNFFS_off_t used_space;
        eHNFFS_file_ram_t *file;
    } eHNFFS_file_list_ram_t;

    /**
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     * -------------------------------------------------------------    Dir structure    -------------------------------------------------------------
     * -----------------------------------------------------------------------------------------------------------------------------------------------
     */

    // File info structure
    typedef struct eHNFFS_info
    {
        uint8_t type;

        // Size of the file, only valid for REG files. Limited to 32-bits.
        // eHNFFS_size_t size;

        // Name of the file stored as a null-terminated string. Limited to
        // LFS_NAME_MAX+1, which can be changed by redefining LFS_NAME_MAX to
        // reduce RAM. LFS_NAME_MAX is stored in superblock and must be
        // respected by other littlefs drivers.
        char name[eHNFFS_NAME_MAX + 1];
    } eHNFFS_info_ram_t;

    /**
     * We store all offset of sectors belong to dir,
     * It tells us where backward begins.
     */
    typedef struct eHNFFS_list_ram
    {
        eHNFFS_off_t off;
        struct eHNFFS_list_ram *next;
    } eHNFFS_list_ram_t;

    /**
     * The in ram structure of directory.
     *
     *  1. old_space tell us how many space can we gc if gc happends for the dir.
     *
     *  2. The name of file and data are seperated. To quickly find file in data,
     *     we store all names in the front of the sector belongs to the dir, and
     *     all data in the end of it.
     *     The forward and backward pointer tell us where we should tail to write.
     *
     *  3. If a dir is very big, then we should use many sectors. All these sectors
     *     use a pointer at the end of each sector linked together.
     *     tail is the first sector we use for the dir.
     *     current_sector is the sector now we use to program.
     *     And rest_space tells us how many space there are in current sector.
     *
     *  4. All dir we stored in ram are linked by the next_dir, and are sorted by
     *     the last time we use. If there is no more space, could delete dir at the
     *     tail of the link list.
     *
     *  5. Backward list stores all backward start message of former sectors belong
     *     to dir.
     */
    typedef struct eHNFFS_dir_ram
    {
        eHNFFS_size_t id;
        eHNFFS_size_t father_id;
        eHNFFS_size_t old_space;
        int type;

        eHNFFS_size_t pos_sector;
        eHNFFS_size_t pos_off;
        eHNFFS_size_t pos_presector;

        eHNFFS_size_t sector;
        eHNFFS_size_t off;
        eHNFFS_size_t namelen;

        eHNFFS_size_t tail;
        eHNFFS_off_t forward;
        eHNFFS_off_t backward;
        eHNFFS_off_t rest_space;

        eHNFFS_list_ram_t *backward_list;
        struct eHNFFS_dir_ram *next_dir;
    } eHNFFS_dir_ram_t;

    /**
     * The list of opened dir we store in ram.
     *
     *  1. Count is the length of link list.
     *  2. Max is the max length of link list, if large than it, we should delete tail.
     *  3. Dir is the most recently used dir.
     */
    typedef struct eHNFFS_dir_list_ram
    {
        eHNFFS_size_t count;
        eHNFFS_dir_ram_t *dir;
    } eHNFFS_dir_list_ram_t;

    /**
     * The name of dir, stored at the beginning of sectors that belong to the dir.
     */
    typedef struct eHNFFS_dir_name_flash
    {
        eHNFFS_head_t head;
        eHNFFS_size_t tail;
        uint8_t name[];
    } eHNFFS_dir_name_flash_t;

    /**
     * Writen to father dir to represent hierarchy relationship.
     */
    typedef struct eHNFFS_dir_flash
    {
        eHNFFS_head_t head;
        eHNFFS_size_t tail;
        uint8_t name[];
    } eHNFFS_dir_flash_t;

    /**
     * Writen to a dir to seperate name and data/index.
     */
    typedef struct eHNFFS_skip_flash
    {
        eHNFFS_head_t head;
        eHNFFS_size_t len;
    } eHNFFS_skip_flash_t;

    /**
     * -------------------------------------------------------------------------------------------------------
     * -------------------------------    Other ram fundamental structure    ---------------------------------
     * -------------------------------------------------------------------------------------------------------
     */

    /**
     * Record id of file/dir that is worth doing gc.
     */
    typedef struct eHNFFS_gc_id_ram
    {
        eHNFFS_size_t id;
        struct eHNFFS_gc_id_ram *next_id;
    } eHNFFS_gc_id_ram_t;

    /**
     * The in-ram list of file/dir that is worth doing gc.
     */
    typedef struct eHNFFS_gc_list_ram
    {
        eHNFFS_size_t count;
        eHNFFS_gc_id_ram_t *first_id;
    } eHNFFS_gc_list_ram_t;

    /**
     * eHNFFS_t includes all source we use and all message we have.
     * It's the main ram structure of the fs.
     */
    typedef struct eHNFFS
    {
        eHNFFS_cache_ram_t *rcache;
        eHNFFS_cache_ram_t *pcache;

        eHNFFS_superblock_ram_t *superblock;
        eHNFFS_flash_manage_ram_t *manager;
        eHNFFS_hash_tree_ram_t *hash_tree;
        eHNFFS_idmap_ram_t *id_map;

        eHNFFS_file_list_ram_t *file_list;
        eHNFFS_dir_list_ram_t *dir_list;

        const struct eHNFFS_config *cfg;
    } eHNFFS_t;

#ifdef __cplusplus
}
#endif

#endif
