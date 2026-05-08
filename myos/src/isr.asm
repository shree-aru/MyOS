; ============================================================================
; MyOS - Interrupt Service Routine & IRQ Stubs
; Assembly stubs that save state, call C handler, restore state
; ============================================================================

; External C handler
extern isr_handler
extern irq_handler

; ============================================================================
; Macro: ISR with no error code (push dummy 0)
; ============================================================================
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0           ; Dummy error code
    push dword %1          ; Interrupt number
    jmp isr_common_stub
%endmacro

; ============================================================================
; Macro: ISR with error code (CPU pushes it automatically)
; ============================================================================
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1          ; Interrupt number (error code already pushed)
    jmp isr_common_stub
%endmacro

; ============================================================================
; Macro: IRQ stub
; ============================================================================
%macro IRQ 2
global irq%1
irq%1:
    push dword 0           ; Dummy error code
    push dword %2          ; ISR number (32 + IRQ number)
    jmp irq_common_stub
%endmacro

; ============================================================================
; CPU Exception ISRs (0-31)
; ============================================================================
ISR_NOERRCODE 0        ; Division by zero
ISR_NOERRCODE 1        ; Debug
ISR_NOERRCODE 2        ; Non-maskable interrupt
ISR_NOERRCODE 3        ; Breakpoint
ISR_NOERRCODE 4        ; Overflow
ISR_NOERRCODE 5        ; Bound range exceeded
ISR_NOERRCODE 6        ; Invalid opcode
ISR_NOERRCODE 7        ; Device not available
ISR_ERRCODE   8        ; Double fault
ISR_NOERRCODE 9        ; Coprocessor segment overrun
ISR_ERRCODE   10       ; Invalid TSS
ISR_ERRCODE   11       ; Segment not present
ISR_ERRCODE   12       ; Stack-segment fault
ISR_ERRCODE   13       ; General protection fault
ISR_ERRCODE   14       ; Page fault
ISR_NOERRCODE 15       ; Reserved
ISR_NOERRCODE 16       ; x87 FPU error
ISR_ERRCODE   17       ; Alignment check
ISR_NOERRCODE 18       ; Machine check
ISR_NOERRCODE 19       ; SIMD FPU exception
ISR_NOERRCODE 20       ; Virtualization exception
ISR_ERRCODE   21       ; Control protection exception
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; ============================================================================
; Hardware IRQs (0-15, mapped to ISR 32-47)
; ============================================================================
IRQ  0, 32             ; PIT Timer
IRQ  1, 33             ; Keyboard
IRQ  2, 34             ; Cascade (PIC2)
IRQ  3, 35             ; COM2
IRQ  4, 36             ; COM1
IRQ  5, 37             ; LPT2
IRQ  6, 38             ; Floppy
IRQ  7, 39             ; LPT1 / Spurious
IRQ  8, 40             ; CMOS RTC
IRQ  9, 41             ; Free
IRQ 10, 42             ; Free
IRQ 11, 43             ; Free
IRQ 12, 44             ; PS/2 Mouse
IRQ 13, 45             ; FPU
IRQ 14, 46             ; Primary ATA
IRQ 15, 47             ; Secondary ATA

; ============================================================================
; Common ISR Stub - Saves state, calls C handler, restores state
; ============================================================================
isr_common_stub:
    pusha                  ; Push EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI

    ; Save data segment
    mov ax, ds
    push eax

    ; Load kernel data segment (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Push pointer to registers struct
    push esp
    call isr_handler
    add esp, 4             ; Clean up pushed argument

    ; Restore data segment
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa                   ; Restore registers
    add esp, 8             ; Remove error code and ISR number
    iret                   ; Return from interrupt

; ============================================================================
; Common IRQ Stub - Same as ISR but calls irq_handler
; ============================================================================
irq_common_stub:
    pusha

    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_handler
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iret
