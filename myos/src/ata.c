/* ============================================================================
 * MyOS - Custom ATA (IDE) hard disk driver implementation (PIO Mode)
 * ============================================================================ */

#include "ata.h"
#include "kernel.h"

static bool ata_present = false;

void ata_init(void) {
    // Select primary master drive
    outb(0x1F6, 0xA0);
    io_wait();
    
    // Basic probe: read status to verify drive presence
    uint8_t status = inb(0x1F7);
    if (status == 0xFF) {
        ata_present = false;
    } else {
        // Double check by writing to a cylinder register and reading it back
        outb(0x1F4, 0x55);
        outb(0x1F5, 0xAA);
        io_wait();
        uint8_t cyl_low = inb(0x1F4);
        uint8_t cyl_high = inb(0x1F5);
        if (cyl_low == 0x55 && cyl_high == 0xAA) {
            ata_present = true;
        } else {
            ata_present = false;
        }
    }
}

bool ata_is_present(void) {
    return ata_present;
}

void ata_read_sector(uint32_t lba, uint8_t* buffer) {
    if (!ata_present) {
        memset(buffer, 0, 512);
        return;
    }

    // Select drive (LBA mode, primary master) and send upper bits of LBA
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    io_wait();
    
    // Send sector count (1 sector)
    outb(0x1F2, 1);
    
    // Send LBA address bytes
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    
    // Send command: READ SECTORS with retry
    outb(0x1F7, 0x20);

    // Wait until BSY (Busy, bit 7) clears
    uint32_t timeout = 100000;
    while ((inb(0x1F7) & 0x80) != 0) {
        if (--timeout == 0) {
            memset(buffer, 0, 512);
            return;
        }
    }
    
    // Wait until DRQ (Data Request, bit 3) is set
    timeout = 100000;
    while ((inb(0x1F7) & 0x08) == 0) {
        if (--timeout == 0) {
            memset(buffer, 0, 512);
            return;
        }
    }

    // Read 256 words (512 bytes) from the data port (0x1F0)
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(0x1F0);
    }
}

void ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    if (!ata_present) {
        return;
    }

    // Select drive (LBA mode, primary master) and send upper bits of LBA
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    io_wait();
    
    // Send sector count (1 sector)
    outb(0x1F2, 1);
    
    // Send LBA address bytes
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    
    // Send command: WRITE SECTORS with retry
    outb(0x1F7, 0x30);

    // Wait until BSY (Busy, bit 7) clears
    uint32_t timeout = 100000;
    while ((inb(0x1F7) & 0x80) != 0) {
        if (--timeout == 0) return;
    }
    
    // Wait until DRQ (Data Request, bit 3) is set
    timeout = 100000;
    while ((inb(0x1F7) & 0x08) == 0) {
        if (--timeout == 0) return;
    }

    // Write 256 words (512 bytes) to the data port (0x1F0)
    const uint16_t* ptr = (const uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        outw(0x1F0, ptr[i]);
    }
    
    // Flush cache to ensure data is committed to disk immediately
    outb(0x1F7, 0xE7); // CACHE FLUSH
    timeout = 100000;
    while ((inb(0x1F7) & 0x80) != 0) {
        if (--timeout == 0) return;
    }
}
