/* ============================================================================
 * MyOS - Custom MyFS flat file system header
 * ============================================================================ */

#ifndef MYFS_H
#define MYFS_H

#include "kernel.h"

/* Sector layout constants (not needed by GUI, only by myfs.c internals) */
#define MYFS_SUPER_SECTOR   200              /* Superblock resides on Sector 200 */
#define MYFS_DIR_START      201              /* Directory entries reside on Sectors 201-204 */
#define MYFS_DATA_START     205              /* Data blocks reside on Sectors 205+ */

/* myfs_entry_t, MYFS_MAX_FILES, MYFS_FILENAME_MAX are defined in kernel.h */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t total_sectors;
    uint32_t free_sector_start;
    uint8_t  padding[496];     /* Pad superblock to exactly 512 bytes */
} __attribute__((packed)) myfs_superblock_t;

void myfs_init(void);
void myfs_format(void);
void myfs_list(void);
bool myfs_create(const char* name);
bool myfs_write(const char* name, const uint8_t* data, uint32_t size);
int32_t myfs_read(const char* name, uint8_t* buffer, uint32_t max_size);
bool myfs_delete(const char* name);
myfs_entry_t* myfs_get_entry(int index);
int myfs_count_files(void);

#endif /* MYFS_H */
