/*
 * eHNFFS utility functions
 *
 * Copyright (c) 2022, The littlefs authors.
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef eHNFFS_UTIL_H
#define eHNFFS_UTIL_H

// Users can override eHNFFS_util.h with their own configuration by defining
// eHNFFS_CONFIG as a header file to include (-DeHNFFS_CONFIG=eHNFFS_config.h).
//
// If eHNFFS_CONFIG is used, none of the default utils will be emitted and must be
// provided by the config file. To start, I would suggest copying eHNFFS_util.h
// and modifying as needed.
#ifdef eHNFFS_CONFIG
#define eHNFFS_STRINGIZE(x) eHNFFS_STRINGIZE2(x)
#define eHNFFS_STRINGIZE2(x) #x
#include eHNFFS_STRINGIZE(eHNFFS_CONFIG)
#else

// System includes
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

// Need to add when all things is ready
#include "FreeRTOS.h"

#ifndef eHNFFS_NO_MALLOC
#include <stdlib.h>
#endif
#ifndef eHNFFS_NO_ASSERT
#include <assert.h>
#endif
#if !defined(eHNFFS_NO_DEBUG) || \
    !defined(eHNFFS_NO_WARN) ||  \
    !defined(eHNFFS_NO_ERROR) || \
    defined(eHNFFS_YES_TRACE)
#include <stdio.h>
#endif

#define inline __inline

#ifdef __cplusplus
extern "C"
{
#endif

// Macros, may be replaced by system specific wrappers. Arguments to these
// macros must not have side-effects as the macros can be removed for a smaller
// code footprint

// Logging functions
#ifndef eHNFFS_TRACE
#ifdef eHNFFS_YES_TRACE
#define eHNFFS_TRACE_(fmt, ...) \
    printf("%s:%d:trace: " fmt "%s\n", __FILE__, __LINE__, __VA_ARGS__)
#define eHNFFS_TRACE(...) eHNFFS_TRACE_(__VA_ARGS__, "")
#else
#define eHNFFS_TRACE(...)
#endif
#endif

#ifndef eHNFFS_DEBUG
#ifndef eHNFFS_NO_DEBUG
#define eHNFFS_DEBUG_(fmt, ...) \
    printf("%s:%d:debug: " fmt "%s\n", __FILE__, __LINE__, __VA_ARGS__)
#define eHNFFS_DEBUG(...) eHNFFS_DEBUG_(__VA_ARGS__, "")
#else
#define eHNFFS_DEBUG(...)
#endif
#endif

#ifndef eHNFFS_WARN
#ifndef eHNFFS_NO_WARN
#define eHNFFS_WARN_(fmt, ...) \
    printf("%s:%d:warn: " fmt "%s\n", __FILE__, __LINE__, __VA_ARGS__)
#define eHNFFS_WARN(...) eHNFFS_WARN_(__VA_ARGS__, "")
#else
#define eHNFFS_WARN(...)
#endif
#endif

#ifndef eHNFFS_ERROR
#ifndef eHNFFS_NO_ERROR
#define eHNFFS_ERROR_(fmt, ...) \
    printf("%s:%d:error: " fmt "%s\n", __FILE__, __LINE__, __VA_ARGS__)
#define eHNFFS_ERROR(...) eHNFFS_ERROR_(__VA_ARGS__, "")
#else
#define eHNFFS_ERROR(...)
#endif
#endif

// Runtime assertions
#ifndef eHNFFS_ASSERT
#ifndef eHNFFS_NO_ASSERT
#define eHNFFS_ASSERT(test) assert(test)
#else
#define eHNFFS_ASSERT(test)
#endif
#endif

    // Builtin functions, these may be replaced by more efficient
    // toolchain-specific implementations. eHNFFS_NO_INTRINSICS falls back to a more
    // expensive basic C implementation for debugging purposes

    // Min/max functions for unsigned 32-bit numbers
    static inline uint32_t eHNFFS_max(uint32_t a, uint32_t b)
    {
        return (a > b) ? a : b;
    }

    static inline uint32_t eHNFFS_min(uint32_t a, uint32_t b)
    {
        return (a < b) ? a : b;
    }

    // Align to nearest multiple of a size
    static inline uint32_t eHNFFS_aligndown(uint32_t a, uint32_t alignment)
    {
        return a - (a % alignment);
    }

    static inline uint32_t eHNFFS_alignup(uint32_t a, uint32_t alignment)
    {
        return eHNFFS_aligndown(a + alignment - 1, alignment);
    }

    // Find the smallest power of 2 greater than or equal to a
    static inline uint32_t eHNFFS_npw2(uint32_t a)
    {
#if !defined(eHNFFS_NO_INTRINSICS) && (defined(__GNUC__) || defined(__CC_ARM))
        return 32 - __builtin_clz(a - 1);
#else
    uint32_t r = 0;
    uint32_t s;
    a -= 1;
    s = (a > 0xffff) << 4;
    a >>= s;
    r |= s;
    s = (a > 0xff) << 3;
    a >>= s;
    r |= s;
    s = (a > 0xf) << 2;
    a >>= s;
    r |= s;
    s = (a > 0x3) << 1;
    a >>= s;
    r |= s;
    return (r | (a >> 1)) + 1;
#endif
    }

    // Count the number of trailing binary zeros in a
    // eHNFFS_ctz(0) may be undefined
    static inline uint32_t eHNFFS_ctz(uint32_t a)
    {
#if !defined(eHNFFS_NO_INTRINSICS) && defined(__GNUC__)
        return __builtin_ctz(a);
#else
    return eHNFFS_npw2((a & -a) + 1) - 1;
#endif
    }

    // Count the number of binary ones in a
    static inline uint32_t eHNFFS_popc(uint32_t a)
    {
#if !defined(eHNFFS_NO_INTRINSICS) && (defined(__GNUC__) || defined(__CC_ARM))
        return __builtin_popcount(a);
#else
    a = a - ((a >> 1) & 0x55555555);
    a = (a & 0x33333333) + ((a >> 2) & 0x33333333);
    return (((a + (a >> 4)) & 0xf0f0f0f) * 0x1010101) >> 24;
#endif
    }

    // Find the sequence comparison of a and b, this is the distance
    // between a and b ignoring overflow
    static inline int eHNFFS_scmp(uint32_t a, uint32_t b)
    {
        return (int)(unsigned)(a - b);
    }

    // Convert between 32-bit little-endian and native order
    static inline uint32_t eHNFFS_fromle32(uint32_t a)
    {
#if !defined(eHNFFS_NO_INTRINSICS) && ((defined(BYTE_ORDER) && defined(ORDER_LITTLE_ENDIAN) && BYTE_ORDER == ORDER_LITTLE_ENDIAN) ||         \
                                       (defined(__BYTE_ORDER) && defined(__ORDER_LITTLE_ENDIAN) && __BYTE_ORDER == __ORDER_LITTLE_ENDIAN) || \
                                       (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
        return a;
#elif !defined(eHNFFS_NO_INTRINSICS) && ((defined(BYTE_ORDER) && defined(ORDER_BIG_ENDIAN) && BYTE_ORDER == ORDER_BIG_ENDIAN) ||         \
                                         (defined(__BYTE_ORDER) && defined(__ORDER_BIG_ENDIAN) && __BYTE_ORDER == __ORDER_BIG_ENDIAN) || \
                                         (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
    return __builtin_bswap32(a);
#else
    return (((uint8_t *)&a)[0] << 0) |
           (((uint8_t *)&a)[1] << 8) |
           (((uint8_t *)&a)[2] << 16) |
           (((uint8_t *)&a)[3] << 24);
#endif
    }

    static inline uint32_t eHNFFS_tole32(uint32_t a)
    {
        return eHNFFS_fromle32(a);
    }

    // Convert between 32-bit big-endian and native order
    static inline uint32_t eHNFFS_frombe32(uint32_t a)
    {
#if !defined(eHNFFS_NO_INTRINSICS) && ((defined(BYTE_ORDER) && defined(ORDER_LITTLE_ENDIAN) && BYTE_ORDER == ORDER_LITTLE_ENDIAN) ||         \
                                       (defined(__BYTE_ORDER) && defined(__ORDER_LITTLE_ENDIAN) && __BYTE_ORDER == __ORDER_LITTLE_ENDIAN) || \
                                       (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
        return __builtin_bswap32(a);
#elif !defined(eHNFFS_NO_INTRINSICS) && ((defined(BYTE_ORDER) && defined(ORDER_BIG_ENDIAN) && BYTE_ORDER == ORDER_BIG_ENDIAN) ||         \
                                         (defined(__BYTE_ORDER) && defined(__ORDER_BIG_ENDIAN) && __BYTE_ORDER == __ORDER_BIG_ENDIAN) || \
                                         (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
    return a;
#else
    return (((uint8_t *)&a)[0] << 24) |
           (((uint8_t *)&a)[1] << 16) |
           (((uint8_t *)&a)[2] << 8) |
           (((uint8_t *)&a)[3] << 0);
#endif
    }

    static inline uint32_t eHNFFS_tobe32(uint32_t a)
    {
        return eHNFFS_frombe32(a);
    }

    // Calculate CRC-32 with polynomial = 0x04c11db7
    uint32_t eHNFFS_crc(uint32_t crc, const void *buffer, size_t size);

    // Allocate memory, only used if buffers are not provided to littlefs
    // Note, memory must be 64-bit aligned
    static inline void *eHNFFS_malloc(size_t size)
    {
#ifndef eHNFFS_NO_MALLOC
        // Need to change to the malloc function that used in your system
        return pvPortMalloc(size);
        // return malloc(size);
#else
    (void)size;
    return NULL;
#endif
    }

    // Deallocate memory, only used if buffers are not provided to littlefs
    static inline void eHNFFS_free(void *p)
    {
#ifndef eHNFFS_NO_MALLOC
        // Need to change to the free function that used in your system
        vPortFree(p);
        // free(p);
#else
    (void)p;
#endif
    }

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
#endif
