/* ============================================================================
 * MyOS Kernel Linker Script
 * Loads kernel at 1MB (standard Multiboot load address)
 * ============================================================================ */

ENTRY(_start)

SECTIONS
{
    /* Start at 1MB - standard location for Multiboot kernels */
    . = 1M;

    _kernel_start = .;

    /* Multiboot header must be in first 8KB for GRUB to find it */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.entry)
        *(.multiboot)
        *(.text)
    }

    /* Read-only data */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Initialized data */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    /* Uninitialized data */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        _bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        _bss_end = .;
    }

    /* Stack section to prevent BSS clearing from overwriting stack */
    .stack BLOCK(4K) : ALIGN(4K)
    {
        *(.stack)
    }

    _kernel_end = .;

    /* Discard unwanted sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
    }
}
