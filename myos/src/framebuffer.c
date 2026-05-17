/* ============================================================================
 * MyOS - Framebuffer Graphics Driver
 * VESA/Multiboot framebuffer with drawing primitives and double buffering
 * ============================================================================ */

#include "kernel.h"

static framebuffer_t fb;
static uint8_t* back_buffer = NULL;

/* Simple kernel heap pointer for back buffer allocation */
static uint32_t heap_ptr = 0;

static uint32_t* simple_alloc(size_t bytes) {
    if (heap_ptr == 0) {
        /* Start heap after kernel end, aligned to 4KB */
        heap_ptr = ((uint32_t)&_kernel_end + 0xFFF) & ~0xFFF;
        heap_ptr += 0x100000;  /* Extra 1MB margin */
    }
    uint32_t* ptr = (uint32_t*)heap_ptr;
    heap_ptr += bytes;
    heap_ptr = (heap_ptr + 3) & ~3;  /* Align to 4 bytes */
    return ptr;
}

void fb_init(multiboot_info_t* mbi) {
    fb.available = false;

    if (!(mbi->flags & MULTIBOOT_FLAG_FB))
        return;

    if (mbi->framebuffer_type != 1)  /* 1 = RGB direct color */
        return;

    fb.buffer = (uint32_t*)(uint32_t)mbi->framebuffer_addr;
    fb.width  = mbi->framebuffer_width;
    fb.height = mbi->framebuffer_height;
    fb.pitch  = mbi->framebuffer_pitch;
    fb.bpp    = mbi->framebuffer_bpp;
    fb.available = true;

    /* Allocate back buffer for double buffering */
    size_t fb_size = fb.pitch * fb.height;
    back_buffer = (uint8_t*)simple_alloc(fb_size);
    memset(back_buffer, 0, fb_size);
}

framebuffer_t* fb_get(void) {
    return &fb;
}

void fb_putpixel(int x, int y, uint32_t color) {
    if (!fb.available) return;
    if (x < 0 || x >= (int)fb.width || y < 0 || y >= (int)fb.height) return;

    if (fb.bpp == 32) {
        uint32_t offset = y * (fb.pitch / 4) + x;
        if (back_buffer)
            ((uint32_t*)back_buffer)[offset] = color;
        else
            fb.buffer[offset] = color;
    } else if (fb.bpp == 24) {
        uint32_t offset = y * fb.pitch + x * 3;
        uint8_t* dest = back_buffer ? (back_buffer + offset) : ((uint8_t*)fb.buffer + offset);
        dest[0] = color & 0xFF;        /* Blue */
        dest[1] = (color >> 8) & 0xFF; /* Green */
        dest[2] = (color >> 16) & 0xFF;/* Red */
    } else if (fb.bpp == 16) {
        uint32_t offset = y * fb.pitch + x * 2;
        uint16_t* dest = back_buffer ? (uint16_t*)(back_buffer + offset) : (uint16_t*)((uint8_t*)fb.buffer + offset);
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        uint16_t packed = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        *dest = packed;
    }
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!fb.available) return;

    /* Clipping */
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > (int)fb.width  ? (int)fb.width  : (x + w);
    int y1 = (y + h) > (int)fb.height ? (int)fb.height : (y + h);
    if (x0 >= x1 || y0 >= y1) return;

    if (fb.bpp == 32) {
        uint32_t* buf = back_buffer ? (uint32_t*)back_buffer : fb.buffer;
        uint32_t stride = fb.pitch / 4;
        for (int row = y0; row < y1; row++) {
            uint32_t* line = buf + row * stride + x0;
            for (int col = 0; col < (x1 - x0); col++) {
                line[col] = color;
            }
        }
    } else if (fb.bpp == 24) {
        uint8_t* buf = back_buffer ? back_buffer : (uint8_t*)fb.buffer;
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        for (int row = y0; row < y1; row++) {
            uint8_t* line = buf + row * fb.pitch + x0 * 3;
            for (int col = 0; col < (x1 - x0); col++) {
                line[col * 3]     = b;
                line[col * 3 + 1] = g;
                line[col * 3 + 2] = r;
            }
        }
    } else if (fb.bpp == 16) {
        uint8_t* buf = back_buffer ? back_buffer : (uint8_t*)fb.buffer;
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        uint16_t packed = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        for (int row = y0; row < y1; row++) {
            uint16_t* line = (uint16_t*)(buf + row * fb.pitch) + x0;
            for (int col = 0; col < (x1 - x0); col++) {
                line[col] = packed;
            }
        }
    }
}

void fb_draw_rect(int x, int y, int w, int h, uint32_t color) {
    fb_fill_rect(x, y, w, 1, color);          /* Top */
    fb_fill_rect(x, y + h - 1, w, 1, color);  /* Bottom */
    fb_fill_rect(x, y, 1, h, color);          /* Left */
    fb_fill_rect(x + w - 1, y, 1, h, color);  /* Right */
}

void fb_draw_gradient_rect(int x, int y, int w, int h,
                           uint32_t c_top, uint32_t c_bot) {
    if (!fb.available || h <= 0) return;

    uint8_t r1 = (c_top >> 16) & 0xFF, g1 = (c_top >> 8) & 0xFF, b1 = c_top & 0xFF;
    uint8_t r2 = (c_bot >> 16) & 0xFF, g2 = (c_bot >> 8) & 0xFF, b2 = c_bot & 0xFF;

    for (int row = 0; row < h; row++) {
        uint8_t r = r1 + (int)(r2 - r1) * row / h;
        uint8_t g = g1 + (int)(g2 - g1) * row / h;
        uint8_t b = b1 + (int)(b2 - b1) * row / h;
        fb_fill_rect(x, y + row, w, 1, RGB(r, g, b));
    }
}

void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if (!fb.available) return;

    const uint8_t* glyph = font_get_glyph(c);

    for (int row = 0; row < FONT_HEIGHT; row++) {
        int py = y + row;
        if (py < 0 || py >= (int)fb.height) continue;

        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            int px = x + col;
            if (px < 0 || px >= (int)fb.width) continue;

            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            fb_putpixel(px, py, color);
        }
    }
}

void fb_draw_string(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            cx = x;
            y += FONT_HEIGHT;
        } else {
            fb_draw_char(cx, y, *str, fg, bg);
            cx += FONT_WIDTH;
        }
        str++;
    }
}

void fb_swap(void) {
    if (!fb.available || !back_buffer) return;
    memcpy(fb.buffer, back_buffer, fb.pitch * fb.height);
}

void fb_blit(uint32_t* src, int sx, int sy, int sw, int sh,
             int dx, int dy) {
    if (!fb.available || !src) return;

    if (fb.bpp == 32) {
        uint32_t* buf = back_buffer ? (uint32_t*)back_buffer : fb.buffer;
        uint32_t stride = fb.pitch / 4;
        for (int y = 0; y < sh; y++) {
            int dest_y = dy + y;
            if (dest_y < 0 || dest_y >= (int)fb.height) continue;
            int src_y = sy + y;
            for (int x = 0; x < sw; x++) {
                int dest_x = dx + x;
                if (dest_x < 0 || dest_x >= (int)fb.width) continue;
                int src_x = sx + x;
                buf[dest_y * stride + dest_x] = src[src_y * sw + src_x];
            }
        }
    } else if (fb.bpp == 24) {
        uint8_t* buf = back_buffer ? back_buffer : (uint8_t*)fb.buffer;
        for (int y = 0; y < sh; y++) {
            int dest_y = dy + y;
            if (dest_y < 0 || dest_y >= (int)fb.height) continue;
            int src_y = sy + y;
            for (int x = 0; x < sw; x++) {
                int dest_x = dx + x;
                if (dest_x < 0 || dest_x >= (int)fb.width) continue;
                int src_x = sx + x;
                uint32_t color = src[src_y * sw + src_x];
                uint32_t offset = dest_y * fb.pitch + dest_x * 3;
                buf[offset]     = color & 0xFF;        /* Blue */
                buf[offset + 1] = (color >> 8) & 0xFF; /* Green */
                buf[offset + 2] = (color >> 16) & 0xFF;/* Red */
            }
        }
    } else if (fb.bpp == 16) {
        uint8_t* buf = back_buffer ? back_buffer : (uint8_t*)fb.buffer;
        for (int y = 0; y < sh; y++) {
            int dest_y = dy + y;
            if (dest_y < 0 || dest_y >= (int)fb.height) continue;
            int src_y = sy + y;
            for (int x = 0; x < sw; x++) {
                int dest_x = dx + x;
                if (dest_x < 0 || dest_x >= (int)fb.width) continue;
                int src_x = sx + x;
                uint32_t color = src[src_y * sw + src_x];
                uint8_t r = (color >> 16) & 0xFF;
                uint8_t g = (color >> 8) & 0xFF;
                uint8_t b = color & 0xFF;
                uint16_t packed = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                uint16_t* line = (uint16_t*)(buf + dest_y * fb.pitch);
                line[dest_x] = packed;
            }
        }
    }
}

void fb_scroll_region(int x, int y, int w, int h, int lines, uint32_t bg_color) {
    if (!fb.available || !back_buffer) return;
    int pixel_lines = lines * FONT_HEIGHT;
    int bpp_bytes = fb.bpp / 8;

    if (pixel_lines >= h) {
        /* Clear entire region */
        fb_fill_rect(x, y, w, h, bg_color);
        return;
    }

    /* Scroll up */
    for (int row = y; row < y + h - pixel_lines; row++) {
        uint8_t* dest = back_buffer + row * fb.pitch + x * bpp_bytes;
        uint8_t* src = back_buffer + (row + pixel_lines) * fb.pitch + x * bpp_bytes;
        memcpy(dest, src, w * bpp_bytes);
    }

    /* Clear newly exposed area */
    fb_fill_rect(x, y + h - pixel_lines, w, pixel_lines, bg_color);
}
