/* ============================================================================
 * MyOS - VGA Text Mode Driver
 * 80x25 text console with color, scrolling, and cursor support
 * ============================================================================ */

#include "kernel.h"

#define VGA_MEMORY 0xB8000
#define VGA_CRTC_ADDR  0x3D4
#define VGA_CRTC_DATA  0x3D5

static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;
static int vga_col = 0;
static int vga_row = 0;
static uint8_t vga_color_attr = 0x0F;  /* White on black */

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void vga_update_cursor(void) {
    uint16_t pos = vga_row * VGA_WIDTH + vga_col;
    outb(VGA_CRTC_ADDR, 14);
    outb(VGA_CRTC_DATA, (pos >> 8) & 0xFF);
    outb(VGA_CRTC_ADDR, 15);
    outb(VGA_CRTC_DATA, pos & 0xFF);
}

static void vga_scroll(void) {
    if (vga_row >= VGA_HEIGHT) {
        /* Move everything up one line */
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        /* Clear last line */
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga_buffer[i] = vga_entry(' ', vga_color_attr);
        }
        vga_row = VGA_HEIGHT - 1;
    }
}

void vga_init(void) {
    vga_col = 0;
    vga_row = 0;
    vga_color_attr = 0x0F;
    vga_clear();
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = vga_entry(' ', vga_color_attr);
    }
    vga_col = 0;
    vga_row = 0;
    vga_update_cursor();
}

void vga_setcolor(uint8_t fg, uint8_t bg) {
    vga_color_attr = (bg << 4) | (fg & 0x0F);
}

void vga_putchar(char c) {
    if (use_fb) {
        con_putchar(c);
        return;
    }
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\t') {
        vga_col = (vga_col + 8) & ~7;
        if (vga_col >= VGA_WIDTH) {
            vga_col = 0;
            vga_row++;
        }
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_color_attr);
        }
    } else {
        vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(c, vga_color_attr);
        vga_col++;
        if (vga_col >= VGA_WIDTH) {
            vga_col = 0;
            vga_row++;
        }
    }
    vga_scroll();
    vga_update_cursor();
}

void vga_puts(const char* str) {
    while (*str)
        vga_putchar(*str++);
}

void vga_put_hex(uint32_t val) {
    char buf[12];
    vga_puts("0x");
    utoa(val, buf, 16);
    vga_puts(buf);
}

void vga_put_dec(uint32_t val) {
    char buf[12];
    utoa(val, buf, 10);
    vga_puts(buf);
}
