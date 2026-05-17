; ============================================================================
; MyOS - Custom Stage 2 Bootloader with Boot Menu
; Executes at physical address 0x8000 in Real Mode.
; Displays a branded boot menu, then switches to Protected Mode,
; copies the kernel to 1MB, and jumps to the C main entry point.
; ============================================================================

[org 0x8000]
[bits 16]

stage2_entry:
    ; Reset segment registers to ensure safe, flat physical addressing
    xor ax, ax
    mov es, ax
    mov ds, ax

    ; ========================================================================
    ; Boot Menu - 16-bit Real Mode BIOS Text Interface
    ; ========================================================================

    ; 1. Reset to clean 80x25 text mode
    mov ax, 0x0003
    int 0x10

    ; 2. Hide the text-mode cursor (makes the menu look cleaner)
    mov ah, 0x01
    mov cx, 0x2607          ; CH=26h (start > 25 = hidden), CL=07h
    int 0x10

    ; 3. Print the boot menu
    mov si, menu_top
    call print_string
    mov si, menu_title
    call print_string
    mov si, menu_author
    call print_string
    mov si, menu_mid
    call print_string
    mov si, menu_opt1
    call print_string
    mov si, menu_opt2
    call print_string
    mov si, menu_prompt
    call print_string
    mov si, menu_bot
    call print_string

    ; 4. Wait for keypress: '1' = GUI, '2' = Safe Mode
.wait_key:
    xor ah, ah              ; AH=0x00: BIOS blocking keyboard read
    int 0x16                ; AL = ASCII keycode

    cmp al, '1'
    je .boot_gui
    cmp al, '2'
    je .boot_safe
    jmp .wait_key           ; Ignore anything else

    ; ========================================================================
    ; Boot Path 1: Standard GUI Mode (VESA 1024x768 Framebuffer)
    ; ========================================================================
.boot_gui:
    ; Print feedback
    mov si, msg_gui
    call print_string

    ; Query VESA VBE Mode Info for Mode 0x4118 (1024x768x32 LFB)
    ; Buffer placed at safe memory address 0x7000
    mov ax, 0x4F01          ; Get VBE Mode Info
    mov cx, 0x4118          ; Mode 0x118 + 0x4000 (Linear Frame Buffer)
    mov di, 0x7000          ; ES:DI = 0x0000:0x7000
    int 0x10
    cmp ax, 0x004F          ; VBE function supported & successful?
    jne vbe_error

    ; Set VBE Graphics Mode
    mov ax, 0x4F02          ; Set VBE Mode
    mov bx, 0x4118          ; Mode 0x4118
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    ; Build Mock Multiboot Info Struct at address 0x1000
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

    jmp .enter_pm

    ; ========================================================================
    ; Boot Path 2: Safe Mode (VGA 80x25 Text Console Only)
    ; ========================================================================
.boot_safe:
    ; Print feedback
    mov si, msg_safe
    call print_string

    ; Build minimal Multiboot Info Struct at address 0x1000
    ; NO framebuffer flag — kernel will detect text-only mode
    mov di, 0x1000
    xor ax, ax
    mov es, ax

    ; Clear the mbi structure (128 bytes)
    mov cx, 32
    xor eax, eax
    rep stosd

    ; mbi.flags: MULTIBOOT_FLAG_MEM only = 0x00000001
    mov dword [0x1000], 0x00000001
    ; mbi.mem_lower: 640 KB
    mov dword [0x1004], 640
    ; mbi.mem_upper: 63 MB
    mov dword [0x1008], 63 * 1024

    ; Fall through to Protected Mode entry

    ; ========================================================================
    ; Enter Protected Mode (common path for both boot modes)
    ; ========================================================================
.enter_pm:
    ; Disable interrupts and load temporary GDT
    cli
    lgdt [gdt_descriptor]

    ; Switch to Protected Mode (Set PE bit in CR0)
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to 32-bit Protected Mode (Selector 0x08 = Code)
    jmp 0x08:pm_entry

; ========================================================================
; VBE Error Handler
; ========================================================================
vbe_error:
    mov ax, 0x0003          ; Reset to standard 80x25 text mode
    int 0x10
    mov si, vbe_err_msg
    call print_string
.err_halt:
    cli
    hlt
    jmp .err_halt

; ========================================================================
; 16-bit BIOS String Print Routine (Teletype)
; ========================================================================
print_string:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    mov bx, 0x0007          ; Page 0, light grey on black
    int 0x10
    jmp print_string
.done:
    ret

; ========================================================================
; Boot Menu Text Data (compact to fit 1024 bytes)
; ========================================================================

menu_top    db 13, 10, 13, 10
            db "  ================================================", 13, 10, 0
menu_title  db "         M y O S   v 0 . 1", 13, 10, 0
menu_author db "      Engineered by Shreearu Bisoi", 13, 10, 0
menu_mid    db "  ================================================", 13, 10, 0
menu_opt1   db "    [1] GUI Mode    (1024x768 Desktop)", 13, 10, 0
menu_opt2   db "    [2] Safe Mode   (Text Console)", 13, 10, 0
menu_prompt db 13, 10, "    Press 1 or 2 to select...", 13, 10, 0
menu_bot    db "  ================================================", 13, 10, 0

msg_gui     db 13, 10, "  Loading VESA Graphics...", 13, 10, 0
msg_safe    db 13, 10, "  Booting Text Console...", 13, 10, 0
vbe_err_msg db "MyOS: VESA 1024x768 Not Supported!", 13, 10, 0

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

    ; 2. Copy Kernel Binary from RAM to 1MB (0x100000)
    ; Stage 2 is now 2 sectors (1024 bytes), so kernel starts at 0x8000 + 0x400 = 0x8400
    mov esi, 0x8400
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

; Pad Stage 2 Bootloader to exactly 1024 bytes (2 sectors)
times 1024-($-$$) db 0
