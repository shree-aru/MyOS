; ============================================================================
; MyOS - Custom Stage 2 Bootloader
; Executes at physical address 0x8000 in Real Mode, switches to Protected Mode,
; copies the kernel to 1MB, and jumps to the C main entry point.
; ============================================================================

[org 0x8000]
[bits 16]

stage2_entry:
    ; Reset segment registers to ensure safe, flat physical addressing (0x0000:0x7000)
    xor ax, ax
    mov es, ax
    mov ds, ax

    ; 1. Query VESA VBE Mode Info for Mode 0x4118 (1024x768x32 LFB)
    ; Buffer placed at safe memory address 0x7000
    mov ax, 0x4F01          ; Get VBE Mode Info
    mov cx, 0x4118          ; Mode 0x118 + 0x4000 (Linear Frame Buffer)
    mov di, 0x7000          ; ES:DI = 0x0000:0x7000
    int 0x10
    cmp ax, 0x004F          ; VBE function supported & successful?
    jne vbe_error

    ; 2. Set VBE Graphics Mode
    mov ax, 0x4F02          ; Set VBE Mode
    mov bx, 0x4118          ; Mode 0x4118
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    ; 3. Build Mock Multiboot Info Struct at address 0x1000
    mov di, 0x1000
    xor ax, ax
    mov es, ax
    
    ; Clear the mbi structure (128 bytes)
    mov cx, 32
    xor eax, eax
    rep stosd
    
    ; Populate Mock Multiboot fields
    ; mbi.flags: MULTIBOOT_FLAG_FB | MULTIBOOT_FLAG_MEM = 0x00001001
    mov dword [0x1000], 0x00001001
    ; mbi.mem_lower: 640 KB
    mov dword [0x1004], 640
    ; mbi.mem_upper: 63 MB (total 64MB)
    mov dword [0x1008], 63 * 1024

    ; Copy framebuffer data from VBE mode info (at 0x7000)
    ; mbi.framebuffer_addr (at offset 88): physical address at VBE mode info + 40
    mov eax, [0x7000 + 40]
    mov [0x1000 + 88], eax
    mov dword [0x1000 + 92], 0 ; Clear high 32-bit of 64-bit address

    ; mbi.framebuffer_pitch (at offset 96): pitch at VBE mode info + 16
    mov ax, [0x7000 + 16]
    movzx eax, ax
    mov [0x1000 + 96], eax

    ; mbi.framebuffer_width (at offset 100) = 1024
    mov dword [0x1000 + 100], 1024
    ; mbi.framebuffer_height (at offset 104) = 768
    mov dword [0x1000 + 104], 768
    ; mbi.framebuffer_bpp (at offset 108): bpp at VBE mode info + 25
    mov al, [0x7000 + 25]
    mov [0x1000 + 108], al
    ; mbi.framebuffer_type (at offset 109) = 1 (RGB)
    mov byte [0x1000 + 109], 1

    ; 4. Disable interrupts and load temporary GDT
    cli
    lgdt [gdt_descriptor]

    ; 5. Switch to Protected Mode (Set PE bit in CR0)
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; 6. Far jump to 32-bit Protected Mode (Selector 0x08 = Code)
    jmp 0x08:pm_entry

vbe_error:
    ; Text mode fallback if VBE fails (print error)
    mov ax, 0x0003          ; Reset to standard 80x25 text mode
    int 0x10
    mov si, vbe_err_msg
.err_loop:
    lodsb
    or al, al
    jz .err_halt
    mov ah, 0x0e
    int 0x10
    jmp .err_loop
.err_halt:
    cli
    hlt
    jmp .err_halt

vbe_err_msg db "MyOS: VESA VBE 1024x768x32 Mode Not Supported!", 13, 10, 0

; ============================================================================
; 32-bit Protected Mode Entry Point
; ============================================================================
[bits 32]
pm_entry:
    ; 1. Load Protected Mode data segments
    mov ax, 0x10            ; Selector 0x10 = Data
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000        ; Set up stack below 1MB

    ; 2. Copy Kernel Binary from RAM (loaded at 0x8200) to 1MB (0x100000)
    ; Source: 0x8200 (Stage 2 occupies Sector 1 at 0x8000, Kernel starts at 0x8200)
    mov esi, 0x8200
    mov edi, 0x100000       ; Destination: 1MB
    mov ecx, 24576          ; Copy 96 KB of kernel (24576 double-words)
    rep movsd

    ; 3. Boot the Kernel!
    ; kmain(multiboot_info_t* mbi, uint32_t magic)
    ; Register EAX must contain MULTIBOOT_MAGIC (0x2BADB002)
    ; Register EBX must contain pointer to multiboot info (0x1000)
    mov eax, 0x2BADB002
    mov ebx, 0x1000
    
    ; Jump straight to the C kernel entry point at 1MB
    jmp 0x100000

; ============================================================================
; Global Descriptor Table (GDT)
; ============================================================================
align 4
gdt_start:
    ; Null Descriptor (Selector 0x00)
    dd 0, 0
gdt_code:
    ; Code Descriptor (Selector 0x08): base=0, limit=4GB, Access=0x9A, Gran=0xCF
    dw 0xFFFF
    dw 0
    db 0
    db 10011010b
    db 11001111b
    db 0
gdt_data:
    ; Data Descriptor (Selector 0x10): base=0, limit=4GB, Access=0x92, Gran=0xCF
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b
    db 11001111b
    db 0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; Pad Stage 2 Bootloader to exactly 512 bytes (1 sector)
times 512-($-$$) db 0
