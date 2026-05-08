; ============================================================================
; MyOS - Custom Stage 1 MBR Bootloader (512 bytes)
; Loaded by BIOS at physical address 0x7C00 in 16-bit Real Mode
; ============================================================================

[org 0x7c00]
[bits 16]

start:
    ; 1. Clear interrupts
    cli

    ; 2. Initialize segment registers to 0
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x7c00          ; Stack grows downwards from 0x7C00

    ; 3. Save boot drive number provided by BIOS in DL
    mov [boot_drive], dl

    ; 4. Enable A20 Line via System Control Port A (Port 0x92)
    in al, 0x92
    or al, 2
    out 0x92, al

    ; 5. Load Stage 2 + Kernel from disk sector-by-sector
    ; Destination in RAM starts at physical address 0x8000
    ; Segment ES = 0x0800, Offset BX = 0. Physical address = 0x0800 * 16 + 0 = 0x8000.
    mov ax, 0x0800
    mov es, ax
    xor bx, bx              ; Load each sector to ES:0 (BX remains 0)
    
    mov ch, 0               ; Cylinder 0
    mov cl, 2               ; Sector 2
    mov dh, 0               ; Head 0
    
    mov bp, 192             ; Read 192 sectors (96 KB - plenty for kernel + stage2)

read_loop:
    mov ax, 0x0201          ; AH = 0x02 (Read Sectors), AL = 1 (Read 1 sector)
    mov dl, [boot_drive]    ; Boot drive
    int 0x13
    jc disk_error           ; If carry flag set, disk error!
    
    ; Advance memory buffer by 512 bytes by incrementing Segment register ES
    ; Segment math: 0x0020 paragraphs * 16 = 512 bytes
    mov ax, es
    add ax, 0x20
    mov es, ax
    
    ; Increment CHS coordinates for sector-by-sector read
    inc cl                  ; Move to next sector
    cmp cl, 64              ; 63 sectors per track (standard IDE/VirtualBox geometry)
    jl .no_wrap
    
    mov cl, 1               ; Reset sector to 1
    inc dh                  ; Move to next head
    cmp dh, 16              ; 16 heads (standard IDE/VirtualBox geometry)
    jl .no_wrap
    
    mov dh, 0               ; Reset head to 0
    inc ch                  ; Move to next cylinder

.no_wrap:
    dec bp                  ; Decrement sectors remaining
    jnz read_loop           ; Keep reading if there are sectors left

    ; 6. Jump to Stage 2 loader at physical address 0x8000
    jmp 0x0000:0x8000

disk_error:
    mov si, error_msg
print_loop:
    lodsb
    or al, al
    jz halt
    mov ah, 0x0e
    int 0x10
    jmp print_loop

halt:
    cli
    hlt
    jmp halt

; Data
boot_drive  db 0
error_msg   db "MyOS: Disk Read Error!", 13, 10, 0

; Pad to 510 bytes and write boot signature
times 510-($-$$) db 0
dw 0xAA55
