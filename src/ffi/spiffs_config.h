/*
 * spiffs_config.h
 *
 *  Created on: Jul 3, 2013
 *      Author: petera
 */

#ifndef SPIFFS_CONFIG_H_
#define SPIFFS_CONFIG_H_

#include "prelude.h"
// ----------- 8< ------------
// #include <FreeRTOS.h>    // for vPortEnterCritical/vPortExitCritical
// ----------- >8 ------------

// compile time switches

// Set generic spiffs debug output call.
#ifndef SPIFFS_DBG
#define SPIFFS_DBG(...) //printf(__VA_ARGS__)
#endif
// Set spiffs debug output call for garbage collecting.
#ifndef SPIFFS_GC_DBG
#define SPIFFS_GC_DBG(...) //printf(__VA_ARGS__)
#endif
// Set spiffs debug output call for caching.
#ifndef SPIFFS_CACHE_DBG
#define SPIFFS_CACHE_DBG(...) //printf(__VA_ARGS__)
#endif
// Set spiffs debug output call for system consistency checks.
#ifndef SPIFFS_CHECK_DBG
#define SPIFFS_CHECK_DBG(...) //printf(__VA_ARGS__)
#endif

// Enable/disable API functions to determine exact number of bytes
// for filedescriptor and cache buffers. Once decided for a configuration,
// this can be disabled to reduce flash.
#ifndef SPIFFS_BUFFER_HELP
#define SPIFFS_BUFFER_HELP              1
#endif

// Enables/disable memory read caching of nucleus file system operations.
// If enabled, memory area must be provided for cache in SPIFFS_mount.
#ifndef  SPIFFS_CACHE
#define SPIFFS_CACHE                    1
#endif
#if SPIFFS_CACHE
// Enables memory write caching for file descriptors in hydrogen
#ifndef  SPIFFS_CACHE_WR
#define SPIFFS_CACHE_WR                 1
#endif

// Enable/disable statistics on caching. Debug/test purpose only.
#ifndef  SPIFFS_CACHE_STATS
#define SPIFFS_CACHE_STATS              0
#endif
#endif

// Always check header of each accessed page to ensure consistent state.
// If enabled it will increase number of reads, will increase flash.
#ifndef SPIFFS_PAGE_CHECK
#define SPIFFS_PAGE_CHECK               1
#endif

// Define maximum number of gc runs to perform to reach desired free pages.
#ifndef SPIFFS_GC_MAX_RUNS
#define SPIFFS_GC_MAX_RUNS              3
#endif

// Enable/disable statistics on gc. Debug/test purpose only.
#ifndef SPIFFS_GC_STATS
#define SPIFFS_GC_STATS                 0
#endif

// Garbage collecting examines all pages in a block which and sums up
// to a block score. Deleted pages normally gives positive score and
// used pages normally gives a negative score (as these must be moved).
// To have a fair wear-leveling, the erase age is also included in score,
// whose factor normally is the most positive.
// The larger the score, the more likely it is that the block will
// picked for garbage collection.

// Garbage collecting heuristics - weight used for deleted pages.
#ifndef SPIFFS_GC_HEUR_W_DELET
#define SPIFFS_GC_HEUR_W_DELET          (5)
#endif
// Garbage collecting heuristics - weight used for used pages.
#ifndef SPIFFS_GC_HEUR_W_USED
#define SPIFFS_GC_HEUR_W_USED           (-1)
#endif
// Garbage collecting heuristics - weight used for time between
// last erased and erase of this block.
#ifndef SPIFFS_GC_HEUR_W_ERASE_AGE
#define SPIFFS_GC_HEUR_W_ERASE_AGE      (50)
#endif

// Object name maximum length. Note that this length include the
// zero-termination character, meaning maximum string of characters
// can at most be SPIFFS_OBJ_NAME_LEN - 1.
#ifndef SPIFFS_OBJ_NAME_LEN
#define SPIFFS_OBJ_NAME_LEN             (32)
#endif

// Size of buffer allocated on stack used when copying data.
// Lower value generates more read/writes. No meaning having it bigger
// than logical page size.
#ifndef SPIFFS_COPY_BUFFER_STACK
#define SPIFFS_COPY_BUFFER_STACK        (64)
#endif

// Enable this to have an identifiable spiffs filesystem. This will look for
// a magic in all sectors to determine if this is a valid spiffs system or
// not on mount point. If not, SPIFFS_format must be called prior to mounting
// again.
#ifndef SPIFFS_USE_MAGIC
#define SPIFFS_USE_MAGIC                (1)
#endif

#if SPIFFS_USE_MAGIC
// Only valid when SPIFFS_USE_MAGIC is enabled. If SPIFFS_USE_MAGIC_LENGTH is
// enabled, the magic will also be dependent on the length of the filesystem.
// For example, a filesystem configured and formatted for 4 megabytes will not
// be accepted for mounting with a configuration defining the filesystem as 2
// megabytes.
#ifndef SPIFFS_USE_MAGIC_LENGTH
#define SPIFFS_USE_MAGIC_LENGTH         (1)
#endif
#endif

// SPIFFS_LOCK and SPIFFS_UNLOCK protects spiffs from reentrancy on api level
// These should be defined on a multithreaded system

// define this to enter a mutex if you're running on a multithreaded system
#ifndef SPIFFS_LOCK
#define SPIFFS_LOCK(fs)        // vPortEnterCritical()
#endif
// define this to exit a mutex if you're running on a multithreaded system
#ifndef SPIFFS_UNLOCK
#define SPIFFS_UNLOCK(fs)      // vPortExitCritical()
#endif

// Enable if only one spiffs instance with constant configuration will exist
// on the target. This will reduce calculations, flash and memory accesses.
// Parts of configuration must be defined below instead of at time of mount.
#ifndef SPIFFS_SINGLETON
#define SPIFFS_SINGLETON 1
#endif

// ESP8266 supports only sector erase, which is 4096 bytes
#define SPIFFS_ESP_ERASE_SIZE       (4096)

#if SPIFFS_SINGLETON
// Instead of giving parameters in config struct, singleton build must
// give parameters in defines below.
#ifndef SPIFFS_CFG_PHYS_SZ
#define SPIFFS_CFG_PHYS_SZ(ignore)          (SPIFFS_SIZE)
#endif
#ifndef SPIFFS_CFG_PHYS_ERASE_SZ
#define SPIFFS_CFG_PHYS_ERASE_SZ(ignore)    (SPIFFS_ESP_ERASE_SIZE)
#endif
#ifndef SPIFFS_CFG_PHYS_ADDR
#define SPIFFS_CFG_PHYS_ADDR(ignore)        (SPIFFS_BASE_ADDR)
#endif
#ifndef SPIFFS_CFG_LOG_PAGE_SZ
#define SPIFFS_CFG_LOG_PAGE_SZ(ignore)      (SPIFFS_LOG_PAGE_SIZE)
#endif
#ifndef SPIFFS_CFG_LOG_BLOCK_SZ
#define SPIFFS_CFG_LOG_BLOCK_SZ(ignore)     (SPIFFS_LOG_BLOCK_SIZE)
#endif
#endif

// Enable this if your target needs aligned data for index tables
#ifndef SPIFFS_ALIGNED_OBJECT_INDEX_TABLES
#define SPIFFS_ALIGNED_OBJECT_INDEX_TABLES       1
#endif

// Enable this if you want the HAL callbacks to be called with the spiffs struct
#ifndef SPIFFS_HAL_CALLBACK_EXTRA
#define SPIFFS_HAL_CALLBACK_EXTRA         0
#endif

// Enable this if you want to add an integer offset to all file handles
// (spiffs_file). This is useful if running multiple instances of spiffs on
// same target, in order to recognise to what spiffs instance a file handle
// belongs.
// NB: This adds config field fh_ix_offset in the configuration struct when
// mounting, which must be defined.
#ifndef SPIFFS_FILEHDL_OFFSET
#define SPIFFS_FILEHDL_OFFSET                 17
// Not ideal having to use a literal above, which is necessary because this file
// is also included by tools that run on the host, but at least do some checks
// when building the target code.
#ifdef LWIP_SOCKET_OFFSET
#if SPIFFS_FILEHDL_OFFSET < (LWIP_SOCKET_OFFSET + MEMP_NUM_NETCONN)
#error SPIFFS_FILEHDL_OFFSET clashes with lwip sockets range.
#endif
#endif
#endif

// Enable this to compile a read only version of spiffs.
// This will reduce binary size of spiffs. All code comprising modification
// of the file system will not be compiled. Some config will be ignored.
// HAL functions for erasing and writing to spi-flash may be null. Cache
// can be disabled for even further binary size reduction (and ram savings).
// Functions modifying the fs will return SPIFFS_ERR_RO_NOT_IMPL.
// If the file system cannot be mounted due to aborted erase operation and
// SPIFFS_USE_MAGIC is enabled, SPIFFS_ERR_RO_ABORTED_OPERATION will be
// returned.
// Might be useful for e.g. bootloaders and such.
#ifndef SPIFFS_READ_ONLY
#define SPIFFS_READ_ONLY                      0
#endif

// Enable this to add a temporal file cache using the fd buffer.
// The effects of the cache is that SPIFFS_open will find the file faster in
// certain cases. It will make it a lot easier for spiffs to find files
// opened frequently, reducing number of readings from the spi flash for
// finding those files.
// This will grow each fd by 6 bytes. If your files are opened in patterns
// with a degree of temporal locality, the system is optimized.
// Examples can be letting spiffs serve web content, where one file is the css.
// The css is accessed for each html file that is opened, meaning it is
// accessed almost every second time a file is opened. Another example could be
// a log file that is often opened, written, and closed.
// The size of the cache is number of given file descriptors, as it piggybacks
// on the fd update mechanism. The cache lives in the closed file descriptors.
// When closed, the fd know the whereabouts of the file. Instead of forgetting
// this, the temporal cache will keep handling updates to that file even if the
// fd is closed. If the file is opened again, the location of the file is found
// directly. If all available descriptors become opened, all cache memory is
// lost.
#ifndef SPIFFS_TEMPORAL_FD_CACHE
#define SPIFFS_TEMPORAL_FD_CACHE              1
#endif

// Temporal file cache hit score. Each time a file is opened, all cached files
// will lose one point. If the opened file is found in cache, that entry will
// gain SPIFFS_TEMPORAL_CACHE_HIT_SCORE points. One can experiment with this
// value for the specific access patterns of the application. However, it must
// be between 1 (no gain for hitting a cached entry often) and 255.
#ifndef SPIFFS_TEMPORAL_CACHE_HIT_SCORE
#define SPIFFS_TEMPORAL_CACHE_HIT_SCORE       4
#endif

// Set SPIFFS_TEST_VISUALISATION to non-zero to enable SPIFFS_vis function
// in the api. This function will visualize all filesystem using given printf
// function.
#ifndef SPIFFS_TEST_VISUALISATION
#define SPIFFS_TEST_VISUALISATION         1
#endif
#if SPIFFS_TEST_VISUALISATION
#ifndef spiffs_printf
#define spiffs_printf(...)                printf(__VA_ARGS__)
#endif
// spiffs_printf argument for a free page
#ifndef SPIFFS_TEST_VIS_FREE_STR
#define SPIFFS_TEST_VIS_FREE_STR          "_"
#endif
// spiffs_printf argument for a deleted page
#ifndef SPIFFS_TEST_VIS_DELE_STR
#define SPIFFS_TEST_VIS_DELE_STR          "/"
#endif
// spiffs_printf argument for an index page for given object id
#ifndef SPIFFS_TEST_VIS_INDX_STR
#define SPIFFS_TEST_VIS_INDX_STR(id)      "i"
#endif
// spiffs_printf argument for a data page for given object id
#ifndef SPIFFS_TEST_VIS_DATA_STR
#define SPIFFS_TEST_VIS_DATA_STR(id)      "d"
#endif
#endif

// Types depending on configuration such as the amount of flash bytes
// given to spiffs file system in total (spiffs_file_system_size),
// the logical block size (log_block_size), and the logical page size
// (log_page_size)

// Block index type. Make sure the size of this type can hold
// the highest number of all blocks - i.e. spiffs_file_system_size / log_block_size
typedef u16 spiffs_block_ix;
// Page index type. Make sure the size of this type can hold
// the highest page number of all pages - i.e. spiffs_file_system_size / log_page_size
typedef u16 spiffs_page_ix;
// Object id type - most significant bit is reserved for index flag. Make sure the
// size of this type can hold the highest object id on a full system,
// i.e. 2 + (spiffs_file_system_size / (2*log_page_size))*2
typedef u16 spiffs_obj_id;
// Object span index type. Make sure the size of this type can
// hold the largest possible span index on the system -
// i.e. (spiffs_file_system_size / log_page_size) - 1
typedef u16 spiffs_span_ix;

#endif /* SPIFFS_CONFIG_H_ */
