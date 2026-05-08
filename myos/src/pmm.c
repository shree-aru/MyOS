/* ============================================================================
 * MyOS - Physical Memory Manager
 * Bitmap-based physical frame allocator using Multiboot memory map
 * ============================================================================ */

#include "kernel.h"

#define BITMAP_SIZE 32768   /* 32K entries = 32K * 32 bits * 4KB = 4 GB coverage */

static uint32_t bitmap[BITMAP_SIZE];
static uint32_t total_frames = 0;
static uint32_t used_frames  = 0;
static uint32_t total_memory = 0;  /* In KB */

/* Bitmap helpers */
static inline void bitmap_set(uint32_t frame) {
    bitmap[frame / 32] |= (1 << (frame % 32));
}

static inline void bitmap_clear(uint32_t frame) {
    bitmap[frame / 32] &= ~(1 << (frame % 32));
}

static inline bool bitmap_test(uint32_t frame) {
    return bitmap[frame / 32] & (1 << (frame % 32));
}

void pmm_init(multiboot_info_t* mbi) {
    /* Mark all frames as used initially */
    memset(bitmap, 0xFF, sizeof(bitmap));
    used_frames = 0;
    total_frames = 0;

    if (!(mbi->flags & MULTIBOOT_FLAG_MMAP)) {
        /* Fallback: use mem_upper (in KB, starting at 1MB) */
        total_memory = mbi->mem_upper + 1024;
        total_frames = total_memory / 4;  /* 4KB pages */

        /* Mark available memory above 2MB as free (below is kernel + boot) */
        for (uint32_t i = 512; i < total_frames && i < BITMAP_SIZE * 32; i++) {
            bitmap_clear(i);
        }
        used_frames = 512;
        return;
    }

    /* Parse multiboot memory map */
    uint32_t mmap_end = mbi->mmap_addr + mbi->mmap_length;
    mmap_entry_t* entry = (mmap_entry_t*)mbi->mmap_addr;

    while ((uint32_t)entry < mmap_end) {
        if (entry->type == 1) {  /* Available memory */
            uint64_t base = entry->base_addr;
            uint64_t end  = base + entry->length;

            /* Track total memory */
            if (end / 1024 > total_memory)
                total_memory = (uint32_t)(end / 1024);

            /* Mark frames as free */
            uint32_t frame_start = (uint32_t)((base + PAGE_SIZE - 1) / PAGE_SIZE);
            uint32_t frame_end   = (uint32_t)(end / PAGE_SIZE);

            if (frame_end > BITMAP_SIZE * 32)
                frame_end = BITMAP_SIZE * 32;

            for (uint32_t f = frame_start; f < frame_end; f++) {
                bitmap_clear(f);
            }
        }

        total_frames = total_memory / 4;

        /* Move to next entry (size field doesn't include itself) */
        entry = (mmap_entry_t*)((uint32_t)entry + entry->size + 4);
    }

    /* Reserve first 2MB for kernel, bootloader, BIOS, etc. */
    for (uint32_t i = 0; i < 512; i++) {
        bitmap_set(i);
    }

    /* Reserve kernel area specifically */
    uint32_t kernel_start_frame = (uint32_t)&_kernel_start / PAGE_SIZE;
    uint32_t kernel_end_frame   = ((uint32_t)&_kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = kernel_start_frame; i <= kernel_end_frame; i++) {
        bitmap_set(i);
    }

    /* Count used frames */
    used_frames = 0;
    for (uint32_t i = 0; i < total_frames && i < BITMAP_SIZE * 32; i++) {
        if (bitmap_test(i))
            used_frames++;
    }
}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t i = 0; i < BITMAP_SIZE; i++) {
        if (bitmap[i] != 0xFFFFFFFF) {
            /* Find first free bit */
            for (uint32_t j = 0; j < 32; j++) {
                if (!(bitmap[i] & (1 << j))) {
                    uint32_t frame = i * 32 + j;
                    bitmap_set(frame);
                    used_frames++;
                    return frame * PAGE_SIZE;
                }
            }
        }
    }
    return 0; /* Out of memory */
}

void pmm_free_frame(uint32_t addr) {
    uint32_t frame = addr / PAGE_SIZE;
    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        used_frames--;
    }
}

uint32_t pmm_get_total_memory(void) {
    return total_memory;
}

uint32_t pmm_get_used_frames(void) {
    return used_frames;
}

uint32_t pmm_get_free_frames(void) {
    if (total_frames > used_frames)
        return total_frames - used_frames;
    return 0;
}
