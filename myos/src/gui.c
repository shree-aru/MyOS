/* ============================================================================
 * MyOS - GUI: Compositor, Window Manager & Desktop Environment
 * Wayland-inspired compositor with window management, taskbar, desktop icons,
 * start menu, context menu, and built-in applications.
 * ============================================================================ */

#include "kernel.h"
#include "myfs.h"

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

/* Start Menu state */
static bool start_menu_open = false;

/* Context Menu state */
static bool ctx_menu_open = false;
static int ctx_menu_x = 0;
static int ctx_menu_y = 0;

/* Desktop icon double-click state */
static int icon_last_clicked = -1;
static uint32_t icon_click_tick = 0;

/* Forward declarations */
static void draw_exit_content(window_t* win);
static void exit_on_mouse(window_t* win, int32_t x, int32_t y, bool left_button);
static void spawn_terminal(void);
static void spawn_file_manager(void);
static void spawn_paint(void);
static void spawn_calculator(void);
static void spawn_hex_editor(void);
static void spawn_text_editor(void);
static void spawn_about(void);
static void spawn_sysinfo(void);

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
        for (int dy = -radius; dy <= radius; dy++) {
            int py = cy + dy;
            if (py >= 0 && py < h) {
                // Calculate background gradient color once per scanline
                uint32_t bg = blend_color(COLOR_DESKTOP_TOP, COLOR_DESKTOP_BOT, py, h);
                for (int dx = -radius; dx <= radius; dx++) {
                    if (dx*dx + dy*dy <= radius*radius) {
                        int px = cx + dx;
                        if (px >= 0 && px < (int)fb->width) {
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
                    for (int sy = 0; sy < 3; sy++)
                        for (int sx = 0; sx < 3; sx++)
                            fb_putpixel(nx + i * FONT_WIDTH * 3 + col * 3 + sx,
                                       ny + row * 3 + sy, COLOR_WHITE);
                }
            }
        }
    }

    /* Subtitle */
    const char* subtitle = "Low Resource Operating System  |  Engineered by Shreearu Bisoi";
    int sx = (fb->width - (int)strlen(subtitle) * FONT_WIDTH) / 2;
    fb_draw_string(sx, ny + FONT_HEIGHT * 3 + 10, subtitle,
                  COLOR_TEXT_DIM, COLOR_DESKTOP_BOT);
}

/* ============================================================================
 * Desktop Shortcut Icons
 * ============================================================================ */

#define ICON_COUNT 6
#define ICON_SIZE  48
#define ICON_MARGIN_X 20
#define ICON_MARGIN_Y 16
#define ICON_LABEL_H  18
#define ICON_SPACING  (ICON_SIZE + ICON_LABEL_H + ICON_MARGIN_Y)

typedef struct {
    const char* label;
    uint32_t color;
    const char* symbol; /* 2-char symbol drawn big in the icon */
} desktop_icon_t;

static const desktop_icon_t desktop_icons[ICON_COUNT] = {
    { "Terminal",   RGB(50, 180, 80),   ">_" },
    { "Files",      RGB(220, 180, 50),  "Fi" },
    { "Paint",      RGB(200, 80, 80),   "Pa" },
    { "Calculator", RGB(100, 140, 255), "Ca" },
    { "Hex Editor", RGB(180, 100, 220), "Hx" },
    { "Notepad",    RGB(80, 180, 200),  "Tx" },
};

static int selected_icon = -1;

static void draw_desktop_icons(void) {
    for (int i = 0; i < ICON_COUNT; i++) {
        int ix = ICON_MARGIN_X;
        int iy = 30 + i * ICON_SPACING;
        uint32_t bg_col = desktop_icons[i].color;

        /* Icon background square */
        fb_fill_rect(ix, iy, ICON_SIZE, ICON_SIZE, bg_col);

        /* Selection highlight */
        if (selected_icon == i) {
            fb_draw_rect(ix - 2, iy - 2, ICON_SIZE + 4, ICON_SIZE + 4, COLOR_WHITE);
            fb_draw_rect(ix - 1, iy - 1, ICON_SIZE + 2, ICON_SIZE + 2, COLOR_ACCENT);
        }

        /* 3D top-left bevel */
        fb_fill_rect(ix, iy, ICON_SIZE, 2, blend_color(bg_col, COLOR_WHITE, 1, 3));
        fb_fill_rect(ix, iy, 2, ICON_SIZE, blend_color(bg_col, COLOR_WHITE, 1, 3));

        /* Symbol text inside icon (centered) */
        const char* sym = desktop_icons[i].symbol;
        int sym_len = strlen(sym);
        int tx = ix + (ICON_SIZE - sym_len * FONT_WIDTH) / 2;
        int ty = iy + (ICON_SIZE - FONT_HEIGHT) / 2;
        fb_draw_string(tx, ty, sym, COLOR_WHITE, bg_col);

        /* Label below icon */
        const char* lbl = desktop_icons[i].label;
        int lbl_len = strlen(lbl);
        int lx = ix + (ICON_SIZE - lbl_len * FONT_WIDTH) / 2;
        if (lx < 2) lx = 2;
        int ly = iy + ICON_SIZE + 2;
        fb_draw_string(lx, ly, lbl, COLOR_TEXT, COLOR_DESKTOP_BOT);
    }
}

static int icon_hit_test(int mx, int my) {
    for (int i = 0; i < ICON_COUNT; i++) {
        int ix = ICON_MARGIN_X;
        int iy = 30 + i * ICON_SPACING;
        if (mx >= ix && mx < ix + ICON_SIZE &&
            my >= iy && my < iy + ICON_SIZE + ICON_LABEL_H) {
            return i;
        }
    }
    return -1;
}

static void icon_activate(int idx) {
    switch (idx) {
        case 0: spawn_terminal(); break;
        case 1: spawn_file_manager(); break;
        case 2: spawn_paint(); break;
        case 3: spawn_calculator(); break;
        case 4: spawn_hex_editor(); break;
        case 5: spawn_text_editor(); break;
    }
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

    /* "MyOS" button area */
    uint32_t start_bg = start_menu_open ? RGB(80, 100, 200) : COLOR_ACCENT;
    fb_fill_rect(4, ty + 4, 60, TASKBAR_HEIGHT - 8, start_bg);
    fb_draw_string(12, ty + (TASKBAR_HEIGHT - FONT_HEIGHT) / 2,
                  "MyOS", COLOR_WHITE, start_bg);

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
 * Start Menu
 * ============================================================================ */

#define START_MENU_W 200
#define START_MENU_ITEM_H 28
#define START_MENU_ITEMS 10

static const char* start_menu_labels[START_MENU_ITEMS] = {
    "Terminal",
    "File Manager",
    "Paint",
    "Calculator",
    "Hex Editor",
    "Text Editor",
    "System Info",
    "About MyOS",
    "---",
    "Power Options...",
};

static void draw_start_menu(void) {
    if (!start_menu_open) return;
    framebuffer_t* fb = fb_get();
    int menu_h = START_MENU_ITEMS * START_MENU_ITEM_H + 8;
    int mx = 4;
    int my = fb->height - TASKBAR_HEIGHT - menu_h;

    /* Menu background with shadow */
    fb_fill_rect(mx + 3, my + 3, START_MENU_W, menu_h, RGB(10, 10, 15));
    fb_fill_rect(mx, my, START_MENU_W, menu_h, RGB(30, 32, 42));
    fb_draw_rect(mx, my, START_MENU_W, menu_h, COLOR_BORDER);

    /* Menu header */
    fb_fill_rect(mx + 1, my + 1, START_MENU_W - 2, 3, COLOR_ACCENT);

    /* Menu items */
    for (int i = 0; i < START_MENU_ITEMS; i++) {
        int iy = my + 4 + i * START_MENU_ITEM_H;
        const char* label = start_menu_labels[i];

        if (strcmp(label, "---") == 0) {
            /* Separator */
            fb_fill_rect(mx + 8, iy + START_MENU_ITEM_H / 2, START_MENU_W - 16, 1, COLOR_BORDER);
        } else {
            /* Item icon color dot */
            uint32_t dot_color = COLOR_ACCENT;
            if (i < 6) dot_color = desktop_icons[i].color;
            else if (i == 6) dot_color = COLOR_YELLOW;
            else if (i == 7) dot_color = COLOR_GREEN;
            else if (i == 9) dot_color = COLOR_RED;
            fb_fill_rect(mx + 10, iy + (START_MENU_ITEM_H - 8) / 2, 8, 8, dot_color);

            /* Item text */
            fb_draw_string(mx + 24, iy + (START_MENU_ITEM_H - FONT_HEIGHT) / 2,
                          label, COLOR_TEXT, RGB(30, 32, 42));
        }
    }
}

static int start_menu_hit_test(int mx, int my) {
    if (!start_menu_open) return -1;
    framebuffer_t* fb = fb_get();
    int menu_h = START_MENU_ITEMS * START_MENU_ITEM_H + 8;
    int menu_x = 4;
    int menu_y = fb->height - TASKBAR_HEIGHT - menu_h;

    if (mx >= menu_x && mx < menu_x + START_MENU_W &&
        my >= menu_y && my < menu_y + menu_h) {
        int rel_y = my - menu_y - 4;
        if (rel_y >= 0) {
            int idx = rel_y / START_MENU_ITEM_H;
            if (idx >= 0 && idx < START_MENU_ITEMS)
                return idx;
        }
    }
    return -1;
}

static void start_menu_activate(int idx) {
    start_menu_open = false;
    switch (idx) {
        case 0: spawn_terminal(); break;
        case 1: spawn_file_manager(); break;
        case 2: spawn_paint(); break;
        case 3: spawn_calculator(); break;
        case 4: spawn_hex_editor(); break;
        case 5: spawn_text_editor(); break;
        case 6: spawn_sysinfo(); break;
        case 7: spawn_about(); break;
        case 8: break; /* separator */
        case 9: {
            /* Create exit dialog */
            framebuffer_t* fb = fb_get();
            int ew = 260, eh = 180;
            int ex = (fb->width - ew) / 2;
            int ey = (fb->height - TASKBAR_HEIGHT - eh) / 2;
            window_t* exit_win = gui_create_window("Power Options", ex, ey, ew, eh);
            if (exit_win) {
                gui_set_draw_callback(exit_win, draw_exit_content);
                gui_set_mouse_callback(exit_win, exit_on_mouse);
            }
            break;
        }
    }
}

/* ============================================================================
 * Right-Click Context Menu
 * ============================================================================ */

#define CTX_MENU_W 160
#define CTX_MENU_ITEM_H 24
#define CTX_MENU_ITEMS 5

static const char* ctx_menu_labels[CTX_MENU_ITEMS] = {
    "New Terminal",
    "File Manager",
    "Refresh Desktop",
    "---",
    "About MyOS",
};

static void draw_context_menu(void) {
    if (!ctx_menu_open) return;
    int menu_h = CTX_MENU_ITEMS * CTX_MENU_ITEM_H + 4;

    /* Shadow + background */
    fb_fill_rect(ctx_menu_x + 2, ctx_menu_y + 2, CTX_MENU_W, menu_h, RGB(10, 10, 15));
    fb_fill_rect(ctx_menu_x, ctx_menu_y, CTX_MENU_W, menu_h, RGB(38, 40, 52));
    fb_draw_rect(ctx_menu_x, ctx_menu_y, CTX_MENU_W, menu_h, COLOR_BORDER);

    for (int i = 0; i < CTX_MENU_ITEMS; i++) {
        int iy = ctx_menu_y + 2 + i * CTX_MENU_ITEM_H;
        if (strcmp(ctx_menu_labels[i], "---") == 0) {
            fb_fill_rect(ctx_menu_x + 6, iy + CTX_MENU_ITEM_H / 2,
                         CTX_MENU_W - 12, 1, COLOR_BORDER);
        } else {
            fb_draw_string(ctx_menu_x + 12, iy + (CTX_MENU_ITEM_H - FONT_HEIGHT) / 2,
                          ctx_menu_labels[i], COLOR_TEXT, RGB(38, 40, 52));
        }
    }
}

static int ctx_menu_hit_test(int mx, int my) {
    if (!ctx_menu_open) return -1;
    int menu_h = CTX_MENU_ITEMS * CTX_MENU_ITEM_H + 4;
    if (mx >= ctx_menu_x && mx < ctx_menu_x + CTX_MENU_W &&
        my >= ctx_menu_y && my < ctx_menu_y + menu_h) {
        int rel_y = my - ctx_menu_y - 2;
        if (rel_y >= 0) {
            int idx = rel_y / CTX_MENU_ITEM_H;
            if (idx >= 0 && idx < CTX_MENU_ITEMS) return idx;
        }
    }
    return -1;
}

static void ctx_menu_activate(int idx) {
    ctx_menu_open = false;
    switch (idx) {
        case 0: spawn_terminal(); break;
        case 1: spawn_file_manager(); break;
        case 2: /* Refresh - just close menu, desktop redraws every frame */ break;
        case 3: break; /* separator */
        case 4: spawn_about(); break;
    }
}

/* ============================================================================
 * Mouse Cursor
 * ============================================================================ */

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
 * About Window Content
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
    fb_draw_string(cx, cy, "Designed by:   Shreearu Bisoi", COLOR_TEXT_DIM, bg);
    cy += FONT_HEIGHT + 12;

    fb_draw_string(cx, cy, "Built from scratch - no Linux/Windows code.", COLOR_YELLOW, bg);
    cy += FONT_HEIGHT + 2;
    fb_draw_string(cx, cy, "Runs on VMware / VirtualBox / QEMU.", COLOR_TEXT_DIM, bg);
}

/* ============================================================================
 * System Info Window Content
 * ============================================================================ */

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
    cy += FONT_HEIGHT + 8;

    fb_draw_string(cx, cy, "Storage:", COLOR_TEXT, bg);
    cy += FONT_HEIGHT + 2;
    fb_draw_string(cx + 16, cy, "Files: ", COLOR_TEXT_DIM, bg);
    utoa(myfs_count_files(), buf, 10);
    fb_draw_string(cx + 72, cy, buf, COLOR_TEXT, bg);
    fb_draw_string(cx + 72 + strlen(buf) * FONT_WIDTH, cy, " / 64 slots", COLOR_TEXT_DIM, bg);
}

/* ============================================================================
 * Terminal Window
 * ============================================================================ */

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
            char buf[160];
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
 * File Manager Application
 * ============================================================================ */

static int fm_selected = -1;
static int fm_scroll = 0;

static void draw_filemgr_content(window_t* win) {
    int cx = win->x + 1;
    int cy = win->y + TITLEBAR_HEIGHT + 1;
    int cw = win->w - 2;
    int ch = win->h - TITLEBAR_HEIGHT - 2;
    uint32_t bg = RGB(28, 30, 38);

    /* Fill client area */
    fb_fill_rect(cx, cy, cw, ch, bg);

    /* Header bar */
    fb_fill_rect(cx, cy, cw, 22, RGB(40, 42, 55));
    fb_draw_string(cx + 8, cy + 3, "Name", COLOR_ACCENT, RGB(40, 42, 55));
    fb_draw_string(cx + cw - 120, cy + 3, "Size (B)", COLOR_ACCENT, RGB(40, 42, 55));
    fb_fill_rect(cx, cy + 22, cw, 1, COLOR_BORDER);

    /* File listing */
    int row_h = 20;
    int visible_rows = (ch - 24) / row_h;
    int file_idx = 0;
    int drawn = 0;

    for (int i = 0; i < MYFS_MAX_FILES && drawn < visible_rows; i++) {
        myfs_entry_t* entry = myfs_get_entry(i);
        if (!entry) continue;
        if (file_idx < fm_scroll) { file_idx++; continue; }

        int ry = cy + 24 + drawn * row_h;

        /* Selection highlight */
        if (file_idx == fm_selected) {
            fb_fill_rect(cx + 1, ry, cw - 2, row_h, RGB(50, 60, 100));
        }

        /* Filename with null-safety */
        char name_buf[MYFS_FILENAME_MAX + 1];
        memcpy(name_buf, entry->filename, MYFS_FILENAME_MAX);
        name_buf[MYFS_FILENAME_MAX] = '\0';

        uint32_t text_col = (file_idx == fm_selected) ? COLOR_WHITE : COLOR_TEXT;
        fb_draw_string(cx + 8, ry + 2, name_buf, text_col,
                      (file_idx == fm_selected) ? RGB(50, 60, 100) : bg);

        /* File size */
        char size_buf[12];
        utoa(entry->file_size, size_buf, 10);
        fb_draw_string(cx + cw - 120, ry + 2, size_buf, COLOR_TEXT_DIM,
                      (file_idx == fm_selected) ? RGB(50, 60, 100) : bg);

        file_idx++;
        drawn++;
    }

    if (drawn == 0) {
        fb_draw_string(cx + 20, cy + 40, "(No files on disk)", COLOR_TEXT_DIM, bg);
        fb_draw_string(cx + 20, cy + 60, "Use shell to create files:", COLOR_TEXT_DIM, bg);
        fb_draw_string(cx + 20, cy + 78, "  fscreate myfile", COLOR_GREEN, bg);
    }

    /* Bottom status bar */
    int status_y = cy + ch - 20;
    fb_fill_rect(cx, status_y, cw, 20, RGB(35, 37, 48));
    fb_fill_rect(cx, status_y, cw, 1, COLOR_BORDER);
    char status[40];
    strcpy(status, " Files: ");
    char num[8];
    utoa(myfs_count_files(), num, 10);
    strcat(status, num);
    strcat(status, " / 64");
    fb_draw_string(cx + 4, status_y + 2, status, COLOR_TEXT_DIM, RGB(35, 37, 48));
}

static void filemgr_on_mouse(window_t* win, int32_t x, int32_t y, bool left_button) {
    (void)win;
    if (!left_button) return;

    /* Check if clicking in file list area */
    if (y > 24 && y < win->h - TITLEBAR_HEIGHT - 22) {
        int row_h = 20;
        int clicked_row = (y - 24) / row_h + fm_scroll;

        /* Map clicked_row to actual file index */
        int file_idx = 0;
        for (int i = 0; i < MYFS_MAX_FILES; i++) {
            myfs_entry_t* entry = myfs_get_entry(i);
            if (!entry) continue;
            if (file_idx == clicked_row) {
                fm_selected = file_idx;
                win->dirty = true;
                return;
            }
            file_idx++;
        }
    }
    (void)x;
}

static void filemgr_on_key(window_t* win, char key) {
    (void)win;
    /* Delete key (using 'd') removes selected file */
    if (key == 'd' || key == 'D') {
        if (fm_selected >= 0) {
            int file_idx = 0;
            for (int i = 0; i < MYFS_MAX_FILES; i++) {
                myfs_entry_t* entry = myfs_get_entry(i);
                if (!entry) continue;
                if (file_idx == fm_selected) {
                    myfs_delete(entry->filename);
                    fm_selected = -1;
                    break;
                }
                file_idx++;
            }
        }
    }
}

/* ============================================================================
 * Hex Editor Application
 * ============================================================================ */

static uint32_t hex_sector = 0;
static uint8_t hex_data[512];
static bool hex_loaded = false;
static char hex_input[8];
static int hex_input_len = 0;

static char hex_nibble(uint8_t n) {
    n &= 0x0F;
    return n < 10 ? '0' + n : 'A' + n - 10;
}

static void draw_hex_content(window_t* win) {
    int cx = win->x + 1;
    int cy = win->y + TITLEBAR_HEIGHT + 1;
    int cw = win->w - 2;
    int ch = win->h - TITLEBAR_HEIGHT - 2;
    uint32_t bg = RGB(20, 22, 28);

    fb_fill_rect(cx, cy, cw, ch, bg);

    /* Toolbar */
    fb_fill_rect(cx, cy, cw, 22, RGB(35, 37, 48));
    fb_draw_string(cx + 6, cy + 3, "Sector:", COLOR_TEXT_DIM, RGB(35, 37, 48));

    /* Sector input field */
    fb_fill_rect(cx + 64, cy + 2, 60, 18, RGB(20, 22, 28));
    fb_draw_rect(cx + 64, cy + 2, 60, 18, COLOR_BORDER);
    char sec_str[12];
    utoa(hex_sector, sec_str, 10);
    fb_draw_string(cx + 68, cy + 3, hex_input_len > 0 ? hex_input : sec_str,
                  COLOR_TEXT, RGB(20, 22, 28));

    fb_draw_string(cx + 132, cy + 3, "[Enter=Load]", COLOR_TEXT_DIM, RGB(35, 37, 48));
    fb_fill_rect(cx, cy + 22, cw, 1, COLOR_BORDER);

    if (!hex_loaded) {
        fb_draw_string(cx + 20, cy + 50, "Press Enter to load sector data", COLOR_TEXT_DIM, bg);
        return;
    }

    /* Column headers */
    int hy = cy + 26;
    fb_draw_string(cx + 6, hy, "Offset", COLOR_ACCENT, bg);
    for (int c = 0; c < 16; c++) {
        char hdr[3];
        hdr[0] = hex_nibble(c);
        hdr[1] = '\0';
        fb_draw_string(cx + 60 + c * 22, hy, hdr, COLOR_ACCENT, bg);
    }
    fb_draw_string(cx + 60 + 16 * 22 + 8, hy, "ASCII", COLOR_ACCENT, bg);

    /* Hex dump rows (20 rows of 16 bytes = 320 bytes shown) */
    int rows = 20;
    if (rows * 16 > 512) rows = 512 / 16;
    for (int r = 0; r < rows; r++) {
        int ry = hy + 18 + r * 16;
        int offset = r * 16;

        /* Offset column */
        char off_str[8];
        off_str[0] = hex_nibble((offset >> 8) & 0xF);
        off_str[1] = hex_nibble((offset >> 4) & 0xF);
        off_str[2] = hex_nibble(offset & 0xF);
        off_str[3] = '\0';
        fb_draw_string(cx + 6, ry, off_str, COLOR_TEXT_DIM, bg);

        /* Hex bytes */
        for (int c = 0; c < 16 && (offset + c) < 512; c++) {
            uint8_t byte = hex_data[offset + c];
            char hex_pair[3];
            hex_pair[0] = hex_nibble(byte >> 4);
            hex_pair[1] = hex_nibble(byte & 0xF);
            hex_pair[2] = '\0';

            uint32_t col = (byte == 0) ? COLOR_TEXT_DIM : COLOR_TEXT;
            if (byte >= 0x20 && byte < 0x7F) col = COLOR_GREEN;
            fb_draw_string(cx + 60 + c * 22, ry, hex_pair, col, bg);
        }

        /* ASCII representation */
        for (int c = 0; c < 16 && (offset + c) < 512; c++) {
            uint8_t byte = hex_data[offset + c];
            char ch = (byte >= 0x20 && byte < 0x7F) ? byte : '.';
            char asc[2] = { ch, '\0' };
            uint32_t col = (ch == '.') ? RGB(80, 80, 90) : COLOR_YELLOW;
            fb_draw_string(cx + 60 + 16 * 22 + 8 + c * 8, ry, asc, col, bg);
        }
    }
}

static void hex_on_key(window_t* win, char key) {
    (void)win;
    if (key == '\n') {
        /* Parse sector number and load */
        if (hex_input_len > 0) {
            uint32_t val = 0;
            for (int i = 0; i < hex_input_len; i++) {
                if (hex_input[i] >= '0' && hex_input[i] <= '9')
                    val = val * 10 + (hex_input[i] - '0');
            }
            hex_sector = val;
            hex_input_len = 0;
            memset(hex_input, 0, sizeof(hex_input));
        }
        ata_read_sector(hex_sector, hex_data);
        hex_loaded = true;
    } else if (key == '\b') {
        if (hex_input_len > 0) {
            hex_input_len--;
            hex_input[hex_input_len] = '\0';
        }
    } else if (key >= '0' && key <= '9' && hex_input_len < 6) {
        hex_input[hex_input_len++] = key;
        hex_input[hex_input_len] = '\0';
    }
}

/* ============================================================================
 * Text Editor (Notepad) Application
 * ============================================================================ */

#define EDITOR_MAX_TEXT 2048
#define EDITOR_MAX_LINES 80
#define EDITOR_COLS 60

static char editor_text[EDITOR_MAX_TEXT];
static int editor_len = 0;
static int editor_cursor = 0;
static int editor_scroll = 0;
static char editor_filename[32] = "untitled.txt";
static bool editor_saved = true;

static void draw_editor_content(window_t* win) {
    int cx = win->x + 1;
    int cy = win->y + TITLEBAR_HEIGHT + 1;
    int cw = win->w - 2;
    int ch = win->h - TITLEBAR_HEIGHT - 2;
    uint32_t bg = RGB(22, 24, 30);

    fb_fill_rect(cx, cy, cw, ch, bg);

    /* Toolbar with filename */
    fb_fill_rect(cx, cy, cw, 22, RGB(38, 40, 52));
    fb_draw_string(cx + 6, cy + 3, "File: ", COLOR_TEXT_DIM, RGB(38, 40, 52));
    fb_draw_string(cx + 54, cy + 3, editor_filename, COLOR_TEXT, RGB(38, 40, 52));

    const char* save_hint = editor_saved ? "[F2=Save]" : "[F2=Save *]";
    fb_draw_string(cx + cw - 90, cy + 3, save_hint,
                  editor_saved ? COLOR_TEXT_DIM : COLOR_YELLOW, RGB(38, 40, 52));
    fb_fill_rect(cx, cy + 22, cw, 1, COLOR_BORDER);

    /* Text area */
    int text_y = cy + 24;
    int visible_rows = (ch - 26) / FONT_HEIGHT;
    int drawn_lines = 0;

    /* Calculate line positions */
    int line_starts[EDITOR_MAX_LINES];
    int num_lines = 0;
    line_starts[0] = 0;
    num_lines = 1;
    for (int i = 0; i < editor_len && num_lines < EDITOR_MAX_LINES; i++) {
        if (editor_text[i] == '\n') {
            line_starts[num_lines++] = i + 1;
        }
    }

    /* Draw visible lines */
    for (int l = editor_scroll; l < num_lines && drawn_lines < visible_rows; l++) {
        int start = line_starts[l];
        int end = (l + 1 < num_lines) ? line_starts[l + 1] - 1 : editor_len;
        int ry = text_y + drawn_lines * FONT_HEIGHT;

        /* Line number */
        char lnum[5];
        utoa(l + 1, lnum, 10);
        fb_draw_string(cx + 4, ry, lnum, RGB(80, 85, 100), bg);

        /* Text content */
        int tx = cx + 36;
        for (int i = start; i < end && (tx < cx + cw - 8); i++) {
            char c = editor_text[i];
            if (c == '\n') continue;
            char s[2] = { c, '\0' };
            fb_draw_string(tx, ry, s, COLOR_TEXT, bg);

            /* Draw cursor at position */
            if (i == editor_cursor && (timer_get_ticks() / 50) % 2 == 0) {
                fb_fill_rect(tx, ry, 2, FONT_HEIGHT, COLOR_ACCENT);
            }
            tx += FONT_WIDTH;
        }

        /* Cursor at end of line */
        if (editor_cursor == end && (timer_get_ticks() / 50) % 2 == 0) {
            fb_fill_rect(tx, ry, 2, FONT_HEIGHT, COLOR_ACCENT);
        }

        drawn_lines++;
    }

    /* Status bar */
    int status_y = cy + ch - 18;
    fb_fill_rect(cx, status_y, cw, 18, RGB(35, 37, 48));
    char status[48];
    strcpy(status, " Ln ");
    char num[8];
    /* Find current line */
    int cur_line = 0;
    for (int l = 0; l < num_lines; l++) {
        if (editor_cursor >= line_starts[l]) cur_line = l;
    }
    utoa(cur_line + 1, num, 10);
    strcat(status, num);
    strcat(status, "  Len ");
    utoa(editor_len, num, 10);
    strcat(status, num);
    fb_draw_string(cx + 4, status_y + 1, status, COLOR_TEXT_DIM, RGB(35, 37, 48));
}

static void editor_on_key(window_t* win, char key) {
    (void)win;

    if (key == '\n') {
        /* Insert newline */
        if (editor_len < EDITOR_MAX_TEXT - 1) {
            memmove(&editor_text[editor_cursor + 1], &editor_text[editor_cursor],
                    editor_len - editor_cursor);
            editor_text[editor_cursor] = '\n';
            editor_len++;
            editor_cursor++;
            editor_saved = false;
        }
    } else if (key == '\b') {
        if (editor_cursor > 0) {
            memmove(&editor_text[editor_cursor - 1], &editor_text[editor_cursor],
                    editor_len - editor_cursor);
            editor_cursor--;
            editor_len--;
            editor_saved = false;
        }
    } else if (key == 0x02) {
        /* F2 key mapped to Ctrl+B (0x02) - Save file */
        if (editor_len > 0) {
            editor_text[editor_len] = '\0';
            myfs_write(editor_filename, (const uint8_t*)editor_text, editor_len);
            editor_saved = true;
        }
    } else if (key >= 32 && key < 127) {
        /* Printable character */
        if (editor_len < EDITOR_MAX_TEXT - 1) {
            memmove(&editor_text[editor_cursor + 1], &editor_text[editor_cursor],
                    editor_len - editor_cursor);
            editor_text[editor_cursor] = key;
            editor_len++;
            editor_cursor++;
            editor_saved = false;
        }
    }
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

    uint32_t colors[7] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW,
        COLOR_ORANGE, COLOR_WHITE, COLOR_BLACK
    };

    int start_x = cx + 80;
    int swatch_y = cy + 170;
    for (int i = 0; i < 7; i++) {
        int sx = start_x + i * 30;
        fb_fill_rect(sx, swatch_y, 24, 16, colors[i]);
        fb_draw_rect(sx, swatch_y, 24, 16, COLOR_BORDER);
        if (paint_color == colors[i]) {
            fb_draw_rect(sx - 1, swatch_y - 1, 26, 18, COLOR_ACCENT);
            fb_draw_rect(sx - 2, swatch_y - 2, 28, 20, COLOR_WHITE);
        }
    }

    fb_draw_string(cx + 80 + 7 * 30, cy + 172, paint_color == COLOR_BLACK ? "Eraser" : "Pen", COLOR_TEXT, bg);
}

static void paint_on_mouse(window_t* win, int32_t x, int32_t y, bool left_button) {
    (void)win;
    if (!left_button) return;

    int canvas_rx = x - 9;
    int canvas_ry = y - 10;

    if (canvas_rx >= 0 && canvas_rx < PAINT_CANVAS_W &&
        canvas_ry >= 0 && canvas_ry < PAINT_CANVAS_H) {
        int brush_size = 2;
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

    if (y >= 170 && y < 186) {
        int px = x - 80;
        if (px >= 0 && px < 7 * 30) {
            int idx = px / 30;
            int rem = px % 30;
            if (rem < 24) {
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

    fb_fill_rect(cx, cy, win->w - 2, win->h - TITLEBAR_HEIGHT - 1, bg);

    /* Draw display box */
    int disp_x = cx + 8;
    int disp_y = cy + 10;
    int disp_w = 220;
    int disp_h = 36;
    fb_fill_rect(disp_x, disp_y, disp_w, disp_h, RGB(240, 242, 245));
    fb_draw_rect(disp_x, disp_y, disp_w, disp_h, COLOR_BORDER);

    int text_len = strlen(calc_input);
    int text_x = disp_x + disp_w - 12 - text_len * FONT_WIDTH;
    int text_y = disp_y + (disp_h - FONT_HEIGHT) / 2;
    fb_draw_string(text_x, text_y, calc_input, COLOR_BLACK, RGB(240, 242, 245));

    const char* buttons[4][4] = {
        {"7", "8", "9", "/"},
        {"4", "5", "6", "*"},
        {"1", "2", "3", "-"},
        {"C", "0", "=", "+"}
    };

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
            if (r == 3 && c == 0) {
                btn_bg = RGB(220, 80, 80);
                text_fg = COLOR_WHITE;
            } else if (r == 3 && c == 2) {
                btn_bg = COLOR_ACCENT;
                text_fg = COLOR_WHITE;
            } else if (c == 3) {
                btn_bg = RGB(70, 75, 95);
                text_fg = COLOR_YELLOW;
            }

            fb_fill_rect(bx, by, btn_w, btn_h, btn_bg);
            fb_draw_rect(bx, by, btn_w, btn_h, COLOR_BORDER);
            fb_fill_rect(bx + 1, by + 1, btn_w - 2, 1, blend_color(btn_bg, COLOR_WHITE, 1, 6));
            fb_fill_rect(bx + 1, by + 1, 1, btn_h - 2, blend_color(btn_bg, COLOR_WHITE, 1, 6));

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
    } else {
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

    fb_fill_rect(cx, cy, win->w - 2, win->h - TITLEBAR_HEIGHT - 1, bg);

    const char* prompt = "Select a power action:";
    int px = cx + (win->w - 2 - strlen(prompt) * FONT_WIDTH) / 2;
    fb_draw_string(px, cy + 12, prompt, COLOR_TEXT, bg);

    /* Button 1: Power Off (Red) */
    int btn1_y = cy + 36;
    fb_fill_rect(cx + 20, btn1_y, 218, 28, COLOR_RED);
    fb_draw_rect(cx + 20, btn1_y, 218, 28, COLOR_BORDER);
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
 * Window Spawn Helpers
 * ============================================================================ */

static void spawn_terminal(void) {
    window_t* win = gui_create_window("Terminal", 200, 120, 480, 340);
    if (win) {
        gui_set_draw_callback(win, draw_terminal_content);
        gui_set_key_callback(win, terminal_on_key);
        term_clear();
        term_add_line("MyOS Terminal v0.1.0");
        term_add_line("Type 'help' for commands, 'exit' to quit GUI.");
        term_add_line("");
    }
}

static void spawn_file_manager(void) {
    fm_selected = -1;
    fm_scroll = 0;
    window_t* win = gui_create_window("File Manager", 150, 80, 400, 320);
    if (win) {
        gui_set_draw_callback(win, draw_filemgr_content);
        gui_set_mouse_callback(win, filemgr_on_mouse);
        gui_set_key_callback(win, filemgr_on_key);
    }
}

static void spawn_paint(void) {
    window_t* win = gui_create_window("Paint", 120, 100, 320, 240);
    if (win) {
        gui_set_draw_callback(win, draw_paint_content);
        gui_set_mouse_callback(win, paint_on_mouse);
    }
}

static void spawn_calculator(void) {
    /* Reset calculator state */
    strcpy(calc_input, "0");
    calc_input_len = 1;
    calc_num1 = 0;
    calc_op = 0;
    calc_clear_on_next = false;

    window_t* win = gui_create_window("Calculator", 400, 120, 240, 320);
    if (win) {
        gui_set_draw_callback(win, draw_calc_content);
        gui_set_mouse_callback(win, calc_on_mouse);
    }
}

static void spawn_hex_editor(void) {
    hex_sector = 0;
    hex_loaded = false;
    hex_input_len = 0;
    memset(hex_input, 0, sizeof(hex_input));
    memset(hex_data, 0, sizeof(hex_data));

    window_t* win = gui_create_window("Hex Editor", 100, 60, 560, 400);
    if (win) {
        gui_set_draw_callback(win, draw_hex_content);
        gui_set_key_callback(win, hex_on_key);
        /* Auto-load sector 0 */
        ata_read_sector(0, hex_data);
        hex_loaded = true;
    }
}

static void spawn_text_editor(void) {
    memset(editor_text, 0, sizeof(editor_text));
    editor_len = 0;
    editor_cursor = 0;
    editor_scroll = 0;
    editor_saved = true;
    strcpy(editor_filename, "untitled.txt");

    window_t* win = gui_create_window("Text Editor", 180, 80, 460, 360);
    if (win) {
        gui_set_draw_callback(win, draw_editor_content);
        gui_set_key_callback(win, editor_on_key);
    }
}

static void spawn_about(void) {
    window_t* win = gui_create_window("About MyOS", 200, 120, 420, 280);
    if (win) {
        gui_set_draw_callback(win, draw_about_content);
    }
}

static void spawn_sysinfo(void) {
    window_t* win = gui_create_window("System Info", 180, 100, 380, 340);
    if (win) {
        gui_set_draw_callback(win, draw_sysinfo_content);
    }
}

/* ============================================================================
 * GUI Event Handling
 * ============================================================================ */

static int find_window_at(int mx, int my) {
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
    static bool prev_left = false;
    static bool prev_right = false;

    bool left_click = ms.left && !prev_left;
    bool right_click = ms.right && !prev_right;

    /* Right-click context menu */
    if (right_click) {
        int win_idx = find_window_at(ms.x, ms.y);
        framebuffer_t* fb = fb_get();
        int ty = fb->height - TASKBAR_HEIGHT;
        if (win_idx < 0 && ms.y < ty) {
            /* Right-clicked on desktop */
            ctx_menu_open = true;
            ctx_menu_x = ms.x;
            ctx_menu_y = ms.y;
            /* Ensure menu stays on screen */
            int menu_h = CTX_MENU_ITEMS * CTX_MENU_ITEM_H + 4;
            if (ctx_menu_x + CTX_MENU_W > (int)fb->width)
                ctx_menu_x = fb->width - CTX_MENU_W;
            if (ctx_menu_y + menu_h > ty)
                ctx_menu_y = ty - menu_h;
            start_menu_open = false;
        }
    }

    if (ms.left) {
        if (!dragging) {
            /* Check context menu click first */
            if (ctx_menu_open && left_click) {
                int idx = ctx_menu_hit_test(ms.x, ms.y);
                if (idx >= 0) {
                    ctx_menu_activate(idx);
                    prev_left = ms.left;
                    prev_right = ms.right;
                    return;
                } else {
                    ctx_menu_open = false;
                }
            }

            /* Check start menu click */
            if (start_menu_open && left_click) {
                int idx = start_menu_hit_test(ms.x, ms.y);
                if (idx >= 0 && strcmp(start_menu_labels[idx], "---") != 0) {
                    start_menu_activate(idx);
                    prev_left = ms.left;
                    prev_right = ms.right;
                    return;
                }
                /* Check if click is outside the menu */
                framebuffer_t* fb = fb_get();
                int menu_h = START_MENU_ITEMS * START_MENU_ITEM_H + 8;
                int menu_y = fb->height - TASKBAR_HEIGHT - menu_h;
                if (ms.x < 4 || ms.x >= 4 + START_MENU_W ||
                    ms.y < menu_y || ms.y >= (int)fb->height - TASKBAR_HEIGHT) {
                    if (!(ms.x >= 4 && ms.x < 64 &&
                          ms.y >= (int)fb->height - TASKBAR_HEIGHT)) {
                        start_menu_open = false;
                    }
                }
            }

            int idx = find_window_at(ms.x, ms.y);
            if (idx >= 0) {
                /* Close menus when clicking a window */
                ctx_menu_open = false;
                start_menu_open = false;

                /* Check close button */
                if (is_on_close_btn(&windows[idx], ms.x, ms.y)) {
                    gui_destroy_window(&windows[idx]);
                    prev_left = ms.left;
                    prev_right = ms.right;
                    return;
                }

                /* Focus and raise */
                raise_window(idx);
                idx = window_count - 1;

                /* Start drag if on title bar */
                if (is_on_titlebar(&windows[idx], ms.x, ms.y)) {
                    dragging = true;
                    drag_win_idx = idx;
                    drag_offset_x = ms.x - windows[idx].x;
                    drag_offset_y = ms.y - windows[idx].y;
                }
            } else {
                /* Clicked outside any window */
                framebuffer_t* fb = fb_get();
                int ty = fb->height - TASKBAR_HEIGHT;
                if (ms.y >= ty && ms.y < (int)fb->height) {
                    /* Taskbar click */
                    if (ms.x >= 4 && ms.x < 64 && left_click) {
                        start_menu_open = !start_menu_open;
                        ctx_menu_open = false;
                    }
                } else if (left_click) {
                    /* Desktop click - check icons */
                    int icon_idx = icon_hit_test(ms.x, ms.y);
                    if (icon_idx >= 0) {
                        ctx_menu_open = false;
                        start_menu_open = false;
                        if (icon_idx == icon_last_clicked &&
                            (timer_get_ticks() - icon_click_tick) < 50) {
                            /* Double-click: activate */
                            icon_activate(icon_idx);
                            icon_last_clicked = -1;
                        } else {
                            selected_icon = icon_idx;
                            icon_last_clicked = icon_idx;
                            icon_click_tick = timer_get_ticks();
                        }
                    } else {
                        selected_icon = -1;
                        ctx_menu_open = false;
                        start_menu_open = false;
                    }
                }
            }
        } else {
            /* Continue dragging */
            if (drag_win_idx >= 0 && drag_win_idx < window_count) {
                windows[drag_win_idx].x = ms.x - drag_offset_x;
                windows[drag_win_idx].y = ms.y - drag_offset_y;

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
    }

    /* Route mouse position events to focused window */
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

    prev_left = ms.left;
    prev_right = ms.right;
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
    draw_desktop_background();
    draw_desktop_icons();

    for (int i = 0; i < window_count; i++) {
        if (windows[i].visible) {
            draw_window(&windows[i]);
        }
    }

    draw_taskbar();

    /* Draw overlays (menus) on top of everything */
    draw_start_menu();
    draw_context_menu();

    /* Draw mouse cursor (always on top) */
    mouse_state_t ms;
    mouse_get_state(&ms);
    draw_cursor(ms.x, ms.y);

    fb_swap();
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
    start_menu_open = false;
    ctx_menu_open = false;
    selected_icon = -1;
    icon_last_clicked = -1;

    term_clear();
    term_input_len = 0;
    memset(term_input, 0, sizeof(term_input));
}

void gui_run(void) {
    gui_init();

    framebuffer_t* fb = fb_get();
    if (!fb->available) return;

    /* Flush keyboard buffer before entering GUI to discard remnants from boot selection */
    while (keyboard_has_key()) {
        keyboard_poll();
    }

    mouse_set_bounds(fb->width, fb->height);
    gui_running = true;

    /* Create initial demo windows */
    spawn_terminal();

    uint32_t last_render_tick = 0;

    /* Main compositor loop */
    while (gui_running) {
        handle_mouse_events();
        handle_keyboard_events();

        uint32_t current_tick = timer_get_ticks();
        /* Limit rendering to ~33 FPS (once every 3 ticks) */
        if (current_tick - last_render_tick >= 3) {
            compositor_render();
            last_render_tick = current_tick;
        }

        /* Yield CPU until the next interrupt (timer, keyboard, mouse) */
        sti();
        hlt();
    }

    /* Cleanup */
    window_count = 0;
}
