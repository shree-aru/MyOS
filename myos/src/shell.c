/* ============================================================================
 * MyOS - Text Shell
 * Basic command-line shell with built-in commands
 * Runs on either VGA text mode or framebuffer console
 * ============================================================================ */

#include "kernel.h"

#define CMD_MAX 256
#define CONSOLE_COLS 80
#define CONSOLE_ROWS 40

/* Framebuffer console state */
static int con_x = 0;
static int con_y = 0;
static int con_max_rows = 0;
static int con_max_cols = 0;
bool use_fb = false;

static void con_init(void) {
    framebuffer_t* fb = fb_get();
    if (fb->available) {
        use_fb = true;
        con_max_cols = fb->width / FONT_WIDTH;
        con_max_rows = fb->height / FONT_HEIGHT;
    } else {
        use_fb = false;
        con_max_cols = VGA_WIDTH;
        con_max_rows = VGA_HEIGHT;
    }
    con_x = 0;
    con_y = 0;
}

static void con_scroll(void) {
    if (con_y >= con_max_rows) {
        if (use_fb) {
            fb_scroll_region(0, 0, fb_get()->width, fb_get()->height, 1);
        } else {
            /* VGA scroll handled by vga_putchar */
        }
        con_y = con_max_rows - 1;
    }
}

void con_putchar(char c) {
    if (use_fb) {
        if (c == '\n') {
            con_x = 0;
            con_y++;
            con_scroll();
        } else if (c == '\b') {
            if (con_x > 0) {
                con_x--;
                fb_draw_char(con_x * FONT_WIDTH, con_y * FONT_HEIGHT,
                            ' ', COLOR_TEXT, COLOR_BLACK);
            }
        } else if (c == '\t') {
            con_x = (con_x + 4) & ~3;
            if (con_x >= con_max_cols) {
                con_x = 0;
                con_y++;
                con_scroll();
            }
        } else {
            fb_draw_char(con_x * FONT_WIDTH, con_y * FONT_HEIGHT,
                        c, COLOR_TEXT, COLOR_BLACK);
            con_x++;
            if (con_x >= con_max_cols) {
                con_x = 0;
                con_y++;
                con_scroll();
            }
        }
    } else {
        vga_putchar(c);
    }
}

void con_flush(void) {
    if (use_fb) {
        fb_swap();
    }
}

void con_puts(const char* str) {
    while (*str)
        con_putchar(*str++);
    con_flush();
}

static void con_put_dec(uint32_t val) {
    char buf[12];
    utoa(val, buf, 10);
    con_puts(buf);
}

static void con_put_hex(uint32_t val) {
    char buf[12];
    con_puts("0x");
    utoa(val, buf, 16);
    con_puts(buf);
}

static void con_clear(void) {
    if (use_fb) {
        fb_fill_rect(0, 0, fb_get()->width, fb_get()->height, COLOR_BLACK);
        fb_swap();
    } else {
        vga_clear();
    }
    con_x = 0;
    con_y = 0;
}

/* ---- Shell commands ---- */

static void cmd_help(void) {
    con_puts("\n");
    con_puts("  Available commands:\n");
    con_puts("  -------------------------------------------\n");
    con_puts("  help      - Show this help message\n");
    con_puts("  clear     - Clear the screen\n");
    con_puts("  echo TEXT - Print text\n");
    con_puts("  mem       - Show memory information\n");
    con_puts("  uptime    - Show system uptime\n");
    con_puts("  gui       - Launch graphical desktop\n");
    con_puts("  forth     - Launch interactive Forth compiler\n");
    con_puts("  fslist    - List all files on MyFS\n");
    con_puts("  fsformat  - Format the MyFS filesystem disk\n");
    con_puts("  fscreate  - Create empty file (fscreate FILENAME)\n");
    con_puts("  fsread    - Read & print file (fsread FILENAME)\n");
    con_puts("  fswrite   - Write text to file (fswrite FILENAME TEXT)\n");
    con_puts("  fsdelete  - Delete file (fsdelete FILENAME)\n");
    con_puts("  reboot    - Reboot the system\n");
    con_puts("  shutdown  - Power off the system\n");
    con_puts("  poweroff  - Power off the system (alias)\n");
    con_puts("  about     - About MyOS\n");
    con_puts("\n");
}

static void cmd_mem(void) {
    con_puts("\n  Memory Information:\n");
    con_puts("  Total: ");
    con_put_dec(pmm_get_total_memory());
    con_puts(" KB (");
    con_put_dec(pmm_get_total_memory() / 1024);
    con_puts(" MB)\n");
    con_puts("  Used frames:  ");
    con_put_dec(pmm_get_used_frames());
    con_puts(" (");
    con_put_dec(pmm_get_used_frames() * 4);
    con_puts(" KB)\n");
    con_puts("  Free frames:  ");
    con_put_dec(pmm_get_free_frames());
    con_puts(" (");
    con_put_dec(pmm_get_free_frames() * 4);
    con_puts(" KB)\n\n");
}

static void cmd_uptime(void) {
    uint32_t secs = timer_get_seconds();
    uint32_t mins = secs / 60;
    uint32_t hrs  = mins / 60;

    con_puts("\n  Uptime: ");
    con_put_dec(hrs);
    con_puts("h ");
    con_put_dec(mins % 60);
    con_puts("m ");
    con_put_dec(secs % 60);
    con_puts("s (");
    con_put_dec(timer_get_ticks());
    con_puts(" ticks)\n\n");
}

static void cmd_about(void) {
    con_puts("\n");
    con_puts("  =============================================\n");
    con_puts("  MyOS - A Low-Resource Operating System\n");
    con_puts("  =============================================\n");
    con_puts("  Version:  0.1.0\n");
    con_puts("  Arch:     i686 (x86 32-bit)\n");
    con_puts("  Kernel:   Hybrid microkernel design\n");
    con_puts("  License:  BSD/MIT (original code)\n");
    con_puts("  =============================================\n");
    con_puts("\n");
}

static void cmd_reboot(void) {
    con_puts("\n  Rebooting...\n");
    sys_reboot();
}

static void cmd_shutdown(void) {
    con_puts("\n  Powering off...\n");
    sys_shutdown();
}

/* ---- Shell main loop ---- */

static char cmd_buf[CMD_MAX];
static int  cmd_len = 0;

static void shell_prompt(void) {
    if (use_fb) {
        /* Draw colored prompt */
        const char* user = "myos";
        const char* sep = ":~$ ";
        int px = con_x * FONT_WIDTH;
        int py = con_y * FONT_HEIGHT;
        /* Green username */
        for (int i = 0; user[i]; i++) {
            fb_draw_char(px, py, user[i], COLOR_GREEN, COLOR_BLACK);
            px += FONT_WIDTH;
            con_x++;
        }
        /* White separator */
        for (int i = 0; sep[i]; i++) {
            fb_draw_char(px, py, sep[i], COLOR_TEXT, COLOR_BLACK);
            px += FONT_WIDTH;
            con_x++;
        }
        fb_swap();
    } else {
        vga_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_puts("myos");
        vga_setcolor(VGA_WHITE, VGA_BLACK);
        vga_puts(":~$ ");
    }
}

static void shell_execute(const char* cmd) {
    /* Skip leading whitespace */
    while (*cmd == ' ') cmd++;

    if (cmd[0] == '\0') return;

    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "clear") == 0) {
        con_clear();
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        con_puts("\n  ");
        con_puts(cmd + 5);
        con_puts("\n\n");
    } else if (strcmp(cmd, "echo") == 0) {
        con_puts("\n\n");
    } else if (strcmp(cmd, "mem") == 0) {
        cmd_mem();
    } else if (strcmp(cmd, "uptime") == 0) {
        cmd_uptime();
    } else if (strcmp(cmd, "gui") == 0) {
        framebuffer_t* fb = fb_get();
        if (fb->available) {
            con_puts("\n  Launching GUI desktop...\n");
            gui_run();
            /* If gui_run returns, go back to shell */
            con_clear();
            con_init();
        } else {
            con_puts("\n  Error: No framebuffer available for GUI.\n");
            con_puts("  Boot with GRUB graphics mode enabled.\n\n");
        }
    } else if (strcmp(cmd, "forth") == 0) {
        forth_shell();
        con_clear();
        con_init();
    } else if (strcmp(cmd, "fslist") == 0) {
        con_puts("\n");
        myfs_list();
        con_puts("\n");
    } else if (strcmp(cmd, "fsformat") == 0) {
        con_puts("\n  Formatting disk...\n");
        myfs_format();
        con_puts("\n");
    } else if (strncmp(cmd, "fscreate ", 9) == 0) {
        const char* name = cmd + 9;
        while (*name == ' ') name++;
        if (myfs_create(name)) {
            con_puts("\n  File created successfully.\n\n");
        } else {
            con_puts("\n  Error: Could not create file.\n\n");
        }
    } else if (strncmp(cmd, "fsdelete ", 9) == 0) {
        const char* name = cmd + 9;
        while (*name == ' ') name++;
        if (myfs_delete(name)) {
            con_puts("\n  File deleted successfully.\n\n");
        } else {
            con_puts("\n  Error: File not found.\n\n");
        }
    } else if (strncmp(cmd, "fsread ", 7) == 0) {
        const char* name = cmd + 7;
        while (*name == ' ') name++;
        
        static uint8_t read_buf[2048];
        memset(read_buf, 0, sizeof(read_buf));
        int32_t size = myfs_read(name, read_buf, sizeof(read_buf) - 1);
        if (size >= 0) {
            con_puts("\n=== ");
            con_puts(name);
            con_puts(" ===\n");
            con_puts((const char*)read_buf);
            con_puts("\n===================\n\n");
        } else {
            con_puts("\n  Error: File not found.\n\n");
        }
    } else if (strncmp(cmd, "fswrite ", 8) == 0) {
        const char* params = cmd + 8;
        while (*params == ' ') params++;
        
        char filename[32];
        memset(filename, 0, 32);
        int idx = 0;
        while (*params != '\0' && *params != ' ' && idx < 31) {
            filename[idx++] = *params++;
        }
        
        if (*params == ' ') {
            params++; // Skip space
            if (myfs_write(filename, (const uint8_t*)params, strlen(params))) {
                con_puts("\n  File written successfully.\n\n");
            } else {
                con_puts("\n  Error: Write failed.\n\n");
            }
        } else {
            con_puts("\n  Usage: fswrite FILENAME TEXT_CONTENT\n\n");
        }
    } else if (strcmp(cmd, "about") == 0) {
        cmd_about();
    } else if (strcmp(cmd, "reboot") == 0) {
        cmd_reboot();
    } else if (strcmp(cmd, "shutdown") == 0 || strcmp(cmd, "poweroff") == 0) {
        cmd_shutdown();
    } else {
        con_puts("\n  Unknown command: ");
        con_puts(cmd);
        con_puts("\n  Type 'help' for available commands.\n\n");
    }
}

void shell_init(void) {
    con_init();
    cmd_len = 0;
    memset(cmd_buf, 0, CMD_MAX);
}

void shell_run(void) {
    con_clear();

    /* Boot banner */
    con_puts("\n");
    con_puts("  ==================================================\n");
    con_puts("       MyOS v0.1.0 - Low Resource Operating System\n");
    con_puts("  ==================================================\n");
    con_puts("  Type 'help' for available commands.\n");
    con_puts("  Type 'gui' to launch the graphical desktop.\n");
    con_puts("\n");

    shell_prompt();

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            con_putchar('\n');
            con_flush();
            cmd_buf[cmd_len] = '\0';
            shell_execute(cmd_buf);
            cmd_len = 0;
            memset(cmd_buf, 0, CMD_MAX);
            shell_prompt();
        } else if (c == '\b') {
            if (cmd_len > 0) {
                cmd_len--;
                cmd_buf[cmd_len] = '\0';
                con_putchar('\b');
                con_flush();
            }
        } else if (cmd_len < CMD_MAX - 1) {
            cmd_buf[cmd_len++] = c;
            con_putchar(c);
            con_flush();
        }
    }
}
