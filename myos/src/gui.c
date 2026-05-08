/* ============================================================================
 * MyOS - GUI: Compositor, Window Manager & Desktop Environment
 * Wayland-inspired compositor with window management, taskbar, and demo apps
 * ============================================================================ */

#include "kernel.h"

/* ============================================================================
 * Window Management
 * ============================================================================ */

static window_t windows[MAX_WINDOWS];
static int window_count = 0;
static int next_window_id = 1;
static int focused_index = -1;

/* Dragging state */
static bool dragging = false;
static int drag_win_idx = -1;
static int drag_offset_x = 0;
static int drag_offset_y = 0;

/* Previous mouse position (for cursor redraw) */
static int prev_mouse_x = -1;
static int prev_mouse_y = -1;

/* GUI running flag */
static bool gui_running = false;

/* Forward declarations for Exit/Shutdown Dialog */
static void draw_exit_content(window_t* win);
static void exit_on_mouse(window_t* win, int32_t x, int32_t y, bool left_button);

window_t* gui_create_window(const char* title, int x, int y, int w, int h) {
    if (window_count >= MAX_WINDOWS) return NULL;

    window_t* win = &windows[window_count];
    memset(win, 0, sizeof(window_t));

    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->id = next_window_id++;
    win->visible = true;
    win->focused = false;
    win->dirty = true;
    win->closable = true;
    win->draw_content = NULL;
    win->on_key = NULL;
    strncpy(win->title, title, 63);
    win->title[63] = '\0';

    window_count++;

    /* Focus the new window */
    focused_index = window_count - 1;
    for (int i = 0; i < window_count; i++)
        windows[i].focused = (i == focused_index);

    return win;
}

void gui_destroy_window(window_t* win) {
    if (!win) return;
    int idx = -1;
    for (int i = 0; i < window_count; i++) {
        if (&windows[i] == win) { idx = i; break; }
    }
    if (idx < 0) return;

    /* Shift remaining windows */
    for (int i = idx; i < window_count - 1; i++)
        windows[i] = windows[i + 1];
    window_count--;

    if (focused_index >= window_count)
        focused_index = window_count - 1;
    if (focused_index >= 0)
        windows[focused_index].focused = true;
}

void gui_set_draw_callback(window_t* win, void (*cb)(window_t*)) {
    if (win) win->draw_content = cb;
}

void gui_set_key_callback(window_t* win, void (*cb)(window_t*, char)) {
    if (win) win->on_key = cb;
}

void gui_set_mouse_callback(window_t* win, void (*cb)(window_t*, int32_t, int32_t, bool)) {
    if (win) win->on_mouse = cb;
}

static void raise_window(int idx) {
    if (idx < 0 || idx >= window_count || idx == window_count - 1) return;

    window_t tmp = windows[idx];
    for (int i = idx; i < window_count - 1; i++)
        windows[i] = windows[i + 1];
    windows[window_count - 1] = tmp;

    focused_index = window_count - 1;
    for (int i = 0; i < window_count; i++)
        windows[i].focused = (i == focused_index);
}

/* ============================================================================
 * Drawing Helpers
 * ============================================================================ */

static uint32_t blend_color(uint32_t c1, uint32_t c2, int t, int max) {
    if (max <= 0) return c1;
    uint8_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    uint8_t r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    uint8_t r = r1 + (int)(r2 - r1) * t / max;
    uint8_t g = g1 + (int)(g2 - g1) * t / max;
    uint8_t b = b1 + (int)(b2 - b1) * t / max;
    return RGB(r, g, b);
}

/* Draw a rounded-corner rectangle (simplified: just fills with slight border) */
static void draw_rounded_rect(int x, int y, int w, int h,
                              uint32_t fill, uint32_t border) {
    /* Border */
    fb_fill_rect(x, y, w, h, fill);
    /* Top and bottom borders */
    fb_fill_rect(x + 1, y, w - 2, 1, border);
    fb_fill_rect(x + 1, y + h - 1, w - 2, 1, border);
    /* Left and right borders */
    fb_fill_rect(x, y + 1, 1, h - 2, border);
    fb_fill_rect(x + w - 1, y + 1, 1, h - 2, border);
}

/* ============================================================================
 * Desktop Background
 * ============================================================================ */

static void draw_desktop_background(void) {
    framebuffer_t* fb = fb_get();
    int h = fb->height - TASKBAR_HEIGHT;

    /* Beautiful gradient background */
    fb_draw_gradient_rect(0, 0, fb->width, h,
                         COLOR_DESKTOP_TOP, COLOR_DESKTOP_BOT);

    /* Draw subtle decorative circles/dots */
    for (int i = 0; i < 5; i++) {
        int cx = 100 + i * 220;
        int cy = 120 + (i % 3) * 150;
        int radius = 30 + i * 10;
        /* Simple filled circle approximation */
        for (int dy = -radius; dy <= radius; dy++) {
            int py = cy + dy;
            if (py >= 0 && py < h) {
                // Calculate background gradient color once per scanline
                uint32_t bg = blend_color(COLOR_DESKTOP_TOP, COLOR_DESKTOP_BOT, py, h);
                for (int dx = -radius; dx <= radius; dx++) {
                    if (dx*dx + dy*dy <= radius*radius) {
                        int px = cx + dx;
                        if (px >= 0 && px < (int)fb->width) {
                            /* Semi-transparent effect */
                            int dist = dx*dx + dy*dy;
                            int alpha = 20 - (dist * 20 / (radius * radius));
                            if (alpha > 0) {
                                uint32_t acc = COLOR_ACCENT;
                                uint32_t c = blend_color(bg, acc, alpha, 30);
                                fb_putpixel(px, py, c);
                            }
                        }
                    }
                }
            }
        }
    }

    /* OS name in center-ish area */
    const char* name = "MyOS";
    int name_len = strlen(name);
    int nx = (fb->width - name_len * FONT_WIDTH * 3) / 2;
    int ny = h / 2 - 40;

    /* Draw large text (3x scale) */
    for (int i = 0; name[i]; i++) {
        const uint8_t* glyph = font_get_glyph(name[i]);
        for (int row = 0; row < FONT_HEIGHT; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < FONT_WIDTH; col++) {
                if (bits & (0x80 >> col)) {
                    /* 3x scale */
                    for (int sy = 0; sy < 3; sy++)
                        for (int sx = 0; sx < 3; sx++)
                            fb_putpixel(nx + i * FONT_WIDTH * 3 + col * 3 + sx,
                                       ny + row * 3 + sy, COLOR_WHITE);
                }
            }
        }
    }

    /* Subtitle */
    const char* subtitle = "Low Resource Operating System  |  Type in terminal to interact";
    int sx = (fb->width - (int)strlen(subtitle) * FONT_WIDTH) / 2;
    fb_draw_string(sx, ny + FONT_HEIGHT * 3 + 10, subtitle,
                  COLOR_TEXT_DIM, COLOR_DESKTOP_BOT);
}

/* ============================================================================
 * Window Drawing
 * ============================================================================ */

static void draw_window(window_t* win) {
    int x = win->x, y = win->y, w = win->w, h = win->h;
    bool focused = win->focused;

    /* Window shadow */
    fb_fill_rect(x + 3, y + 3, w, h, RGB(10, 10, 15));

    /* Window background */
    fb_fill_rect(x, y, w, h, COLOR_WINDOW_BG);

    /* Window border */
    uint32_t border_color = focused ? COLOR_ACCENT : COLOR_BORDER;
    fb_draw_rect(x, y, w, h, border_color);
    if (focused) {
        fb_draw_rect(x + 1, y + 1, w - 2, h - 2,
                    blend_color(border_color, COLOR_WINDOW_BG, 1, 2));
    }

    /* Title bar */
    uint32_t title_bg = focused ? COLOR_TITLE_ACTIVE : COLOR_TITLE_BG;
    fb_fill_rect(x + 1, y + 1, w - 2, TITLEBAR_HEIGHT - 1, title_bg);

    /* Title bar gradient shine */
    for (int i = 0; i < 3; i++) {
        uint32_t shine = blend_color(title_bg, COLOR_WHITE, 1, 8 + i * 3);
        fb_fill_rect(x + 1, y + 1 + i, w - 2, 1, shine);
    }

    /* Title text */
    int title_x = x + 10;
    int title_y = y + (TITLEBAR_HEIGHT - FONT_HEIGHT) / 2;
    fb_draw_string(title_x, title_y, win->title,
                  focused ? COLOR_WHITE : COLOR_TEXT_DIM, title_bg);

    /* Close button */
    if (win->closable) {
        int bx = x + w - CLOSE_BTN_SIZE - 6;
        int by = y + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2;
        fb_fill_rect(bx, by, CLOSE_BTN_SIZE, CLOSE_BTN_SIZE, COLOR_CLOSE_BTN);
        /* X mark */
        for (int i = 3; i < CLOSE_BTN_SIZE - 3; i++) {
            fb_putpixel(bx + i, by + i, COLOR_WHITE);
            fb_putpixel(bx + i, by + CLOSE_BTN_SIZE - 1 - i, COLOR_WHITE);
            fb_putpixel(bx + i + 1, by + i, COLOR_WHITE);
            fb_putpixel(bx + i + 1, by + CLOSE_BTN_SIZE - 1 - i, COLOR_WHITE);
        }
    }

    /* Separator line below title bar */
    fb_fill_rect(x + 1, y + TITLEBAR_HEIGHT, w - 2, 1, COLOR_BORDER);

    /* Client area - call draw callback */
    if (win->draw_content) {
        win->draw_content(win);
    }
}

/* ============================================================================
 * Taskbar
 * ============================================================================ */

static void draw_taskbar(void) {
    framebuffer_t* fb = fb_get();
    int ty = fb->height - TASKBAR_HEIGHT;

    /* Taskbar background */
    fb_fill_rect(0, ty, fb->width, TASKBAR_HEIGHT, COLOR_TASKBAR);
    /* Top border */
    fb_fill_rect(0, ty, fb->width, 1, COLOR_BORDER);

    /* "Start" button area */
    fb_fill_rect(4, ty + 4, 60, TASKBAR_HEIGHT - 8, COLOR_ACCENT);
    fb_draw_string(12, ty + (TASKBAR_HEIGHT - FONT_HEIGHT) / 2,
                  "MyOS", COLOR_WHITE, COLOR_ACCENT);

    /* Window buttons in taskbar */
    int btn_x = 72;
    for (int i = 0; i < window_count; i++) {
        if (!windows[i].visible) continue;

        uint32_t btn_bg = windows[i].focused ? RGB(55, 60, 80) : RGB(40, 42, 55);
        int btn_w = 120;
        fb_fill_rect(btn_x, ty + 4, btn_w, TASKBAR_HEIGHT - 8, btn_bg);

        /* Active indicator line */
        if (windows[i].focused) {
            fb_fill_rect(btn_x, ty + 1, btn_w, 2, COLOR_ACCENT);
        }

        /* Truncated title */
        char truncated[16];
        strncpy(truncated, windows[i].title, 13);
        truncated[13] = '\0';
        if (strlen(windows[i].title) > 13) {
            truncated[12] = '.';
            truncated[13] = '.';
            truncated[14] = '\0';
        }
        fb_draw_string(btn_x + 6, ty + (TASKBAR_HEIGHT - FONT_HEIGHT) / 2,
                      truncated, COLOR_TEXT, btn_bg);

        btn_x += btn_w + 4;
    }

    /* Clock on right */
    uint32_t secs = timer_get_seconds();
    uint32_t mins = secs / 60;
    uint32_t hrs = (mins / 60) % 24;
    char clock[9];
    clock[0] = '0' + (hrs / 10);
    clock[1] = '0' + (hrs % 10);
    clock[2] = ':';
    clock[3] = '0' + ((mins % 60) / 10);
    clock[4] = '0' + ((mins % 60) % 10);
    clock[5] = ':';
    clock[6] = '0' + ((secs % 60) / 10);
    clock[7] = '0' + ((secs % 60) % 10);
    clock[8] = '\0';
    int clock_x = fb->width - strlen(clock) * FONT_WIDTH - 12;
    fb_draw_string(clock_x, ty + (TASKBAR_HEIGHT - FONT_HEIGHT) / 2,
                  clock, COLOR_TEXT, COLOR_TASKBAR);
}

/* ============================================================================
 * Mouse Cursor
 * ============================================================================ */

/* Simple arrow cursor bitmap (12x16) */
static const uint8_t cursor_data[16] = {
    0x80, 0xC0, 0xE0, 0xF0,
    0xF8, 0xFC, 0xFE, 0xFF,
    0xFC, 0xFC, 0xF8, 0xB0,
    0x30, 0x18, 0x18, 0x00
};
static const uint8_t cursor_mask[16] = {
    0xC0, 0xE0, 0xF0, 0xF8,
    0xFC, 0xFE, 0xFF, 0xFF,
    0xFE, 0xFE, 0xFC, 0xF8,
    0x78, 0x3C, 0x3C, 0x18
};

static void draw_cursor(int mx, int my) {
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            int px = mx + col;
            int py = my + row;
            if (cursor_mask[row] & (0x80 >> col)) {
                uint32_t c = (cursor_data[row] & (0x80 >> col))
                           ? COLOR_WHITE : COLOR_BLACK;
                fb_putpixel(px, py, c);
            }
        }
    }
}

/* ============================================================================
 * Demo Window Content Callbacks
 * ============================================================================ */

static void draw_about_content(window_t* win) {
    int cx = win->x + 10;
    int cy = win->y + TITLEBAR_HEIGHT + 10;
    uint32_t bg = COLOR_WINDOW_BG;

    fb_draw_string(cx, cy, "MyOS v0.1.0", COLOR_ACCENT, bg);
    cy += FONT_HEIGHT + 4;
    fb_draw_string(cx, cy, "A Low-Resource Operating System", COLOR_TEXT, bg);
    cy += FONT_HEIGHT + 12;

    fb_draw_string(cx, cy, "Architecture:  i686 (x86 32-bit)", COLOR_TEXT_DIM, bg);
    cy += FONT_HEIGHT + 2;
    fb_draw_string(cx, cy, "Kernel Type:   Hybrid Microkernel", COLOR_TEXT_DIM, bg);
    cy += FONT_HEIGHT + 2;
    fb_draw_string(cx, cy, "GUI Stack:     Wayland-like Compositor", COLOR_TEXT_DIM, bg);
    cy += FONT_HEIGHT + 2;
    fb_draw_string(cx, cy, "Font:          8x16 Bitmap (VGA style)", COLOR_TEXT_DIM, bg);
    cy += FONT_HEIGHT + 2;
    fb_draw_string(cx, cy, "License:       BSD/MIT (Original Code)", COLOR_TEXT_DIM, bg);
    cy += FONT_HEIGHT + 12;

    fb_draw_string(cx, cy, "Built from scratch - no Linux/Windows code.", COLOR_YELLOW, bg);
    cy += FONT_HEIGHT + 2;
    fb_draw_string(cx, cy, "Designed for VMware / QEMU virtual machines.", COLOR_TEXT_DIM, bg);
}

static void draw_sysinfo_content(window_t* win) {
    int cx = win->x + 10;
    int cy = win->y + TITLEBAR_HEIGHT + 10;
    uint32_t bg = COLOR_WINDOW_BG;
    char buf[32];

    fb_draw_string(cx, cy, "System Information", COLOR_ACCENT, bg);
    cy += FONT_HEIGHT + 8;

    framebuffer_t* fb = fb_get();

    fb_draw_string(cx, cy, "Display:", COLOR_TEXT, bg);
    cy += FONT_HEIGHT + 2;
    fb_draw_string(cx + 16, cy, "Resolution: ", COLOR_TEXT_DIM, bg);
    utoa(fb->width, buf, 10); fb_draw_string(cx + 112, cy, buf, COLOR_TEXT, bg);
    fb_draw_string(cx + 112 + strlen(buf) * FONT_WIDTH, cy, " x ", COLOR_TEXT_DIM, bg);
    utoa(fb->height, buf, 10);
    fb_draw_string(cx + 112 + (strlen(buf) + 3) * FONT_WIDTH + 24, cy, buf, COLOR_TEXT, bg);
    cy += FONT_HEIGHT + 2;
    fb_draw_string(cx + 16, cy, "Color Depth: ", COLOR_TEXT_DIM, bg);
    utoa(fb->bpp, buf, 10); fb_draw_string(cx + 120, cy, buf, COLOR_TEXT, bg);
    fb_draw_string(cx + 120 + strlen(buf) * FONT_WIDTH, cy, " bpp", COLOR_TEXT_DIM, bg);
    cy += FONT_HEIGHT + 8;

    fb_draw_string(cx, cy, "Memory:", COLOR_TEXT, bg);
    cy += FONT_HEIGHT + 2;
    fb_draw_string(cx + 16, cy, "Total: ", COLOR_TEXT_DIM, bg);
    utoa(pmm_get_total_memory() / 1024, buf, 10);
    fb_draw_string(cx + 72, cy, buf, COLOR_TEXT, bg);
    fb_draw_string(cx + 72 + strlen(buf) * FONT_WIDTH, cy, " MB", COLOR_TEXT_DIM, bg);
    cy += FONT_HEIGHT + 2;
    fb_draw_string(cx + 16, cy, "Free:  ", COLOR_TEXT_DIM, bg);
    utoa(pmm_get_free_frames() * 4 / 1024, buf, 10);
    fb_draw_string(cx + 72, cy, buf, COLOR_GREEN, bg);
    fb_draw_string(cx + 72 + strlen(buf) * FONT_WIDTH, cy, " MB", COLOR_TEXT_DIM, bg);
    cy += FONT_HEIGHT + 8;

    fb_draw_string(cx, cy, "Uptime:", COLOR_TEXT, bg);
    cy += FONT_HEIGHT + 2;
    uint32_t secs = timer_get_seconds();
    utoa(secs, buf, 10);
    fb_draw_string(cx + 16, cy, buf, COLOR_TEXT, bg);
    fb_draw_string(cx + 16 + strlen(buf) * FONT_WIDTH, cy, " seconds", COLOR_TEXT_DIM, bg);
}

/* Terminal window state */
static char term_buffer[40][80];
static int  term_lines = 0;
static int  term_col = 0;
static char term_input[128];
static int  term_input_len = 0;
#define TERM_MAX_LINES 40

static void term_clear(void) {
    memset(term_buffer, 0, sizeof(term_buffer));
    term_lines = 0;
    term_col = 0;
}

static void term_add_line(const char* text) {
    if (term_lines >= TERM_MAX_LINES) {
        /* Scroll up */
        for (int i = 0; i < TERM_MAX_LINES - 1; i++)
            memcpy(term_buffer[i], term_buffer[i+1], 80);
        term_lines = TERM_MAX_LINES - 1;
    }
    strncpy(term_buffer[term_lines], text, 79);
    term_buffer[term_lines][79] = '\0';
    term_lines++;
}

static void draw_terminal_content(window_t* win) {
    int cx = win->x + 6;
    int cy = win->y + TITLEBAR_HEIGHT + 4;
    int client_h = win->h - TITLEBAR_HEIGHT - 8;
    int visible_lines = client_h / FONT_HEIGHT;

    /* Dark terminal background */
    fb_fill_rect(win->x + 1, win->y + TITLEBAR_HEIGHT + 1,
                win->w - 2, win->h - TITLEBAR_HEIGHT - 2, RGB(15, 15, 20));

    int start_line = 0;
    if (term_lines > visible_lines - 1)
        start_line = term_lines - (visible_lines - 1);

    for (int i = start_line; i < term_lines && (i - start_line) < visible_lines - 1; i++) {
        fb_draw_string(cx, cy, term_buffer[i], COLOR_TEXT, RGB(15, 15, 20));
        cy += FONT_HEIGHT;
    }

    /* Input line with prompt */
    int iy = win->y + TITLEBAR_HEIGHT + 4 + (visible_lines - 1) * FONT_HEIGHT;
    if (iy < win->y + win->h - FONT_HEIGHT) {
        fb_draw_string(cx, iy, "$ ", COLOR_GREEN, RGB(15, 15, 20));
        fb_draw_string(cx + 2 * FONT_WIDTH, iy, term_input,
                      COLOR_TEXT, RGB(15, 15, 20));
        /* Blinking cursor effect */
        if ((timer_get_ticks() / 50) % 2 == 0) {
            int cursor_x = cx + (2 + term_input_len) * FONT_WIDTH;
            fb_fill_rect(cursor_x, iy, FONT_WIDTH, FONT_HEIGHT, COLOR_ACCENT);
        }
    }
}

static void terminal_on_key(window_t* win, char key) {
    (void)win;
    if (key == '\n') {
        /* Process command */
        char line[128];
        strcpy(line, "$ ");
        strcat(line, term_input);
        term_add_line(line);

        if (strcmp(term_input, "help") == 0) {
            term_add_line("Commands: help, clear, echo, mem, uptime, exit");
        } else if (strcmp(term_input, "clear") == 0) {
            term_clear();
        } else if (strncmp(term_input, "echo ", 5) == 0) {
            term_add_line(term_input + 5);
        } else if (strcmp(term_input, "mem") == 0) {
            char buf[64];
            strcpy(buf, "Total: ");
            char num[12];
            utoa(pmm_get_total_memory() / 1024, num, 10);
            strcat(buf, num);
            strcat(buf, " MB, Free: ");
            utoa(pmm_get_free_frames() * 4 / 1024, num, 10);
            strcat(buf, num);
            strcat(buf, " MB");
            term_add_line(buf);
        } else if (strcmp(term_input, "uptime") == 0) {
            char buf[32];
            char num[12];
            strcpy(buf, "Uptime: ");
            utoa(timer_get_seconds(), num, 10);
            strcat(buf, num);
            strcat(buf, " seconds");
            term_add_line(buf);
        } else if (strcmp(term_input, "exit") == 0) {
            gui_running = false;
        } else if (term_input[0] != '\0') {
            char buf[80];
            strcpy(buf, "Unknown command: ");
            strcat(buf, term_input);
            term_add_line(buf);
        }

        term_input_len = 0;
        memset(term_input, 0, sizeof(term_input));
    } else if (key == '\b') {
        if (term_input_len > 0) {
            term_input_len--;
            term_input[term_input_len] = '\0';
        }
    } else if (term_input_len < 120) {
        term_input[term_input_len++] = key;
        term_input[term_input_len] = '\0';
    }
}

/* ============================================================================
 * GUI Event Handling
 * ============================================================================ */

static int find_window_at(int mx, int my) {
    /* Search from top (last) to bottom (first) */
    for (int i = window_count - 1; i >= 0; i--) {
        window_t* w = &windows[i];
        if (!w->visible) continue;
        if (mx >= w->x && mx < w->x + w->w &&
            my >= w->y && my < w->y + w->h) {
            return i;
        }
    }
    return -1;
}

static bool is_on_close_btn(window_t* win, int mx, int my) {
    if (!win->closable) return false;
    int bx = win->x + win->w - CLOSE_BTN_SIZE - 6;
    int by = win->y + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2;
    return mx >= bx && mx < bx + CLOSE_BTN_SIZE &&
           my >= by && my < by + CLOSE_BTN_SIZE;
}

static bool is_on_titlebar(window_t* win, int mx, int my) {
    return mx >= win->x && mx < win->x + win->w &&
           my >= win->y && my < win->y + TITLEBAR_HEIGHT;
}

static void handle_mouse_events(void) {
    mouse_state_t ms;
    mouse_get_state(&ms);
    static bool start_was_pressed = false;

    if (ms.left) {
        if (!dragging) {
            int idx = find_window_at(ms.x, ms.y);
            if (idx >= 0) {
                /* Check close button */
                if (is_on_close_btn(&windows[idx], ms.x, ms.y)) {
                    gui_destroy_window(&windows[idx]);
                    return;
                }

                /* Focus and raise */
                raise_window(idx);
                /* After raise, it's now at window_count-1 */
                idx = window_count - 1;

                /* Start drag if on title bar */
                if (is_on_titlebar(&windows[idx], ms.x, ms.y)) {
                    dragging = true;
                    drag_win_idx = idx;
                    drag_offset_x = ms.x - windows[idx].x;
                    drag_offset_y = ms.y - windows[idx].y;
                }
            } else {
                /* Clicked outside any window, check if taskbar click */
                framebuffer_t* fb = fb_get();
                int ty = fb->height - TASKBAR_HEIGHT;
                if (ms.y >= ty && ms.y < (int)fb->height) {
                    /* Click is on the taskbar */
                    if (ms.x >= 4 && ms.x < 64) {
                        if (!start_was_pressed) {
                            start_was_pressed = true;

                            /* Create or focus the exit window */
                            window_t* exit_win = NULL;
                            for (int i = 0; i < window_count; i++) {
                                if (strcmp(windows[i].title, "Exit MyOS") == 0) {
                                    exit_win = &windows[i];
                                    raise_window(i);
                                    break;
                                }
                            }
                            if (!exit_win) {
                                int ew = 260;
                                int eh = 180;
                                int ex = (fb->width - ew) / 2;
                                int ey = (fb->height - TASKBAR_HEIGHT - eh) / 2;
                                exit_win = gui_create_window("Exit MyOS", ex, ey, ew, eh);
                                if (exit_win) {
                                    gui_set_draw_callback(exit_win, draw_exit_content);
                                    gui_set_mouse_callback(exit_win, exit_on_mouse);
                                }
                            }
                        }
                    }
                }
            }
        } else {
            /* Continue dragging */
            if (drag_win_idx >= 0 && drag_win_idx < window_count) {
                windows[drag_win_idx].x = ms.x - drag_offset_x;
                windows[drag_win_idx].y = ms.y - drag_offset_y;

                /* Keep window on screen */
                framebuffer_t* fb = fb_get();
                if (windows[drag_win_idx].x < 0) windows[drag_win_idx].x = 0;
                if (windows[drag_win_idx].y < 0) windows[drag_win_idx].y = 0;
                if (windows[drag_win_idx].x + windows[drag_win_idx].w > (int)fb->width)
                    windows[drag_win_idx].x = fb->width - windows[drag_win_idx].w;
                if (windows[drag_win_idx].y + windows[drag_win_idx].h >
                    (int)fb->height - TASKBAR_HEIGHT)
                    windows[drag_win_idx].y = fb->height - TASKBAR_HEIGHT -
                                               windows[drag_win_idx].h;
            }
        }
    } else {
        dragging = false;
        drag_win_idx = -1;
        start_was_pressed = false;
    }

    /* Route mouse position events to focused window's on_mouse if mouse is in its client area */
    if (window_count > 0 && !dragging) {
        window_t* win = &windows[window_count - 1];
        int32_t rx = ms.x - (win->x + 1);
        int32_t ry = ms.y - (win->y + TITLEBAR_HEIGHT);
        if (rx >= 0 && rx < win->w - 2 && ry >= 0 && ry < win->h - TITLEBAR_HEIGHT - 1) {
            if (win->on_mouse) {
                win->on_mouse(win, rx, ry, ms.left);
            }
        }
    }
}

static void handle_keyboard_events(void) {
    while (keyboard_has_key()) {
        char c = keyboard_poll();
        if (c == 0) continue;

        /* Escape exits GUI */
        if (c == 27) {
            gui_running = false;
            return;
        }

        /* Pass to focused window */
        if (focused_index >= 0 && focused_index < window_count) {
            window_t* win = &windows[focused_index];
            if (win->on_key) {
                win->on_key(win, c);
            }
        }
    }
}

/* ============================================================================
 * Compositor Main Loop
 * ============================================================================ */

static void compositor_render(void) {
    /* Draw desktop background */
    draw_desktop_background();

    /* Draw windows (back to front) */
    for (int i = 0; i < window_count; i++) {
        if (windows[i].visible) {
            draw_window(&windows[i]);
        }
    }

    /* Draw taskbar */
    draw_taskbar();

    /* Draw mouse cursor (always on top) */
    mouse_state_t ms;
    mouse_get_state(&ms);
    draw_cursor(ms.x, ms.y);

    /* Swap buffers */
    fb_swap();
}

/* ============================================================================
 * Paint Application
 * ============================================================================ */

#define PAINT_CANVAS_W 300
#define PAINT_CANVAS_H 150
static uint32_t paint_canvas[PAINT_CANVAS_H][PAINT_CANVAS_W];
static uint32_t paint_color = COLOR_RED;
static bool paint_canvas_initialized = false;

static void paint_init(void) {
    if (paint_canvas_initialized) return;
    for (int y = 0; y < PAINT_CANVAS_H; y++) {
        for (int x = 0; x < PAINT_CANVAS_W; x++) {
            paint_canvas[y][x] = COLOR_WHITE;
        }
    }
    paint_canvas_initialized = true;
}

static void draw_paint_content(window_t* win) {
    paint_init();

    int cx = win->x + 1;
    int cy = win->y + TITLEBAR_HEIGHT;
    uint32_t bg = COLOR_WINDOW_BG;

    /* Fill client area background */
    fb_fill_rect(cx, cy, win->w - 2, win->h - TITLEBAR_HEIGHT - 1, bg);

    /* Draw canvas border */
    int canvas_x = cx + 9;
    int canvas_y = cy + 10;
    fb_draw_rect(canvas_x - 1, canvas_y - 1, PAINT_CANVAS_W + 2, PAINT_CANVAS_H + 2, COLOR_BORDER);

    /* Draw canvas pixels */
    for (int y = 0; y < PAINT_CANVAS_H; y++) {
        fb_blit(paint_canvas[y], 0, 0, PAINT_CANVAS_W, 1, canvas_x, canvas_y + y);
    }

    /* Draw palette label */
    fb_draw_string(cx + 10, cy + 172, "Palette:", COLOR_TEXT_DIM, bg);

    /* Colors and their swatches */
    uint32_t colors[7] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW,
        COLOR_ORANGE, COLOR_WHITE, COLOR_BLACK
    };

    int start_x = cx + 80;
    int swatch_y = cy + 170;
    for (int i = 0; i < 7; i++) {
        int sx = start_x + i * 30;
        fb_fill_rect(sx, swatch_y, 24, 16, colors[i]);
        /* Draw a border around swatches */
        fb_draw_rect(sx, swatch_y, 24, 16, COLOR_BORDER);
        /* If selected, draw an active border */
        if (paint_color == colors[i]) {
            fb_draw_rect(sx - 1, swatch_y - 1, 26, 18, COLOR_ACCENT);
            fb_draw_rect(sx - 2, swatch_y - 2, 28, 20, COLOR_WHITE);
        }
    }

    /* Draw selected indicator text */
    fb_draw_string(cx + 80 + 7 * 30, cy + 172, paint_color == COLOR_BLACK ? "Eraser" : "Pen", COLOR_TEXT, bg);
}

static void paint_on_mouse(window_t* win, int32_t x, int32_t y, bool left_button) {
    (void)win;
    if (!left_button) return;

    /* Check if in canvas area */
    int canvas_rx = x - 9;
    int canvas_ry = y - 10;

    if (canvas_rx >= 0 && canvas_rx < PAINT_CANVAS_W &&
        canvas_ry >= 0 && canvas_ry < PAINT_CANVAS_H) {
        /* Draw a brush */
        int brush_size = 2; // radius
        for (int dy = -brush_size; dy <= brush_size; dy++) {
            for (int dx = -brush_size; dx <= brush_size; dx++) {
                int px = canvas_rx + dx;
                int py = canvas_ry + dy;
                if (px >= 0 && px < PAINT_CANVAS_W && py >= 0 && py < PAINT_CANVAS_H) {
                    paint_canvas[py][px] = paint_color;
                }
            }
        }
        win->dirty = true;
        return;
    }

    /* Check if in palette area */
    if (y >= 170 && y < 186) {
        int px = x - 80;
        if (px >= 0 && px < 7 * 30) {
            int idx = px / 30;
            int rem = px % 30;
            if (rem < 24) { /* inside a swatch */
                uint32_t colors[7] = {
                    COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW,
                    COLOR_ORANGE, COLOR_WHITE, COLOR_BLACK
                };
                paint_color = colors[idx];
                win->dirty = true;
            }
        }
    }
}

/* ============================================================================
 * Calculator Application
 * ============================================================================ */

static char calc_input[32] = "0";
static int  calc_input_len = 1;
static int32_t calc_num1 = 0;
static char calc_op = 0;
static bool calc_clear_on_next = false;

static void draw_calc_content(window_t* win) {
    int cx = win->x + 1;
    int cy = win->y + TITLEBAR_HEIGHT;
    uint32_t bg = COLOR_WINDOW_BG;

    /* Fill client area background */
    fb_fill_rect(cx, cy, win->w - 2, win->h - TITLEBAR_HEIGHT - 1, bg);

    /* Draw display box */
    int disp_x = cx + 8;
    int disp_y = cy + 10;
    int disp_w = 220;
    int disp_h = 36;
    fb_fill_rect(disp_x, disp_y, disp_w, disp_h, RGB(240, 242, 245));
    fb_draw_rect(disp_x, disp_y, disp_w, disp_h, COLOR_BORDER);

    /* Draw display text (right-aligned) */
    int text_len = strlen(calc_input);
    int text_x = disp_x + disp_w - 12 - text_len * FONT_WIDTH;
    int text_y = disp_y + (disp_h - FONT_HEIGHT) / 2;
    fb_draw_string(text_x, text_y, calc_input, COLOR_BLACK, RGB(240, 242, 245));

    /* Button labels */
    const char* buttons[4][4] = {
        {"7", "8", "9", "/"},
        {"4", "5", "6", "*"},
        {"1", "2", "3", "-"},
        {"C", "0", "=", "+"}
    };

    /* Draw 4x4 buttons */
    int btn_w = 46;
    int btn_h = 42;
    int start_x = cx + 8;
    int start_y = cy + 60;

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = start_x + c * 56;
            int by = start_y + r * 52;
            
            uint32_t btn_bg = RGB(50, 52, 65);
            uint32_t text_fg = COLOR_TEXT;
            if (r == 3 && c == 0) { /* C */
                btn_bg = RGB(220, 80, 80);
                text_fg = COLOR_WHITE;
            } else if (r == 3 && c == 2) { /* = */
                btn_bg = COLOR_ACCENT;
                text_fg = COLOR_WHITE;
            } else if (c == 3) { /* Operators */
                btn_bg = RGB(70, 75, 95);
                text_fg = COLOR_YELLOW;
            }

            fb_fill_rect(bx, by, btn_w, btn_h, btn_bg);
            fb_draw_rect(bx, by, btn_w, btn_h, COLOR_BORDER);

            /* Highlight top/left button edges for 3D look */
            fb_fill_rect(bx + 1, by + 1, btn_w - 2, 1, blend_color(btn_bg, COLOR_WHITE, 1, 6));
            fb_fill_rect(bx + 1, by + 1, 1, btn_h - 2, blend_color(btn_bg, COLOR_WHITE, 1, 6));

            /* Draw label in center */
            const char* label = buttons[r][c];
            int lx = bx + (btn_w - strlen(label) * FONT_WIDTH) / 2;
            int ly = by + (btn_h - FONT_HEIGHT) / 2;
            fb_draw_string(lx, ly, label, text_fg, btn_bg);
        }
    }
}

static void calc_handle_press(const char* label) {
    if (label[0] >= '0' && label[0] <= '9') {
        if (calc_clear_on_next || (strcmp(calc_input, "0") == 0)) {
            calc_input[0] = '\0';
            calc_input_len = 0;
            calc_clear_on_next = false;
        }
        if (calc_input_len < 15) {
            calc_input[calc_input_len++] = label[0];
            calc_input[calc_input_len] = '\0';
        }
    } else if (strcmp(label, "C") == 0) {
        strcpy(calc_input, "0");
        calc_input_len = 1;
        calc_num1 = 0;
        calc_op = 0;
        calc_clear_on_next = false;
    } else if (strcmp(label, "=") == 0) {
        if (calc_op != 0) {
            int32_t num2 = 0;
            bool neg = false;
            char* p = calc_input;
            if (*p == '-') { neg = true; p++; }
            while (*p >= '0' && *p <= '9') {
                num2 = num2 * 10 + (*p - '0');
                p++;
            }
            if (neg) num2 = -num2;

            int32_t result = 0;
            bool err = false;
            if (calc_op == '+') result = calc_num1 + num2;
            else if (calc_op == '-') result = calc_num1 - num2;
            else if (calc_op == '*') result = calc_num1 * num2;
            else if (calc_op == '/') {
                if (num2 == 0) err = true;
                else result = calc_num1 / num2;
            }

            if (err) {
                strcpy(calc_input, "Error");
            } else {
                itoa(result, calc_input, 10);
            }
            calc_input_len = strlen(calc_input);
            calc_op = 0;
            calc_clear_on_next = true;
        }
    } else { /* Operator: +, -, *, / */
        int32_t val = 0;
        bool neg = false;
        char* p = calc_input;
        if (*p == '-') { neg = true; p++; }
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            p++;
        }
        if (neg) val = -val;

        calc_num1 = val;
        calc_op = label[0];
        calc_clear_on_next = true;
    }
}

static void calc_on_mouse(window_t* win, int32_t x, int32_t y, bool left_button) {
    (void)win;
    static bool calc_mouse_pressed = false;

    if (!left_button) {
        calc_mouse_pressed = false;
        return;
    }

    if (calc_mouse_pressed) return;

    int grid_x = x - 8;
    int grid_y = y - 60;

    if (grid_x >= 0 && grid_x < 4 * 56 && grid_y >= 0 && grid_y < 4 * 52) {
        int col = grid_x / 56;
        int col_rem = grid_x % 56;
        int row = grid_y / 52;
        int row_rem = grid_y % 52;

        if (col_rem < 46 && row_rem < 42) {
            const char* buttons[4][4] = {
                {"7", "8", "9", "/"},
                {"4", "5", "6", "*"},
                {"1", "2", "3", "-"},
                {"C", "0", "=", "+"}
            };
            calc_mouse_pressed = true;
            calc_handle_press(buttons[row][col]);
            win->dirty = true;
        }
    }
}

/* ============================================================================
 * Exit / Power Control Dialog Application
 * ============================================================================ */

static void draw_exit_content(window_t* win) {
    int cx = win->x + 1;
    int cy = win->y + TITLEBAR_HEIGHT;
    uint32_t bg = COLOR_WINDOW_BG;

    /* Fill client area background */
    fb_fill_rect(cx, cy, win->w - 2, win->h - TITLEBAR_HEIGHT - 1, bg);

    /* Draw prompt text */
    const char* prompt = "Select a power action:";
    int px = cx + (win->w - 2 - strlen(prompt) * FONT_WIDTH) / 2;
    fb_draw_string(px, cy + 12, prompt, COLOR_TEXT, bg);

    /* Button 1: Power Off (Red) */
    int btn1_y = cy + 36;
    fb_fill_rect(cx + 20, btn1_y, 218, 28, COLOR_RED);
    fb_draw_rect(cx + 20, btn1_y, 218, 28, COLOR_BORDER);
    /* 3D bevel look */
    fb_fill_rect(cx + 21, btn1_y + 1, 216, 1, blend_color(COLOR_RED, COLOR_WHITE, 1, 6));
    fb_fill_rect(cx + 21, btn1_y + 1, 1, 26, blend_color(COLOR_RED, COLOR_WHITE, 1, 6));
    const char* lbl1 = "Power Off";
    int lx1 = cx + 20 + (218 - strlen(lbl1) * FONT_WIDTH) / 2;
    int ly1 = btn1_y + (28 - FONT_HEIGHT) / 2;
    fb_draw_string(lx1, ly1, lbl1, COLOR_WHITE, COLOR_RED);

    /* Button 2: Restart (Orange) */
    int btn2_y = cy + 74;
    fb_fill_rect(cx + 20, btn2_y, 218, 28, COLOR_ORANGE);
    fb_draw_rect(cx + 20, btn2_y, 218, 28, COLOR_BORDER);
    /* 3D bevel look */
    fb_fill_rect(cx + 21, btn2_y + 1, 216, 1, blend_color(COLOR_ORANGE, COLOR_WHITE, 1, 6));
    fb_fill_rect(cx + 21, btn2_y + 1, 1, 26, blend_color(COLOR_ORANGE, COLOR_WHITE, 1, 6));
    const char* lbl2 = "Restart";
    int lx2 = cx + 20 + (218 - strlen(lbl2) * FONT_WIDTH) / 2;
    int ly2 = btn2_y + (28 - FONT_HEIGHT) / 2;
    fb_draw_string(lx2, ly2, lbl2, COLOR_WHITE, COLOR_ORANGE);

    /* Button 3: Exit to Shell (Accent Blue) */
    int btn3_y = cy + 112;
    fb_fill_rect(cx + 20, btn3_y, 218, 28, COLOR_ACCENT);
    fb_draw_rect(cx + 20, btn3_y, 218, 28, COLOR_BORDER);
    /* 3D bevel look */
    fb_fill_rect(cx + 21, btn3_y + 1, 216, 1, blend_color(COLOR_ACCENT, COLOR_WHITE, 1, 6));
    fb_fill_rect(cx + 21, btn3_y + 1, 1, 26, blend_color(COLOR_ACCENT, COLOR_WHITE, 1, 6));
    const char* lbl3 = "Exit to Shell";
    int lx3 = cx + 20 + (218 - strlen(lbl3) * FONT_WIDTH) / 2;
    int ly3 = btn3_y + (28 - FONT_HEIGHT) / 2;
    fb_draw_string(lx3, ly3, lbl3, COLOR_WHITE, COLOR_ACCENT);
}

static void exit_on_mouse(window_t* win, int32_t x, int32_t y, bool left_button) {
    (void)win;
    static bool exit_mouse_pressed = false;

    if (!left_button) {
        exit_mouse_pressed = false;
        return;
    }

    if (exit_mouse_pressed) return;

    /* Check if click inside buttons' horizontal range */
    if (x >= 20 && x < 238) {
        if (y >= 36 && y < 64) {
            exit_mouse_pressed = true;
            sys_shutdown();
        } else if (y >= 74 && y < 102) {
            exit_mouse_pressed = true;
            sys_reboot();
        } else if (y >= 112 && y < 140) {
            exit_mouse_pressed = true;
            gui_running = false;
        }
    }
}

/* ============================================================================
 * GUI Initialization & Main Loop
 * ============================================================================ */

void gui_init(void) {
    window_count = 0;
    focused_index = -1;
    next_window_id = 1;
    dragging = false;
    gui_running = false;
    prev_mouse_x = -1;
    prev_mouse_y = -1;

    term_clear();
    term_input_len = 0;
    memset(term_input, 0, sizeof(term_input));
}

void gui_run(void) {
    gui_init();

    framebuffer_t* fb = fb_get();
    if (!fb->available) return;

    mouse_set_bounds(fb->width, fb->height);
    gui_running = true;

    /* Create demo windows */
    window_t* about_win = gui_create_window("About MyOS", 80, 60, 420, 280);
    if (about_win) {
        gui_set_draw_callback(about_win, draw_about_content);
    }

    window_t* sysinfo_win = gui_create_window("System Info", 180, 140, 380, 300);
    if (sysinfo_win) {
        gui_set_draw_callback(sysinfo_win, draw_sysinfo_content);
    }

    window_t* term_win = gui_create_window("Terminal", 350, 220, 480, 340);
    if (term_win) {
        gui_set_draw_callback(term_win, draw_terminal_content);
        gui_set_key_callback(term_win, terminal_on_key);
        term_add_line("MyOS Terminal v0.1.0");
        term_add_line("Type 'help' for commands, 'exit' to quit GUI.");
        term_add_line("");
    }

    /* Paint App */
    window_t* paint_win = gui_create_window("Paint", 120, 100, 320, 240);
    if (paint_win) {
        gui_set_draw_callback(paint_win, draw_paint_content);
        gui_set_mouse_callback(paint_win, paint_on_mouse);
    }

    /* Calculator App */
    window_t* calc_win = gui_create_window("Calculator", 400, 120, 240, 320);
    if (calc_win) {
        gui_set_draw_callback(calc_win, draw_calc_content);
        gui_set_mouse_callback(calc_win, calc_on_mouse);
    }

    /* Main compositor loop */
    while (gui_running) {
        handle_mouse_events();
        handle_keyboard_events();
        compositor_render();

        /* Small delay to not burn CPU */
        for (volatile int i = 0; i < 50000; i++);
    }

    /* Cleanup */
    window_count = 0;
}
