/* ============================================================================
 * MyOS - Kernel Header
 * Master header with all types, macros, structures, and function declarations
 * ============================================================================ */

#ifndef KERNEL_H
#define KERNEL_H

/* ============================================================================
 * Standard Types
 * ============================================================================ */

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;
typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;
typedef uint32_t            size_t;
typedef int32_t             ssize_t;
typedef int32_t             bool;

#define true  1
#define false 0
#define NULL  ((void*)0)

/* ============================================================================
 * Multiboot Structures
 * ============================================================================ */

#define MULTIBOOT_MAGIC         0x2BADB002
#define MULTIBOOT_FLAG_MEM      (1 << 0)
#define MULTIBOOT_FLAG_MMAP     (1 << 6)
#define MULTIBOOT_FLAG_VBE      (1 << 11)
#define MULTIBOOT_FLAG_FB       (1 << 12)

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
} __attribute__((packed)) multiboot_info_t;

typedef struct {
    uint32_t size;
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
} __attribute__((packed)) mmap_entry_t;

/* ============================================================================
 * I/O Port Functions (inline assembly)
 * ============================================================================ */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0); /* Port 0x80 is used for POST codes; safe to write */
}

static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }

/* ============================================================================
 * GDT (Global Descriptor Table)
 * ============================================================================ */

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

void gdt_init(void);
extern void gdt_flush(uint32_t gdt_ptr);

/* ============================================================================
 * IDT (Interrupt Descriptor Table) & ISR
 * ============================================================================ */

typedef struct {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

/* Registers pushed by ISR stubs */
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, useless_esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, esp, ss;
} __attribute__((packed)) registers_t;

typedef void (*isr_handler_t)(registers_t*);

void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void isr_register_handler(uint8_t n, isr_handler_t handler);
extern void idt_load(uint32_t idt_ptr);

/* ISR declarations (defined in isr.asm) */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

/* IRQ declarations (mapped to ISR 32-47) */
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

#define IRQ0  32
#define IRQ1  33
#define IRQ2  34
#define IRQ3  35
#define IRQ4  36
#define IRQ5  37
#define IRQ6  38
#define IRQ7  39
#define IRQ8  40
#define IRQ9  41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47

/* ============================================================================
 * PIC (Programmable Interrupt Controller)
 * ============================================================================ */

#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1
#define PIC_EOI     0x20

void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);

/* ============================================================================
 * Timer (PIT - Programmable Interval Timer)
 * ============================================================================ */

#define PIT_FREQ    1193180
#define TIMER_HZ    100

void timer_init(void);
uint32_t timer_get_ticks(void);
uint32_t timer_get_seconds(void);

/* ============================================================================
 * PMM (Physical Memory Manager)
 * ============================================================================ */

#define PAGE_SIZE   4096

void pmm_init(multiboot_info_t* mbi);
uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t addr);
uint32_t pmm_get_total_memory(void);
uint32_t pmm_get_used_frames(void);
uint32_t pmm_get_free_frames(void);

/* ============================================================================
 * VGA Text Mode
 * ============================================================================ */

#define VGA_WIDTH   80
#define VGA_HEIGHT  25

/* VGA Colors */
enum vga_color {
    VGA_BLACK = 0, VGA_BLUE = 1, VGA_GREEN = 2, VGA_CYAN = 3,
    VGA_RED = 4, VGA_MAGENTA = 5, VGA_BROWN = 6, VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8, VGA_LIGHT_BLUE = 9, VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11, VGA_LIGHT_RED = 12, VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14, VGA_WHITE = 15
};

void vga_init(void);
void vga_clear(void);
void vga_setcolor(uint8_t fg, uint8_t bg);
void vga_putchar(char c);
void vga_puts(const char* str);
void vga_put_hex(uint32_t val);
void vga_put_dec(uint32_t val);

/* Console / Shell Output Redirection */
extern bool use_fb;
void con_putchar(char c);
void con_puts(const char* str);
void con_flush(void);

/* ============================================================================
 * Keyboard (PS/2)
 * ============================================================================ */

#define KEY_BUFFER_SIZE 256

void keyboard_init(void);
char keyboard_getchar(void);       /* Blocking read */
bool keyboard_has_key(void);       /* Non-blocking check */
char keyboard_poll(void);          /* Non-blocking read, 0 if none */

/* ============================================================================
 * Mouse (PS/2)
 * ============================================================================ */

typedef struct {
    int32_t  x, y;
    bool     left, right, middle;
    int32_t  dx, dy;               /* Delta since last read */
} mouse_state_t;

void mouse_init(void);
void mouse_get_state(mouse_state_t* state);
void mouse_set_bounds(int32_t max_x, int32_t max_y);

/* ============================================================================
 * Framebuffer Graphics
 * ============================================================================ */

typedef struct {
    uint32_t* buffer;       /* Framebuffer address */
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;        /* Bytes per scanline */
    uint8_t   bpp;          /* Bits per pixel */
    bool      available;
} framebuffer_t;

/* RGBA color helpers */
#define RGB(r, g, b) ((uint32_t)((r) << 16 | (g) << 8 | (b)))
#define RGBA(r, g, b, a) ((uint32_t)((a) << 24 | (r) << 16 | (g) << 8 | (b)))

/* Common colors */
#define COLOR_BLACK       RGB(0, 0, 0)
#define COLOR_WHITE       RGB(255, 255, 255)
#define COLOR_RED         RGB(220, 50, 50)
#define COLOR_GREEN       RGB(50, 200, 50)
#define COLOR_BLUE        RGB(50, 100, 220)
#define COLOR_DARK_GREY   RGB(40, 40, 45)
#define COLOR_MID_GREY    RGB(80, 80, 88)
#define COLOR_LIGHT_GREY  RGB(180, 180, 190)
#define COLOR_TITLE_BG    RGB(55, 65, 110)
#define COLOR_TITLE_ACTIVE RGB(75, 95, 160)
#define COLOR_TASKBAR     RGB(30, 32, 40)
#define COLOR_ACCENT      RGB(100, 140, 255)
#define COLOR_CLOSE_BTN   RGB(220, 60, 60)
#define COLOR_DESKTOP_TOP RGB(25, 25, 55)
#define COLOR_DESKTOP_BOT RGB(15, 55, 85)
#define COLOR_WINDOW_BG   RGB(35, 38, 48)
#define COLOR_BORDER      RGB(60, 65, 80)
#define COLOR_TEXT         RGB(220, 225, 235)
#define COLOR_TEXT_DIM     RGB(140, 145, 160)
#define COLOR_HIGHLIGHT   RGB(120, 160, 255)
#define COLOR_YELLOW      RGB(255, 200, 60)
#define COLOR_ORANGE      RGB(255, 150, 50)

void fb_init(multiboot_info_t* mbi);
framebuffer_t* fb_get(void);
void fb_putpixel(int x, int y, uint32_t color);
void fb_fill_rect(int x, int y, int w, int h, uint32_t color);
void fb_draw_rect(int x, int y, int w, int h, uint32_t color);
void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void fb_draw_string(int x, int y, const char* str, uint32_t fg, uint32_t bg);
void fb_draw_gradient_rect(int x, int y, int w, int h,
                           uint32_t c_top, uint32_t c_bot);
void fb_blit(uint32_t* src, int sx, int sy, int sw, int sh,
             int dx, int dy);
void fb_swap(void);                /* Swap back buffer to front */
void fb_scroll_region(int x, int y, int w, int h, int lines);

/* ============================================================================
 * Font (8x16 Bitmap)
 * ============================================================================ */

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

const uint8_t* font_get_glyph(char c);

/* ============================================================================
 * Shell (Text Console)
 * ============================================================================ */

void shell_init(void);
void shell_run(void);              /* Main shell loop (blocking) */

/* ============================================================================
 * GUI (Compositor, Window Manager, Desktop)
 * ============================================================================ */

#define MAX_WINDOWS 16
#define TITLEBAR_HEIGHT 28
#define TASKBAR_HEIGHT 36
#define CLOSE_BTN_SIZE 16

typedef struct window {
    int32_t  x, y, w, h;
    char     title[64];
    uint32_t* buffer;              /* Client area pixel buffer */
    bool     visible;
    bool     focused;
    bool     dirty;
    bool     closable;
    int32_t  id;
    /* Content callback: draws into the window's client area */
    void (*draw_content)(struct window* win);
    void (*on_key)(struct window* win, char key);
    void (*on_mouse)(struct window* win, int32_t x, int32_t y, bool left_button);
} window_t;

void gui_init(void);
void gui_run(void);                /* Main GUI loop (blocking) */
window_t* gui_create_window(const char* title, int x, int y, int w, int h);
void gui_destroy_window(window_t* win);
void gui_set_draw_callback(window_t* win, void (*cb)(window_t*));
void gui_set_key_callback(window_t* win, void (*cb)(window_t*, char));
void gui_set_mouse_callback(window_t* win, void (*cb)(window_t*, int32_t, int32_t, bool));

/* ============================================================================
 * String Utilities
 * ============================================================================ */

void*  memset(void* dst, int val, size_t n);
void*  memcpy(void* dst, const void* src, size_t n);
void*  memmove(void* dst, const void* src, size_t n);
int    memcmp(const void* s1, const void* s2, size_t n);
size_t strlen(const char* s);
int    strcmp(const char* s1, const char* s2);
int    strncmp(const char* s1, const char* s2, size_t n);
char*  strcpy(char* dst, const char* src);
char*  strncpy(char* dst, const char* src, size_t n);
char*  strcat(char* dst, const char* src);
char*  itoa(int val, char* buf, int base);
char*  utoa(uint32_t val, char* buf, int base);

/* ============================================================================
 * Kernel Panic
 * ============================================================================ */

void kpanic(const char* msg);

/* ============================================================================
 * System Power Control
 * ============================================================================ */

void sys_shutdown(void);
void sys_reboot(void);

/* External symbols from linker script */
extern uint32_t _kernel_start;
extern uint32_t _kernel_end;
extern uint32_t _bss_start;
extern uint32_t _bss_end;

/* ============================================================================
 * Custom ATA hard disk driver API
 * ============================================================================ */
void ata_init(void);
void ata_read_sector(uint32_t lba, uint8_t* buffer);
void ata_write_sector(uint32_t lba, const uint8_t* buffer);

/* ============================================================================
 * Custom MyFS file system API
 * ============================================================================ */
void myfs_init(void);
void myfs_format(void);
void myfs_list(void);
bool myfs_create(const char* name);
bool myfs_write(const char* name, const uint8_t* data, uint32_t size);
int32_t myfs_read(const char* name, uint8_t* buffer, uint32_t max_size);
bool myfs_delete(const char* name);

/* ============================================================================
 * Custom Forth compiler API
 * ============================================================================ */
void forth_init(void);
void forth_run_command(const char* line);
void forth_shell(void);

#endif /* KERNEL_H */
