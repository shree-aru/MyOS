/* ============================================================================
 * MyOS - Custom MyFS flat file system implementation
 * ============================================================================ */

#include "myfs.h"
#include "ata.h"

static myfs_superblock_t superblock;
static myfs_entry_t directory[MYFS_MAX_FILES];

// Read directory entries from sectors 201-204 into RAM
static void myfs_read_directory(void) {
    uint8_t* ptr = (uint8_t*)directory;
    for (int i = 0; i < 4; i++) {
        ata_read_sector(MYFS_DIR_START + i, ptr + (i * 512));
    }
}

// Write RAM directory entries back to sectors 201-204
static void myfs_write_directory(void) {
    uint8_t* ptr = (uint8_t*)directory;
    for (int i = 0; i < 4; i++) {
        ata_write_sector(MYFS_DIR_START + i, ptr + (i * 512));
    }
}

// Read superblock from sector 200 into RAM
static void myfs_read_superblock(void) {
    ata_read_sector(MYFS_SUPER_SECTOR, (uint8_t*)&superblock);
}

// Write RAM superblock back to sector 200
static void myfs_write_superblock(void) {
    ata_write_sector(MYFS_SUPER_SECTOR, (uint8_t*)&superblock);
}

void myfs_init(void) {
    if (!ata_is_present()) {
        vga_puts("MyFS: IDE drive not present. Running in RAM-only mode.\n");
        return;
    }
    myfs_read_superblock();
    
    if (superblock.magic != MYFS_MAGIC) {
        vga_puts("MyFS: Valid filesystem not found. Formatting disk...\n");
        myfs_format();
    } else {
        vga_puts("MyFS: Filesystem mounted successfully!\n");
        myfs_read_directory();
    }
}

void myfs_format(void) {
    // 1. Initialize superblock
    memset(&superblock, 0, sizeof(myfs_superblock_t));
    superblock.magic = MYFS_MAGIC;
    superblock.version = 1;
    superblock.total_sectors = 40000;          // Mock size of ~20MB
    superblock.free_sector_start = MYFS_DATA_START;
    myfs_write_superblock();

    // 2. Clear directory entries
    memset(directory, 0, sizeof(directory));
    myfs_write_directory();
    
    vga_puts("MyFS: Format complete. 64 directory slots available.\n");
}

void myfs_list(void) {
    vga_puts("MyFS Directory Listing:\n");
    vga_puts("---------------------------------------------\n");
    vga_puts("Name                 Sector    Size (Bytes)\n");
    vga_puts("---------------------------------------------\n");
    
    int count = 0;
    for (int i = 0; i < MYFS_MAX_FILES; i++) {
        if (directory[i].active == 1) {
            // Guarantee null termination of filename for safety
            char name_buf[MYFS_FILENAME_MAX + 1];
            memcpy(name_buf, directory[i].filename, MYFS_FILENAME_MAX);
            name_buf[MYFS_FILENAME_MAX] = '\0';

            // Print filename
            vga_puts(name_buf);
            
            // Print alignment spaces
            int len = strlen(name_buf);
            if (len > 20) len = 20;
            for (int s = 0; s < 21 - len; s++) {
                vga_putchar(' ');
            }
            
            // Print start sector
            vga_put_dec(directory[i].start_sector);
            vga_puts("         ");
            
            // Print file size
            vga_put_dec(directory[i].file_size);
            vga_puts("\n");
            count++;
        }
    }
    
    if (count == 0) {
        vga_puts("(Disk is empty)\n");
    }
    vga_puts("---------------------------------------------\n");
    con_flush();
}

bool myfs_create(const char* name) {
    // Check if file already exists
    for (int i = 0; i < MYFS_MAX_FILES; i++) {
        if (directory[i].active == 1 && strcmp(directory[i].filename, name) == 0) {
            return false; // File exists
        }
    }
    
    // Find an empty slot
    for (int i = 0; i < MYFS_MAX_FILES; i++) {
        if (directory[i].active == 0) {
            memset(directory[i].filename, 0, MYFS_FILENAME_MAX);
            strncpy(directory[i].filename, name, MYFS_FILENAME_MAX - 1);
            directory[i].start_sector = 0;
            directory[i].file_size = 0;
            directory[i].active = 1;
            
            myfs_write_directory();
            return true;
        }
    }
    return false; // Directory full
}

bool myfs_write(const char* name, const uint8_t* data, uint32_t size) {
    int file_idx = -1;
    
    // Find existing file
    for (int i = 0; i < MYFS_MAX_FILES; i++) {
        if (directory[i].active == 1 && strcmp(directory[i].filename, name) == 0) {
            file_idx = i;
            break;
        }
    }
    
    // If not found, create new file
    if (file_idx == -1) {
        if (!myfs_create(name)) {
            return false;
        }
        // Re-locate newly created entry index
        for (int i = 0; i < MYFS_MAX_FILES; i++) {
            if (directory[i].active == 1 && strcmp(directory[i].filename, name) == 0) {
                file_idx = i;
                break;
            }
        }
    }
    
    if (file_idx == -1) return false;
    
    // Calculate sectors needed
    uint32_t num_sectors = (size + 511) / 512;
    if (num_sectors == 0) num_sectors = 1;
    
    uint32_t start_sector = superblock.free_sector_start;

    // Check for disk space exhaustion
    if (start_sector + num_sectors > superblock.total_sectors) {
        return false;
    }
    
    // Sector-by-sector write with 512-byte temporary buffering
    uint8_t temp_buf[512];
    for (uint32_t s = 0; s < num_sectors; s++) {
        uint32_t bytes_to_copy = size - (s * 512);
        if (bytes_to_copy > 512) bytes_to_copy = 512;
        
        memset(temp_buf, 0, 512);
        memcpy(temp_buf, data + (s * 512), bytes_to_copy);
        
        ata_write_sector(start_sector + s, temp_buf);
    }
    
    // Update directory entry
    directory[file_idx].start_sector = start_sector;
    directory[file_idx].file_size = size;
    
    // Advance free sector start
    superblock.free_sector_start += num_sectors;
    
    // Commit metadata changes to disk
    myfs_write_superblock();
    myfs_write_directory();
    
    return true;
}

int32_t myfs_read(const char* name, uint8_t* buffer, uint32_t max_size) {
    int file_idx = -1;
    
    // Find file
    for (int i = 0; i < MYFS_MAX_FILES; i++) {
        if (directory[i].active == 1 && strcmp(directory[i].filename, name) == 0) {
            file_idx = i;
            break;
        }
    }
    
    if (file_idx == -1) return -1; // File not found
    
    uint32_t size_to_read = directory[file_idx].file_size;
    if (size_to_read > max_size) size_to_read = max_size;
    
    uint32_t num_sectors = (size_to_read + 511) / 512;
    uint32_t start_sector = directory[file_idx].start_sector;
    
    // Sector-by-sector read with 512-byte temp buffering
    uint8_t temp_buf[512];
    for (uint32_t s = 0; s < num_sectors; s++) {
        uint32_t offset = s * 512;
        uint32_t bytes_to_copy = size_to_read - offset;
        if (bytes_to_copy > 512) bytes_to_copy = 512;
        
        ata_read_sector(start_sector + s, temp_buf);
        memcpy(buffer + offset, temp_buf, bytes_to_copy);
    }
    
    return size_to_read;
}

bool myfs_delete(const char* name) {
    for (int i = 0; i < MYFS_MAX_FILES; i++) {
        if (directory[i].active == 1 && strcmp(directory[i].filename, name) == 0) {
            directory[i].active = 0; // Soft delete entry
            myfs_write_directory();
            return true;
        }
    }
    return false;
}

myfs_entry_t* myfs_get_entry(int index) {
    if (index < 0 || index >= MYFS_MAX_FILES) return NULL;
    if (directory[index].active != 1) return NULL;
    return &directory[index];
}

int myfs_count_files(void) {
    int count = 0;
    for (int i = 0; i < MYFS_MAX_FILES; i++) {
        if (directory[i].active == 1) count++;
    }
    return count;
}
