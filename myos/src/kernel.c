/* ============================================================================
 * MyOS - Kernel Main
 * Entry point: initializes all subsystems and launches the shell
 * ============================================================================ */

#include "kernel.h"

/* Kernel panic handler */
void kpanic(const char* msg) {
    cli();
    vga_setcolor(VGA_WHITE, VGA_RED);
    vga_puts("\n\n*** KERNEL PANIC ***\n");
    vga_puts(msg);
    vga_puts("\nSystem halted.\n");
    for (;;) hlt();
}

/* ============================================================================
 * Kernel Main Entry Point
 * Called from boot.asm after Multiboot setup
 * ============================================================================ */

void kmain(multiboot_info_t* mbi, uint32_t magic) {
    /* Clear uninitialized BSS section */
    uint8_t* bss = (uint8_t*)&_bss_start;
    uint8_t* bss_end = (uint8_t*)&_bss_end;
    while (bss < bss_end) {
        *bss++ = 0;
    }

    /* Initialize VESA framebuffer and console redirect as early as possible */
    fb_init(mbi);
    shell_init();

    /* ---- Phase 1: Core initialization (no interrupts yet) ---- */

    /* Initialize VGA text mode as early console */
    vga_init();
    vga_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("MyOS v0.1.0 - Booting...\n\n");
    vga_setcolor(VGA_WHITE, VGA_BLACK);

    /* Verify multiboot magic */
    if (magic != MULTIBOOT_MAGIC) {
        vga_setcolor(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("[FAIL] Not booted by a Multiboot-compliant loader!\n");
        vga_puts("       Magic: ");
        vga_put_hex(magic);
        vga_puts(" (expected: ");
        vga_put_hex(MULTIBOOT_MAGIC);
        vga_puts(")\n");
        kpanic("Invalid Multiboot magic number");
    }
    vga_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("[OK] ");
    vga_setcolor(VGA_WHITE, VGA_BLACK);
    vga_puts("Multiboot verified (magic: ");
    vga_put_hex(magic);
    vga_puts(")\n");

    /* Initialize GDT */
    gdt_init();
    vga_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("[OK] ");
    vga_setcolor(VGA_WHITE, VGA_BLACK);
    vga_puts("GDT initialized (5 entries: null, kcode, kdata, ucode, udata)\n");

    /* Initialize IDT and PIC */
    idt_init();
    vga_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("[OK] ");
    vga_setcolor(VGA_WHITE, VGA_BLACK);
    vga_puts("IDT initialized (256 entries, PIC remapped to 0x20-0x2F)\n");

    /* Initialize PIT Timer */
    timer_init();
    vga_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("[OK] ");
    vga_setcolor(VGA_WHITE, VGA_BLACK);
    vga_puts("PIT Timer initialized (100 Hz)\n");

    /* ---- Phase 2: Enable interrupts ---- */
    sti();

    /* Initialize physical memory manager */
    pmm_init(mbi);
    vga_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("[OK] ");
    vga_setcolor(VGA_WHITE, VGA_BLACK);
    vga_puts("PMM initialized (");
    vga_put_dec(pmm_get_total_memory() / 1024);
    vga_puts(" MB total, ");
    vga_put_dec(pmm_get_free_frames() * 4 / 1024);
    vga_puts(" MB free)\n");

    /* ---- Phase 3: Device drivers ---- */

    /* Initialize keyboard */
    keyboard_init();
    vga_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("[OK] ");
    vga_setcolor(VGA_WHITE, VGA_BLACK);
    vga_puts("PS/2 Keyboard initialized (IRQ1)\n");

    /* Initialize mouse */
    mouse_init();
    vga_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("[OK] ");
    vga_setcolor(VGA_WHITE, VGA_BLACK);
    vga_puts("PS/2 Mouse initialized (IRQ12)\n");

    /* Initialize ATA IDE Controller */
    ata_init();
    vga_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("[OK] ");
    vga_setcolor(VGA_WHITE, VGA_BLACK);
    vga_puts("ATA IDE Primary Master probed\n");

    /* Mount MyFS flat filesystem */
    myfs_init();

    /* Initialize Forth dynamic compiler environment */
    forth_init();

    /* Framebuffer is already initialized early; print status */
    framebuffer_t* fb = fb_get();
    if (fb->available) {
        vga_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_puts("[OK] ");
        vga_setcolor(VGA_WHITE, VGA_BLACK);
        vga_puts("Framebuffer initialized (");
        vga_put_dec(fb->width);
        vga_puts("x");
        vga_put_dec(fb->height);
        vga_puts("x");
        vga_put_dec(fb->bpp);
        vga_puts(" @ ");
        vga_put_hex((uint32_t)fb->buffer);
        vga_puts(")\n");
    } else {
        vga_setcolor(VGA_YELLOW, VGA_BLACK);
        vga_puts("[--] ");
        vga_setcolor(VGA_WHITE, VGA_BLACK);
        vga_puts("Framebuffer not available (text mode only)\n");
    }

    /* ---- Phase 4: Boot complete ---- */
    vga_puts("\n");
    vga_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("=== Boot complete ===\n");
    vga_setcolor(VGA_WHITE, VGA_BLACK);
    vga_puts("Kernel: ");
    vga_put_hex((uint32_t)&_kernel_start);
    vga_puts(" - ");
    vga_put_hex((uint32_t)&_kernel_end);
    vga_puts("\n\n");

    /* Brief pause to show boot messages */
    uint32_t start_tick = timer_get_ticks();
    while (timer_get_ticks() - start_tick < 200) {
        hlt();
    }

    /* ---- Phase 5: Launch shell ---- */
    shell_init();
    shell_run();

    /* Should never reach here */
    kpanic("Shell returned unexpectedly");
}

/* ============================================================================
 * System Power Control
 * ============================================================================ */

void sys_shutdown(void) {
    // 1. Bochs and older QEMU ACPI poweroff
    outw(0xB004, 0x2000);
    io_wait();
    
    // 2. QEMU and VirtualBox ACPI poweroff
    outw(0x604, 0x2000);
    io_wait();
    
    // 3. Newer QEMU and VirtualBox ACPI poweroff
    outw(0x604, 0x3400);
    io_wait();

    // 4. VirtualBox ACPI specific alternative
    outw(0x4004, 0x3400);
    io_wait();

    // 5. Fallback: Halt the CPU
    cli();
    for (;;) {
        hlt();
    }
}

void sys_reboot(void) {
    // Triple fault: load an empty IDT and trigger an interrupt
    idt_ptr_t null_idt = { 0, 0 };
    idt_load((uint32_t)&null_idt);
    __asm__ volatile("int $0x03");
    
    // Fallback loop if triple fault doesn't fire immediately
    cli();
    for (;;) {
        hlt();
    }
}
