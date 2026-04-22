/*++

Copyright (c) 2026  The EverywhereOS Authors

Module Name:

    evryfs.h

Abstract:

    EVRYFS v2 on-disk structures and public API. design:
    MFT-based file records with attribute fields, cluster bitmap for free
    space tracking, extent run-lists for non-contiguous allocation, full
    subdirectory support, file attributes, stat, rename, delete, mkdir,
    rmdir, and volume label.

    On-disk layout (512-byte sectors, 2KB clusters = 4 sectors each):

        LBA 0         Boot sector           (EVRYFS_BOOT, 512 bytes)
        LBA 1-2       Cluster bitmap        (2 sectors = 8192 cluster bits)
        LBA 3-130     MFT                   (128 records x 512 bytes)
                        Record 0: $Volume   (volume metadata)
                        Record 1: $Bitmap   (bitmap descriptor)
                        Record 2: Root dir  (\)
                        Record 3+: User files and directories
        LBA 131+      Data clusters         (cluster N at LBA 131 + N*4)

Author:

    Noah Juopperi <nipfswd@gmail.com>

Environment:

    Kernel-mode only

--*/

#ifndef _EVRYFS_H_
#define _EVRYFS_H_

#include <stdint.h>

#define EVRYFS_MAGIC                0x45565259U     /* 'EVRY' */
#define EVRYFS_MFT_MAGIC            0x464D5645U     /* 'EVMF' */
#define EVRYFS_VERSION              2

#define EVRYFS_BOOT_LBA             0
#define EVRYFS_BITMAP_LBA           1
#define EVRYFS_BITMAP_SECTORS       2
#define EVRYFS_MFT_LBA              3
#define EVRYFS_MFT_SECTORS          128
#define EVRYFS_MFT_MAX_RECORDS      128
#define EVRYFS_DATA_START_LBA       131
#define EVRYFS_SECTORS_PER_CLUSTER  4
#define EVRYFS_CLUSTER_SIZE         2048
#define EVRYFS_BITMAP_BITS          (EVRYFS_BITMAP_SECTORS * 512 * 8)  /* 8192 */

#define EVRYFS_RECORD_VOLUME        0
#define EVRYFS_RECORD_BITMAP        1
#define EVRYFS_RECORD_ROOT          2
#define EVRYFS_FIRST_USER_RECORD    3
#define EVRYFS_INVALID_RECORD       0xFFFFFFFFU

#define EVRYFS_NAME_LEN             59
#define EVRYFS_MAX_EXTENTS          8
#define EVRYFS_MAX_DEPTH            8
#define EVRYFS_LIST_MAX             32

#define EVRYFS_ATTR_READONLY        0x00000001
#define EVRYFS_ATTR_HIDDEN          0x00000002
#define EVRYFS_ATTR_SYSTEM          0x00000004
#define EVRYFS_ATTR_DIRECTORY       0x00000010
#define EVRYFS_ATTR_ARCHIVE         0x00000020

#define EVRYFS_FLAG_INUSE           0x00000001
#define EVRYFS_FLAG_ISDIR           0x00000002

/*
 * EVRYFS_BOOT -- boot sector, exactly 512 bytes.
 *
 * jump code, OEM identifier,
 * geometry, MFT location, bitmap location, and volume label.
 */
typedef struct {
    uint8_t  jump[3];
    uint8_t  oem_id[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint32_t total_sectors;
    uint32_t mft_start_lba;
    uint32_t mft_record_count;
    uint32_t bitmap_start_lba;
    uint32_t bitmap_sectors;
    uint32_t data_start_lba;
    uint32_t total_clusters;
    uint32_t magic;
    uint32_t version;
    uint32_t serial_number;
    char     volume_label[32];
    uint8_t  reserved[512 - 3 - 8 - 2 - 1 - (10 * 4) - 32];
} __attribute__((packed)) EVRYFS_BOOT;

/*
 * EVRYFS_MFT_RECORD -- one file/directory record, exactly 512 bytes.
 *
 * each record carries all
 * metadata for one file or directory inline (resident attributes).
 * Non-resident data is referenced via the extent run-list.
 */
typedef struct {
    uint32_t signature;
    uint32_t flags;
    uint32_t record_number;
    uint32_t parent_record;
    uint32_t file_attributes;
    uint32_t create_time;
    uint32_t modify_time;
    uint32_t data_size;
    uint32_t alloc_size;
    uint8_t  name_len;
    char     name[EVRYFS_NAME_LEN];
    uint8_t  extent_count;
    uint8_t  pad[2];
    struct {
        uint32_t start_cluster;
        uint32_t cluster_count;
    } extents[EVRYFS_MAX_EXTENTS];
    uint8_t  reserved[512
        - (9 * 4)
        - 1 - EVRYFS_NAME_LEN
        - 1 - 2
        - EVRYFS_MAX_EXTENTS * 8];
} __attribute__((packed)) EVRYFS_MFT_RECORD;

/*
 * EVRYFS_STAT -- per-file metadata returned by EvryFsStat.
 */
typedef struct {
    char     name[EVRYFS_NAME_LEN];
    uint32_t file_attributes;
    uint32_t data_size;
    uint32_t alloc_size;
    uint32_t create_time;
    uint32_t modify_time;
    uint32_t record_number;
    uint32_t parent_record;
} EVRYFS_STAT;

/* ATA layer (ata.c) */
int AtaReadSector (uint32_t lba, uint8_t* buf);
int AtaWriteSector(uint32_t lba, const uint8_t* buf);

/* Public filesystem API (evryfs.c) */
int  EvryFsInit(void);
int  EvryFsFormat(const char* volume_label);
int  EvryFsWriteFile(const char* path, const uint8_t* data, uint32_t len);
int  EvryFsReadFile(const char* path, uint8_t* buf, uint32_t maxlen);
int  EvryFsDeleteFile(const char* path);
int  EvryFsMkDir(const char* path);
int  EvryFsRmDir(const char* path);
int  EvryFsRename(const char* old_path, const char* new_path);
int  EvryFsStat(const char* path, EVRYFS_STAT* out);
int  EvryFsGetAttr(const char* path, uint32_t* attrs);
int  EvryFsSetAttr(const char* path, uint32_t attrs);
int  EvryFsGetVolumeLabel(char* label, int maxlen);
int  EvryFsSetVolumeLabel(const char* label);
void EvryFsList(const char* path, char names[][EVRYFS_NAME_LEN],
                uint32_t* sizes, uint32_t* attrs, int* count);

#endif /* _EVRYFS_H_ */
