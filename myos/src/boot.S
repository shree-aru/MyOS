/* ============================================================================
 * MyOS - Multiboot Entry Point (GAS/AT&T Syntax)
 * Sets up multiboot header, stack, and calls kmain
 * ============================================================================ */

/* Multiboot constants */
.set MBALIGN,   1 << 0              /* Align loaded modules on page boundaries */
.set MEMINFO,   1 << 1              /* Provide memory map */
.set VIDINFO,   1 << 2              /* Provide video mode info */
.set FLAGS,     MBALIGN | MEMINFO | VIDINFO
.set MAGIC,     0x1BADB002          /* Multiboot magic number */
.set CHECKSUM,  -(MAGIC + FLAGS)    /* Checksum (magic + flags + checksum = 0) */

/* ============================================================================
 * Multiboot Header (must be in first 8KB of kernel image)
 * ============================================================================ */
.section .multiboot
.align 4
    .long MAGIC
    .long FLAGS
    .long CHECKSUM
    /* Address fields (unused - GRUB reads ELF headers instead) */
    .long 0         /* header_addr */
    .long 0         /* load_addr */
    .long 0         /* load_end_addr */
    .long 0         /* bss_end_addr */
    .long 0         /* entry_addr */
    /* Video mode request (flags bit 2 set) */
    .long 0         /* mode_type: 0 = linear graphics mode */
    .long 1024      /* width */
    .long 768       /* height */
    .long 32        /* depth (bits per pixel) */

/* ============================================================================
 * Stack (16 KB, isolated from BSS)
 * ============================================================================ */
.section .stack, "aw", @nobits
.align 16
stack_bottom:
    .skip 16384             /* 16 KB kernel stack */
stack_top:

/* ============================================================================
 * Entry Point
 * ============================================================================ */
.section .entry
.global _start
.extern kmain

_start:
    /* Set up kernel stack */
    movl $stack_top, %esp

    /* Push multiboot parameters for kmain(mbi, magic) */
    pushl %eax              /* Second arg: multiboot magic */
    pushl %ebx              /* First arg: multiboot info pointer */

    /* Call kernel main */
    call kmain

    /* If kmain returns, halt the CPU */
    cli
1:
    hlt
    jmp 1b

/* ============================================================================
 * GDT Flush - Load GDT and reload segment registers
 * void gdt_flush(uint32_t gdt_ptr_address)
 * ============================================================================ */
.global gdt_flush
gdt_flush:
    movl 4(%esp), %eax      /* Get GDT pointer address from stack */
    lgdt (%eax)              /* Load GDT */

    /* Reload segment registers with kernel data segment (0x10) */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    /* Far jump to reload CS with kernel code segment (0x08) */
    ljmp $0x08, $.gdt_done
.gdt_done:
    ret

/* ============================================================================
 * IDT Load - Load IDT register
 * void idt_load(uint32_t idt_ptr_address)
 * ============================================================================ */
.global idt_load
idt_load:
    movl 4(%esp), %eax
    lidt (%eax)
    ret
