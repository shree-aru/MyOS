/* ============================================================================
 * MyOS - Global Descriptor Table
 * Flat memory model with kernel and user segments
 * ============================================================================ */

#include "kernel.h"

#define GDT_ENTRIES 5

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_pointer;

static void gdt_set_gate(int num, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_mid    = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;
    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity  = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access      = access;
}

void gdt_init(void) {
    gdt_pointer.limit = sizeof(gdt) - 1;
    gdt_pointer.base  = (uint32_t)&gdt;

    /* Null segment */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* Kernel code segment: base=0, limit=4GB, ring 0, executable */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    /* Kernel data segment: base=0, limit=4GB, ring 0, read/write */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* User code segment: base=0, limit=4GB, ring 3, executable */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    /* User data segment: base=0, limit=4GB, ring 3, read/write */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    /* Load GDT and reload segments */
    gdt_flush((uint32_t)&gdt_pointer);
}
