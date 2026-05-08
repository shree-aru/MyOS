/* ============================================================================
 * MyOS - Custom MyFS flat file system header
 * ============================================================================ */

#ifndef MYFS_H
#define MYFS_H

#include "kernel.h"

#define MYFS_MAGIC          0x594D4F53      /* "MYOS" in ASCII */
#define MYFS_MAX_FILES      64
#define MYFS_FILENAME_MAX   20

#define MYFS_SUPER_SECTOR   200              /* Superblock resides on Sector 200 */
#define MYFS_DIR_START      201              /* Directory entries reside on Sectors 201-204 */
#define MYFS_DATA_START     205              /* Data blocks reside on Sectors 205+ */

typedef struct {
    char     filename[MYFS_FILENAME_MAX];
    uint32_t start_sector;
    uint32_t file_size;       /* In bytes */
    uint32_t active;          /* 1 if valid, 0 if deleted */
} __attribute__((packed)) myfs_entry_t;

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

#endif /* MYFS_H */
