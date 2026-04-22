/*++

Copyright (c) 2026  The EverywhereOS Authors

Module Name:

    evryfs.c

Abstract:

    EVRYFS v2 filesystem driver. design principles:
    MFT-based file records, cluster bitmap for free-space tracking,
    extent run-list allocation for non-contiguous files, subdirectory
    support via parent record references, file attributes, stat,
    rename, delete, mkdir, rmdir, and volume label management.

    On-disk layout (2KB clusters = 4 sectors each):

        LBA 0:        Boot sector  (EVRYFS_BOOT)
        LBA 1-2:      Cluster bitmap  (8192 cluster bits)
        LBA 3-130:    MFT  (128 records x 512 bytes, one record per sector)
                        Record 0: $Volume
                        Record 1: $Bitmap
                        Record 2: Root directory
                        Record 3+: User files and directories
        LBA 131+:     Data  (cluster N at LBA 131 + N*4)

Author:

    Noah Juopperi <nipfswd@gmail.com>

Environment:

    Kernel-mode only

--*/

#include "evryfs.h"

static uint8_t s_bootbuf[512];
static uint8_t s_bitmap[EVRYFS_BITMAP_SECTORS * 512];
static int     s_present = 0;

/* ---- String and memory helpers ----------------------------------------- */

static int FsStrLen(const char* s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int FsStrEq(const char* a, const char* b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return (*a == 0 && *b == 0);
}

static void FsStrCpy(char* dst, const char* src, int n)
{
    int i = 0;
    while (i < n - 1 && src[i]) { dst[i] = src[i]; i++; }
    while (i < n) { dst[i++] = 0; }
}

static void FsMemSet(void* dst, int val, int len)
{
    uint8_t* p = (uint8_t*)dst;
    for (int i = 0; i < len; i++) p[i] = (uint8_t)val;
}

static void FsMemCpy(void* dst, const void* src, int len)
{
    uint8_t*       d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (int i = 0; i < len; i++) d[i] = s[i];
}

/* ---- Cluster bitmap ---------------------------------------------------- */

static int BitmapGet(uint32_t cluster)
{
    if (cluster >= EVRYFS_BITMAP_BITS) return 1;
    return (s_bitmap[cluster / 8] >> (cluster % 8)) & 1;
}

static void BitmapSet(uint32_t cluster, int val)
{
    if (cluster >= EVRYFS_BITMAP_BITS) return;
    if (val)
        s_bitmap[cluster / 8] |=  (uint8_t)(1u << (cluster % 8));
    else
        s_bitmap[cluster / 8] &= (uint8_t)~(1u << (cluster % 8));
}

static int BitmapFlush(void)
{
    for (int i = 0; i < EVRYFS_BITMAP_SECTORS; i++) {
        if (AtaWriteSector(EVRYFS_BITMAP_LBA + (uint32_t)i,
                           s_bitmap + i * 512) < 0) return -1;
    }
    return 0;
}

/* ---- MFT record I/O ---------------------------------------------------- */

static int MftReadRecord(uint32_t rec, EVRYFS_MFT_RECORD* out)
{
    if (rec >= EVRYFS_MFT_MAX_RECORDS) return -1;
    return AtaReadSector(EVRYFS_MFT_LBA + rec, (uint8_t*)out);
}

static int MftWriteRecord(uint32_t rec, const EVRYFS_MFT_RECORD* data)
{
    if (rec >= EVRYFS_MFT_MAX_RECORDS) return -1;
    return AtaWriteSector(EVRYFS_MFT_LBA + rec, (const uint8_t*)data);
}

/*++

Routine Description:

    Scans the MFT from EVRYFS_FIRST_USER_RECORD upward for the first
    record whose EVRYFS_FLAG_INUSE bit is clear.

Return Value:

    Record index on success. EVRYFS_INVALID_RECORD if the MFT is full.

--*/
static uint32_t MftAllocRecord(void)
{
    EVRYFS_MFT_RECORD rec;
    for (uint32_t i = EVRYFS_FIRST_USER_RECORD; i < EVRYFS_MFT_MAX_RECORDS; i++) {
        if (MftReadRecord(i, &rec) < 0) continue;
        if (!(rec.flags & EVRYFS_FLAG_INUSE)) return i;
    }
    return EVRYFS_INVALID_RECORD;
}

/* ---- Cluster allocation ------------------------------------------------ */

/*++

Routine Description:

    Allocates up to count contiguous free clusters. On a first pass the
    routine searches for a run of exactly count clusters. If none exists
    a second pass returns the largest available run (partial allocation).
    The caller is responsible for handling partial allocation by issuing
    additional calls, using multiple extents.

Arguments:

    count          - Desired contiguous cluster count.
    start_out      - Receives the first allocated cluster index.
    got_out        - Receives the actual count allocated.

Return Value:

    0 on success, -1 if no free clusters remain.

--*/
static int AllocClusters(uint32_t count,
                         uint32_t* start_out,
                         uint32_t* got_out)
{
    EVRYFS_BOOT* boot  = (EVRYFS_BOOT*)s_bootbuf;
    uint32_t     total = boot->total_clusters;
    uint32_t     run_start = 0;
    uint32_t     run_count = 0;

    for (uint32_t i = 0; i < total; i++) {
        if (!BitmapGet(i)) {
            if (run_count == 0) run_start = i;
            run_count++;
            if (run_count == count) {
                *start_out = run_start;
                *got_out   = count;
                for (uint32_t j = 0; j < count; j++) BitmapSet(run_start + j, 1);
                return BitmapFlush();
            }
        } else {
            run_count = 0;
        }
    }

    /* No run of count found -- find largest available run */
    uint32_t best_start = EVRYFS_INVALID_RECORD;
    uint32_t best_count = 0;
    run_count = 0;
    run_start = 0;

    for (uint32_t i = 0; i < total; i++) {
        if (!BitmapGet(i)) {
            if (run_count == 0) run_start = i;
            run_count++;
        } else {
            if (run_count > best_count) {
                best_count = run_count;
                best_start = run_start;
            }
            run_count = 0;
        }
    }
    if (run_count > best_count) {
        best_count = run_count;
        best_start = run_start;
    }

    if (best_count == 0) return -1;

    *start_out = best_start;
    *got_out   = best_count;
    for (uint32_t j = 0; j < best_count; j++) BitmapSet(best_start + j, 1);
    return BitmapFlush();
}

static int FreeClusters(uint32_t start, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) BitmapSet(start + i, 0);
    return BitmapFlush();
}

/* ---- Path parsing ------------------------------------------------------ */

/*++

Routine Description:

    Splits a path string such as "docs/readme.txt" or "/docs/readme.txt"
    into an array of component names, stripping leading and trailing
    slashes and handling consecutive slashes gracefully.

Arguments:

    path       - Null-terminated path string.
    components - Caller-supplied array of EVRYFS_MAX_DEPTH slots.
    depth      - Receives the number of components written.

--*/
static void PathParse(const char*               path,
                      char components[][EVRYFS_NAME_LEN],
                      int*                      depth)
{
    *depth = 0;
    if (!path || !*path) return;

    const char* p = path;
    if (*p == '/') p++;

    while (*p && *depth < EVRYFS_MAX_DEPTH) {
        if (*p == '/') { p++; continue; }
        int len = 0;
        while (p[len] && p[len] != '/') len++;
        int copy = (len < EVRYFS_NAME_LEN - 1) ? len : EVRYFS_NAME_LEN - 1;
        FsStrCpy(components[*depth], p, copy + 1);
        components[*depth][copy] = 0;
        (*depth)++;
        p += len;
    }
}

/*++

Routine Description:

    Scans the MFT for an in-use record belonging to directory dir_record
    whose name field matches name.

Arguments:

    dir_record - Parent directory record number to match against.
    name       - Filename component to search for.
    out        - Receives the matched record on success.
    rec_num    - Receives the matched record's MFT index.

Return Value:

    0 on success, -1 if not found.

--*/
static int LookupInDir(uint32_t          dir_record,
                       const char*       name,
                       EVRYFS_MFT_RECORD* out,
                       uint32_t*         rec_num)
{
    EVRYFS_MFT_RECORD rec;
    for (uint32_t i = EVRYFS_FIRST_USER_RECORD; i < EVRYFS_MFT_MAX_RECORDS; i++) {
        if (MftReadRecord(i, &rec) < 0) continue;
        if (!(rec.flags & EVRYFS_FLAG_INUSE)) continue;
        if (rec.parent_record != dir_record) continue;
        if (!FsStrEq(rec.name, name)) continue;
        *out     = rec;
        *rec_num = i;
        return 0;
    }
    return -1;
}

/*++

Routine Description:

    Resolves a full path string to the matching MFT record. An empty
    path or bare "/" resolves to the root directory (record 2).

Arguments:

    path    - Null-terminated path string.
    out     - Receives the resolved record.
    rec_num - Receives the resolved record's MFT index.

Return Value:

    0 on success, -1 if any path component cannot be resolved.

--*/
static int LookupPath(const char*        path,
                      EVRYFS_MFT_RECORD* out,
                      uint32_t*          rec_num)
{
    char components[EVRYFS_MAX_DEPTH][EVRYFS_NAME_LEN];
    int  depth = 0;
    PathParse(path, components, &depth);

    if (depth == 0) {
        if (MftReadRecord(EVRYFS_RECORD_ROOT, out) < 0) return -1;
        *rec_num = EVRYFS_RECORD_ROOT;
        return 0;
    }

    uint32_t          cur_dir = EVRYFS_RECORD_ROOT;
    EVRYFS_MFT_RECORD rec;
    uint32_t          rn;

    for (int i = 0; i < depth; i++) {
        if (LookupInDir(cur_dir, components[i], &rec, &rn) < 0) return -1;
        cur_dir = rn;
    }

    *out     = rec;
    *rec_num = rn;
    return 0;
}

/*++

Routine Description:

    Resolves the parent directory of path and returns its record number.
    For a single-component path the parent is always the root directory.

Arguments:

    path           - Full path whose parent directory is wanted.
    parent_rec_out - Receives the parent directory record number.

Return Value:

    0 on success, -1 if the parent path cannot be resolved or is not
    a directory.

--*/
static int ResolveParent(const char* path, uint32_t* parent_rec_out)
{
    char components[EVRYFS_MAX_DEPTH][EVRYFS_NAME_LEN];
    int  depth = 0;
    PathParse(path, components, &depth);

    if (depth <= 1) {
        *parent_rec_out = EVRYFS_RECORD_ROOT;
        return 0;
    }

    uint32_t          cur = EVRYFS_RECORD_ROOT;
    EVRYFS_MFT_RECORD tmp;
    uint32_t          tn;

    for (int i = 0; i < depth - 1; i++) {
        if (LookupInDir(cur, components[i], &tmp, &tn) < 0) return -1;
        if (!(tmp.flags & EVRYFS_FLAG_ISDIR)) return -1;
        cur = tn;
    }

    *parent_rec_out = cur;
    return 0;
}

/* ---- Cluster data I/O -------------------------------------------------- */

/*++

Routine Description:

    Reads file data from an MFT record's extent run-list sequentially
    into buf, reading at most maxlen bytes.

Return Value:

    Bytes copied, or -1 on I/O error.

--*/
static int ReadExtents(const EVRYFS_MFT_RECORD* rec,
                       uint8_t*                 buf,
                       uint32_t                 maxlen)
{
    uint8_t  sector_buf[512];
    uint32_t remaining = rec->data_size;
    if (remaining > maxlen) remaining = maxlen;
    uint32_t dst = 0;

    for (int e = 0; e < rec->extent_count && remaining > 0; e++) {
        uint32_t lba =
            EVRYFS_DATA_START_LBA +
            rec->extents[e].start_cluster * EVRYFS_SECTORS_PER_CLUSTER;
        uint32_t sectors =
            rec->extents[e].cluster_count * EVRYFS_SECTORS_PER_CLUSTER;

        for (uint32_t s = 0; s < sectors && remaining > 0; s++) {
            if (AtaReadSector(lba + s, sector_buf) < 0) return -1;
            uint32_t chunk = remaining > 512u ? 512u : remaining;
            FsMemCpy(buf + dst, sector_buf, (int)chunk);
            dst       += chunk;
            remaining -= chunk;
        }
    }

    return (int)dst;
}

/*++

Routine Description:

    Writes data sequentially into the clusters described by an MFT
    record's extent run-list. Each sector is zero-padded when the
    tail of a cluster is not covered by data.

Return Value:

    0 on success, -1 on I/O error.

--*/
static int WriteExtents(const EVRYFS_MFT_RECORD* rec,
                        const uint8_t*           data,
                        uint32_t                 len)
{
    uint8_t  sector_buf[512];
    uint32_t src = 0;

    for (int e = 0; e < rec->extent_count && src < len; e++) {
        uint32_t lba =
            EVRYFS_DATA_START_LBA +
            rec->extents[e].start_cluster * EVRYFS_SECTORS_PER_CLUSTER;
        uint32_t sectors =
            rec->extents[e].cluster_count * EVRYFS_SECTORS_PER_CLUSTER;

        for (uint32_t s = 0; s < sectors && src < len; s++) {
            FsMemSet(sector_buf, 0, 512);
            uint32_t chunk = len - src;
            if (chunk > 512u) chunk = 512u;
            FsMemCpy(sector_buf, data + src, (int)chunk);
            if (AtaWriteSector(lba + s, sector_buf) < 0) return -1;
            src += chunk;
        }
    }

    return 0;
}

/* ---- Boot sector helpers ----------------------------------------------- */

static EVRYFS_BOOT* Boot(void) { return (EVRYFS_BOOT*)s_bootbuf; }

static int BootFlush(void)
{
    return AtaWriteSector(EVRYFS_BOOT_LBA, s_bootbuf);
}

/* ---- Public API -------------------------------------------------------- */

/*++

Routine Description:

    Formats the volume: writes boot sector, clears bitmap, writes all
    128 MFT sectors zeroed, then populates the three reserved records
    ($Volume, $Bitmap, root directory).

Arguments:

    volume_label - Null-terminated label string (max 31 chars).

Return Value:

    0 on success, -1 on I/O error.

--*/
int EvryFsFormat(const char* volume_label)
{
    uint8_t zero[512];
    FsMemSet(zero, 0, 512);
    FsMemSet(s_bitmap, 0, sizeof(s_bitmap));
    FsMemSet(s_bootbuf, 0, 512);

    EVRYFS_BOOT* boot = Boot();
    boot->jump[0]             = 0xEB;
    boot->jump[1]             = 0x58;
    boot->jump[2]             = 0x90;
    boot->oem_id[0] = 'E'; boot->oem_id[1] = 'V'; boot->oem_id[2] = 'R';
    boot->oem_id[3] = 'Y'; boot->oem_id[4] = 'F'; boot->oem_id[5] = 'S';
    boot->oem_id[6] = ' '; boot->oem_id[7] = ' ';
    boot->bytes_per_sector    = 512;
    boot->sectors_per_cluster = EVRYFS_SECTORS_PER_CLUSTER;
    boot->total_sectors       = 0;
    boot->mft_start_lba       = EVRYFS_MFT_LBA;
    boot->mft_record_count    = EVRYFS_MFT_MAX_RECORDS;
    boot->bitmap_start_lba    = EVRYFS_BITMAP_LBA;
    boot->bitmap_sectors      = EVRYFS_BITMAP_SECTORS;
    boot->data_start_lba      = EVRYFS_DATA_START_LBA;
    boot->total_clusters      = EVRYFS_BITMAP_BITS;
    boot->magic               = EVRYFS_MAGIC;
    boot->version             = EVRYFS_VERSION;
    boot->serial_number       = 0x45565259u;
    FsStrCpy(boot->volume_label,
             volume_label ? volume_label : "Everywhere", 32);

    if (AtaWriteSector(EVRYFS_BOOT_LBA, s_bootbuf) < 0) return -1;

    for (int i = 0; i < EVRYFS_BITMAP_SECTORS; i++) {
        if (AtaWriteSector(EVRYFS_BITMAP_LBA + (uint32_t)i, zero) < 0)
            return -1;
    }

    for (int i = 0; i < EVRYFS_MFT_SECTORS; i++) {
        if (AtaWriteSector(EVRYFS_MFT_LBA + (uint32_t)i, zero) < 0)
            return -1;
    }

    EVRYFS_MFT_RECORD rec;

    /* $Volume record */
    FsMemSet(&rec, 0, sizeof(rec));
    rec.signature      = EVRYFS_MFT_MAGIC;
    rec.flags          = EVRYFS_FLAG_INUSE;
    rec.record_number  = EVRYFS_RECORD_VOLUME;
    rec.parent_record  = EVRYFS_INVALID_RECORD;
    rec.file_attributes = EVRYFS_ATTR_SYSTEM | EVRYFS_ATTR_HIDDEN;
    FsStrCpy(rec.name, "$Volume", EVRYFS_NAME_LEN);
    rec.name_len = 7;
    if (MftWriteRecord(EVRYFS_RECORD_VOLUME, &rec) < 0) return -1;

    /* $Bitmap record */
    FsMemSet(&rec, 0, sizeof(rec));
    rec.signature      = EVRYFS_MFT_MAGIC;
    rec.flags          = EVRYFS_FLAG_INUSE;
    rec.record_number  = EVRYFS_RECORD_BITMAP;
    rec.parent_record  = EVRYFS_INVALID_RECORD;
    rec.file_attributes = EVRYFS_ATTR_SYSTEM | EVRYFS_ATTR_HIDDEN;
    FsStrCpy(rec.name, "$Bitmap", EVRYFS_NAME_LEN);
    rec.name_len = 7;
    if (MftWriteRecord(EVRYFS_RECORD_BITMAP, &rec) < 0) return -1;

    /* Root directory record */
    FsMemSet(&rec, 0, sizeof(rec));
    rec.signature       = EVRYFS_MFT_MAGIC;
    rec.flags           = EVRYFS_FLAG_INUSE | EVRYFS_FLAG_ISDIR;
    rec.record_number   = EVRYFS_RECORD_ROOT;
    rec.parent_record   = EVRYFS_INVALID_RECORD;
    rec.file_attributes = EVRYFS_ATTR_DIRECTORY;
    FsStrCpy(rec.name, ".", EVRYFS_NAME_LEN);
    rec.name_len = 1;
    if (MftWriteRecord(EVRYFS_RECORD_ROOT, &rec) < 0) return -1;

    s_present = 1;
    return 0;
}

/*++

Routine Description:

    Detects the ATA drive and loads the boot sector. If the magic number
    is absent or the version does not match, the volume is formatted in
    place with the label "Everywhere". On a valid existing volume the
    cluster bitmap is loaded into the in-memory cache.

Return Value:

    0 on success, -1 if no ATA drive is detected.

--*/
int EvryFsInit(void)
{
    s_present = 0;

    if (AtaReadSector(EVRYFS_BOOT_LBA, s_bootbuf) < 0) return -1;

    EVRYFS_BOOT* boot = Boot();
    if (boot->magic != EVRYFS_MAGIC || boot->version != EVRYFS_VERSION)
        return EvryFsFormat("Everywhere");

    for (int i = 0; i < EVRYFS_BITMAP_SECTORS; i++) {
        if (AtaReadSector(EVRYFS_BITMAP_LBA + (uint32_t)i,
                          s_bitmap + i * 512) < 0) return -1;
    }

    s_present = 1;
    return 0;
}

/*++

Routine Description:

    Creates or overwrites a file at path with the given data. The path
    may include subdirectory components, all of which must already exist.
    Old file extents are freed before new clusters are allocated. Data
    is written via the extent run-list using WriteExtents.

Arguments:

    path - Null-terminated file path (e.g. "docs/readme.txt").
    data - Pointer to file contents.
    len  - File size in bytes.

Return Value:

    0 on success. -1 on error (no disk, parent not found, MFT full,
    disk full, or I/O error).

--*/
int EvryFsWriteFile(const char* path, const uint8_t* data, uint32_t len)
{
    if (!s_present) return -1;

    char components[EVRYFS_MAX_DEPTH][EVRYFS_NAME_LEN];
    int  depth = 0;
    PathParse(path, components, &depth);
    if (depth == 0) return -1;

    uint32_t parent_rec;
    if (ResolveParent(path, &parent_rec) < 0) return -1;

    const char* filename = components[depth - 1];

    EVRYFS_MFT_RECORD existing;
    uint32_t          existing_rn;
    int               overwrite =
        (LookupInDir(parent_rec, filename, &existing, &existing_rn) == 0);

    if (overwrite) {
        for (int i = 0; i < existing.extent_count; i++)
            FreeClusters(existing.extents[i].start_cluster,
                         existing.extents[i].cluster_count);
    } else {
        existing_rn = MftAllocRecord();
        if (existing_rn == EVRYFS_INVALID_RECORD) return -1;
    }

    uint32_t needed = (len + EVRYFS_CLUSTER_SIZE - 1) / EVRYFS_CLUSTER_SIZE;
    if (needed == 0) needed = 1;

    EVRYFS_MFT_RECORD new_rec;
    FsMemSet(&new_rec, 0, sizeof(new_rec));
    new_rec.signature      = EVRYFS_MFT_MAGIC;
    new_rec.flags          = EVRYFS_FLAG_INUSE;
    new_rec.record_number  = existing_rn;
    new_rec.parent_record  = parent_rec;
    new_rec.file_attributes = EVRYFS_ATTR_ARCHIVE;
    new_rec.data_size      = len;
    new_rec.extent_count   = 0;
    FsStrCpy(new_rec.name, filename, EVRYFS_NAME_LEN);
    new_rec.name_len = (uint8_t)FsStrLen(filename);

    uint32_t remaining = needed;
    while (remaining > 0) {
        if (new_rec.extent_count >= EVRYFS_MAX_EXTENTS) {
            for (int i = 0; i < new_rec.extent_count; i++)
                FreeClusters(new_rec.extents[i].start_cluster,
                             new_rec.extents[i].cluster_count);
            return -1;
        }
        uint32_t start, got;
        if (AllocClusters(remaining, &start, &got) < 0) {
            for (int i = 0; i < new_rec.extent_count; i++)
                FreeClusters(new_rec.extents[i].start_cluster,
                             new_rec.extents[i].cluster_count);
            return -1;
        }
        new_rec.extents[new_rec.extent_count].start_cluster = start;
        new_rec.extents[new_rec.extent_count].cluster_count = got;
        new_rec.extent_count++;
        remaining -= got;
    }

    new_rec.alloc_size = needed * EVRYFS_CLUSTER_SIZE;

    if (WriteExtents(&new_rec, data, len) < 0) {
        for (int i = 0; i < new_rec.extent_count; i++)
            FreeClusters(new_rec.extents[i].start_cluster,
                         new_rec.extents[i].cluster_count);
        return -1;
    }

    if (MftWriteRecord(existing_rn, &new_rec) < 0) {
        for (int i = 0; i < new_rec.extent_count; i++)
            FreeClusters(new_rec.extents[i].start_cluster,
                         new_rec.extents[i].cluster_count);
        return -1;
    }
    return 0;
}

/*++

Routine Description:

    Reads up to maxlen bytes from the file at path into buf.

Return Value:

    Bytes read, or -1 if not found, is a directory, or I/O error.

--*/
int EvryFsReadFile(const char* path, uint8_t* buf, uint32_t maxlen)
{
    if (!s_present) return -1;
    EVRYFS_MFT_RECORD rec;
    uint32_t          rn;
    if (LookupPath(path, &rec, &rn) < 0) return -1;
    if (rec.flags & EVRYFS_FLAG_ISDIR) return -1;
    return ReadExtents(&rec, buf, maxlen);
}

/*++

Routine Description:

    Deletes the file at path. All clusters in the extent run-list are
    freed and the MFT record is zeroed. Directories may not be deleted
    with this function; use EvryFsRmDir instead.

Return Value:

    0 on success, -1 on error.

--*/
int EvryFsDeleteFile(const char* path)
{
    if (!s_present) return -1;
    EVRYFS_MFT_RECORD rec;
    uint32_t          rn;
    if (LookupPath(path, &rec, &rn) < 0) return -1;
    if (rec.flags & EVRYFS_FLAG_ISDIR) return -1;
    for (int i = 0; i < rec.extent_count; i++)
        FreeClusters(rec.extents[i].start_cluster,
                     rec.extents[i].cluster_count);
    EVRYFS_MFT_RECORD empty;
    FsMemSet(&empty, 0, sizeof(empty));
    return MftWriteRecord(rn, &empty);
}

/*++

Routine Description:

    Creates a new directory at path. All parent components must already
    exist. The new record carries EVRYFS_FLAG_ISDIR and EVRYFS_ATTR_DIRECTORY
    and has no data extents.

Return Value:

    0 on success, -1 on error or if the name already exists.

--*/
int EvryFsMkDir(const char* path)
{
    if (!s_present) return -1;

    char components[EVRYFS_MAX_DEPTH][EVRYFS_NAME_LEN];
    int  depth = 0;
    PathParse(path, components, &depth);
    if (depth == 0) return -1;

    uint32_t parent_rec;
    if (ResolveParent(path, &parent_rec) < 0) return -1;

    const char* dirname = components[depth - 1];

    EVRYFS_MFT_RECORD existing;
    uint32_t          ern;
    if (LookupInDir(parent_rec, dirname, &existing, &ern) == 0) return -1;

    uint32_t rn = MftAllocRecord();
    if (rn == EVRYFS_INVALID_RECORD) return -1;

    EVRYFS_MFT_RECORD rec;
    FsMemSet(&rec, 0, sizeof(rec));
    rec.signature       = EVRYFS_MFT_MAGIC;
    rec.flags           = EVRYFS_FLAG_INUSE | EVRYFS_FLAG_ISDIR;
    rec.record_number   = rn;
    rec.parent_record   = parent_rec;
    rec.file_attributes = EVRYFS_ATTR_DIRECTORY;
    FsStrCpy(rec.name, dirname, EVRYFS_NAME_LEN);
    rec.name_len = (uint8_t)FsStrLen(dirname);

    return MftWriteRecord(rn, &rec);
}

/*++

Routine Description:

    Removes an empty directory. Returns -1 if path does not name a
    directory or if the directory still contains any entries.

Return Value:

    0 on success, -1 on error.

--*/
int EvryFsRmDir(const char* path)
{
    if (!s_present) return -1;
    EVRYFS_MFT_RECORD rec;
    uint32_t          rn;
    if (LookupPath(path, &rec, &rn) < 0) return -1;
    if (!(rec.flags & EVRYFS_FLAG_ISDIR)) return -1;

    EVRYFS_MFT_RECORD child;
    for (uint32_t i = EVRYFS_FIRST_USER_RECORD; i < EVRYFS_MFT_MAX_RECORDS; i++) {
        if (MftReadRecord(i, &child) < 0) continue;
        if (!(child.flags & EVRYFS_FLAG_INUSE)) continue;
        if (child.parent_record == rn) return -1;
    }

    EVRYFS_MFT_RECORD empty;
    FsMemSet(&empty, 0, sizeof(empty));
    return MftWriteRecord(rn, &empty);
}

/*++

Routine Description:

    Moves or renames a file or directory. The destination must not
    already exist. If new_path contains a different directory component
    the record's parent_record is updated accordingly.

Return Value:

    0 on success, -1 on error.

--*/
int EvryFsRename(const char* old_path, const char* new_path)
{
    if (!s_present) return -1;
    EVRYFS_MFT_RECORD rec;
    uint32_t          rn;
    if (LookupPath(old_path, &rec, &rn) < 0) return -1;

    char new_components[EVRYFS_MAX_DEPTH][EVRYFS_NAME_LEN];
    int  new_depth = 0;
    PathParse(new_path, new_components, &new_depth);
    if (new_depth == 0) return -1;

    uint32_t new_parent;
    if (ResolveParent(new_path, &new_parent) < 0) return -1;

    const char* new_name = new_components[new_depth - 1];

    EVRYFS_MFT_RECORD existing;
    uint32_t          ern;
    if (LookupInDir(new_parent, new_name, &existing, &ern) == 0) return -1;

    FsStrCpy(rec.name, new_name, EVRYFS_NAME_LEN);
    rec.name_len      = (uint8_t)FsStrLen(new_name);
    rec.parent_record = new_parent;

    return MftWriteRecord(rn, &rec);
}

/*++

Routine Description:

    Fills out with metadata for the file or directory at path.

Return Value:

    0 on success, -1 if not found.

--*/
int EvryFsStat(const char* path, EVRYFS_STAT* out)
{
    if (!s_present) return -1;
    EVRYFS_MFT_RECORD rec;
    uint32_t          rn;
    if (LookupPath(path, &rec, &rn) < 0) return -1;
    FsStrCpy(out->name, rec.name, EVRYFS_NAME_LEN);
    out->file_attributes = rec.file_attributes;
    out->data_size       = rec.data_size;
    out->alloc_size      = rec.alloc_size;
    out->create_time     = rec.create_time;
    out->modify_time     = rec.modify_time;
    out->record_number   = rn;
    out->parent_record   = rec.parent_record;
    return 0;
}

/*++

Routine Description:

    Reads the file_attributes field of the record at path into attrs.

Return Value:

    0 on success, -1 if not found.

--*/
int EvryFsGetAttr(const char* path, uint32_t* attrs)
{
    if (!s_present) return -1;
    EVRYFS_MFT_RECORD rec;
    uint32_t          rn;
    if (LookupPath(path, &rec, &rn) < 0) return -1;
    *attrs = rec.file_attributes;
    return 0;
}

/*++

Routine Description:

    Writes attrs to the file_attributes field of the record at path.
    The EVRYFS_ATTR_DIRECTORY flag cannot be cleared by this function;
    it is preserved automatically for directory records.

Return Value:

    0 on success, -1 if not found or I/O error.

--*/
int EvryFsSetAttr(const char* path, uint32_t attrs)
{
    if (!s_present) return -1;
    EVRYFS_MFT_RECORD rec;
    uint32_t          rn;
    if (LookupPath(path, &rec, &rn) < 0) return -1;
    if (rec.flags & EVRYFS_FLAG_ISDIR)
        attrs |= EVRYFS_ATTR_DIRECTORY;
    else
        attrs &= ~EVRYFS_ATTR_DIRECTORY;
    rec.file_attributes = attrs;
    return MftWriteRecord(rn, &rec);
}

/*++

Routine Description:

    Copies the volume label from the boot sector into label.

Return Value:

    0 on success, -1 if no disk is present.

--*/
int EvryFsGetVolumeLabel(char* label, int maxlen)
{
    if (!s_present) return -1;
    FsStrCpy(label, Boot()->volume_label, maxlen < 32 ? maxlen : 32);
    return 0;
}

/*++

Routine Description:

    Writes label to the volume_label field in the boot sector and
    flushes the sector to disk.

Return Value:

    0 on success, -1 on I/O error.

--*/
int EvryFsSetVolumeLabel(const char* label)
{
    if (!s_present) return -1;
    FsStrCpy(Boot()->volume_label, label, 32);
    return BootFlush();
}

/*++

Routine Description:

    Fills parallel arrays with the entries of the directory at path.
    Results are limited to EVRYFS_LIST_MAX entries. attrs may be NULL
    if the caller does not need attribute information.

Arguments:

    path  - Directory path to enumerate. NULL or "" enumerates root.
    names - Caller-supplied array of EVRYFS_LIST_MAX string slots.
    sizes - Caller-supplied array of EVRYFS_LIST_MAX uint32_t slots.
    attrs - Optional array of EVRYFS_LIST_MAX uint32_t slots.
    count - Receives the number of entries written.

--*/
void EvryFsList(const char*           path,
                char                  names[][EVRYFS_NAME_LEN],
                uint32_t*             sizes,
                uint32_t*             attrs,
                int*                  count)
{
    *count = 0;
    if (!s_present) return;

    uint32_t dir_rn;
    if (!path || !*path || FsStrEq(path, "/")) {
        dir_rn = EVRYFS_RECORD_ROOT;
    } else {
        EVRYFS_MFT_RECORD dir_rec;
        if (LookupPath(path, &dir_rec, &dir_rn) < 0) return;
        if (!(dir_rec.flags & EVRYFS_FLAG_ISDIR)) return;
    }

    EVRYFS_MFT_RECORD rec;
    for (uint32_t i = EVRYFS_FIRST_USER_RECORD;
         i < EVRYFS_MFT_MAX_RECORDS && *count < EVRYFS_LIST_MAX;
         i++) {
        if (MftReadRecord(i, &rec) < 0) continue;
        if (!(rec.flags & EVRYFS_FLAG_INUSE)) continue;
        if (rec.parent_record != dir_rn) continue;
        FsStrCpy(names[*count], rec.name, EVRYFS_NAME_LEN);
        sizes[*count] = rec.data_size;
        if (attrs) {
            attrs[*count] = rec.file_attributes;
            if (rec.flags & EVRYFS_FLAG_ISDIR)
                attrs[*count] |= EVRYFS_ATTR_DIRECTORY;
        }
        (*count)++;
    }
}
