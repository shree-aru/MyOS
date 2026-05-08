; ============================================================================
; MyOS - Multiboot Entry Point
; Sets up multiboot header, stack, and calls kmain
; ============================================================================

; Multiboot constants
MBALIGN     equ 1 << 0              ; Align loaded modules on page boundaries
MEMINFO     equ 1 << 1              ; Provide memory map
VIDINFO     equ 1 << 2              ; Provide video mode info
FLAGS       equ MBALIGN | MEMINFO | VIDINFO
MAGIC       equ 0x1BADB002          ; Multiboot magic number
CHECKSUM    equ -(MAGIC + FLAGS)     ; Checksum (magic + flags + checksum = 0)

; ============================================================================
; Multiboot Header (must be in first 8KB of kernel image)
; ============================================================================
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    ; Address fields (unused - GRUB reads ELF headers instead)
    dd 0        ; header_addr
    dd 0        ; load_addr
    dd 0        ; load_end_addr
    dd 0        ; bss_end_addr
    dd 0        ; entry_addr
    ; Video mode request (flags bit 2 set)
    dd 0        ; mode_type: 0 = linear graphics mode
    dd 1024     ; width
    dd 768      ; height
    dd 32       ; depth (bits per pixel)

; ============================================================================
; Stack (16 KB, isolated from BSS)
; ============================================================================
section .stack
align 16
stack_bottom:
    resb 16384          ; 16 KB kernel stack
stack_top:

; ============================================================================
; Entry Point
; ============================================================================
section .entry
global _start
extern kmain

_start:
    ; Set up kernel stack
    mov esp, stack_top

    ; Push multiboot parameters for kmain(mbi, magic)
    push eax            ; Second arg: multiboot magic
    push ebx            ; First arg: multiboot info pointer

    ; Call kernel main
    call kmain

    ; If kmain returns, halt the CPU
    cli
.hang:
    hlt
    jmp .hang

; ============================================================================
; GDT Flush - Load GDT and reload segment registers
; void gdt_flush(uint32_t gdt_ptr_address)
; ============================================================================
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]     ; Get GDT pointer address from stack
    lgdt [eax]             ; Load GDT

    ; Reload segment registers with kernel data segment (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS with kernel code segment (0x08)
    jmp 0x08:.flush
.flush:
    ret

; ============================================================================
; IDT Load - Load IDT register
; void idt_load(uint32_t idt_ptr_address)
; ============================================================================
global idt_load
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret
