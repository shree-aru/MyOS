/* ============================================================================
 * MyOS - Custom ATA (IDE) hard disk driver header
 * ============================================================================ */

#ifndef ATA_H
#define ATA_H

#include "kernel.h"

void ata_init(void);
bool ata_is_present(void);
void ata_read_sector(uint32_t lba, uint8_t* buffer);
void ata_write_sector(uint32_t lba, const uint8_t* buffer);

#endif /* ATA_H */
