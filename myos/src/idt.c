/* ============================================================================
 * MyOS - Interrupt Descriptor Table, ISR & IRQ Handlers, PIC
 * Sets up IDT, remaps PIC, installs CPU exception and IRQ handlers
 * ============================================================================ */

#include "kernel.h"

/* ============================================================================
 * IDT
 * ============================================================================ */

#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_pointer;
static isr_handler_t isr_handlers[IDT_ENTRIES];

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel       = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

void isr_register_handler(uint8_t n, isr_handler_t handler) {
    isr_handlers[n] = handler;
}

/* ============================================================================
 * PIC (8259 Programmable Interrupt Controller)
 * ============================================================================ */

void pic_init(void) {
    /* Save masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* Start initialization sequence (ICW1) */
    outb(PIC1_CMD, 0x11);  io_wait();
    outb(PIC2_CMD, 0x11);  io_wait();

    /* ICW2: Remap IRQs - PIC1 to ISR 32-39, PIC2 to ISR 40-47 */
    outb(PIC1_DATA, 0x20); io_wait();
    outb(PIC2_DATA, 0x28); io_wait();

    /* ICW3: Tell PIC1 about PIC2 at IRQ2, tell PIC2 its cascade identity */
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    /* Restore saved masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_set_mask(uint8_t irq) {
    uint16_t port;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    outb(port, inb(port) | (1 << irq));
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    outb(port, inb(port) & ~(1 << irq));
}

/* ============================================================================
 * ISR & IRQ Handlers (C side)
 * ============================================================================ */

static const char* exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD FPU Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved"
};

/* Called from isr_common_stub in isr.asm */
void isr_handler(registers_t* regs) {
    if (isr_handlers[regs->int_no]) {
        isr_handlers[regs->int_no](regs);
    } else if (regs->int_no < 32) {
        /* Unhandled CPU exception */
        vga_setcolor(VGA_WHITE, VGA_RED);
        vga_puts("\n*** KERNEL PANIC: ");
        vga_puts(exception_messages[regs->int_no]);
        vga_puts(" ***\n");
        vga_puts("Error code: ");
        vga_put_hex(regs->err_code);
        vga_puts("  EIP: ");
        vga_put_hex(regs->eip);
        vga_puts("\n");
        cli();
        for (;;) hlt();
    }
}

/* Called from irq_common_stub in isr.asm */
void irq_handler(registers_t* regs) {
    /* Dispatch to registered handler if any */
    if (isr_handlers[regs->int_no]) {
        isr_handlers[regs->int_no](regs);
    }

    /* Send End-of-Interrupt */
    pic_send_eoi(regs->int_no - 32);
}

/* ============================================================================
 * IDT Initialization
 * ============================================================================ */

void idt_init(void) {
    idt_pointer.limit = sizeof(idt) - 1;
    idt_pointer.base  = (uint32_t)&idt;

    memset(&idt, 0, sizeof(idt));
    memset(&isr_handlers, 0, sizeof(isr_handlers));

    /* Remap PIC first */
    pic_init();

    /* Install CPU exception ISRs (0-31) */
    idt_set_gate(0,  (uint32_t)isr0,  0x08, 0x8E);
    idt_set_gate(1,  (uint32_t)isr1,  0x08, 0x8E);
    idt_set_gate(2,  (uint32_t)isr2,  0x08, 0x8E);
    idt_set_gate(3,  (uint32_t)isr3,  0x08, 0x8E);
    idt_set_gate(4,  (uint32_t)isr4,  0x08, 0x8E);
    idt_set_gate(5,  (uint32_t)isr5,  0x08, 0x8E);
    idt_set_gate(6,  (uint32_t)isr6,  0x08, 0x8E);
    idt_set_gate(7,  (uint32_t)isr7,  0x08, 0x8E);
    idt_set_gate(8,  (uint32_t)isr8,  0x08, 0x8E);
    idt_set_gate(9,  (uint32_t)isr9,  0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);

    /* Install hardware IRQ handlers (32-47) */
    idt_set_gate(32, (uint32_t)irq0,  0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1,  0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2,  0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3,  0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4,  0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5,  0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6,  0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7,  0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8,  0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9,  0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    /* Enable only needed IRQs */
    outb(PIC1_DATA, 0xFF);  /* Mask all PIC1 */
    outb(PIC2_DATA, 0xFF);  /* Mask all PIC2 */

    /* Unmask: timer (0), keyboard (1), cascade (2), mouse (12) */
    pic_clear_mask(0);   /* Timer */
    pic_clear_mask(1);   /* Keyboard */
    pic_clear_mask(2);   /* Cascade - required for PIC2 IRQs */
    pic_clear_mask(12);  /* Mouse */

    /* Load IDT */
    idt_load((uint32_t)&idt_pointer);
}
