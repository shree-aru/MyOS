/* ============================================================================
 * MyOS - PS/2 Mouse Driver
 * IRQ12 handler with 3-byte packet parsing and cursor tracking
 * ============================================================================ */

#include "kernel.h"

#define MOUSE_PORT   0x60
#define MOUSE_STATUS 0x64
#define MOUSE_CMD    0x64

static volatile int32_t mouse_x = 512;
static volatile int32_t mouse_y = 384;
static volatile int32_t mouse_dx = 0;
static volatile int32_t mouse_dy = 0;
static volatile bool mouse_left   = false;
static volatile bool mouse_right  = false;
static volatile bool mouse_middle = false;
static int32_t mouse_max_x = 1024;
static int32_t mouse_max_y = 768;

/* Mouse packet state machine */
static uint8_t  mouse_cycle = 0;
static int8_t   mouse_bytes[3];

static void mouse_wait_write(void) {
    int timeout = 100000;
    while (timeout-- > 0) {
        if (!(inb(MOUSE_STATUS) & 2)) return;
    }
}

static void mouse_wait_read(void) {
    int timeout = 100000;
    while (timeout-- > 0) {
        if (inb(MOUSE_STATUS) & 1) return;
    }
}

static void mouse_write(uint8_t data) {
    mouse_wait_write();
    outb(MOUSE_CMD, 0xD4);  /* Tell controller we're talking to mouse */
    mouse_wait_write();
    outb(MOUSE_PORT, data);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(MOUSE_PORT);
}

static void mouse_callback(registers_t* regs) {
    (void)regs;

    uint8_t status = inb(MOUSE_STATUS);
    if (!(status & 0x20)) return;  /* Not a mouse packet */

    uint8_t data = inb(MOUSE_PORT);

    switch (mouse_cycle) {
    case 0:
        mouse_bytes[0] = (int8_t)data;
        /* Validate: bit 3 should always be set in first byte */
        if (data & 0x08)
            mouse_cycle = 1;
        break;
    case 1:
        mouse_bytes[1] = (int8_t)data;
        mouse_cycle = 2;
        break;
    case 2:
        mouse_bytes[2] = (int8_t)data;
        mouse_cycle = 0;

        /* Parse packet */
        mouse_left   = mouse_bytes[0] & 0x01;
        mouse_right  = mouse_bytes[0] & 0x02;
        mouse_middle = mouse_bytes[0] & 0x04;

        /* Delta X (with sign extension) */
        int32_t dx = mouse_bytes[1];
        if (mouse_bytes[0] & 0x10)
            dx |= 0xFFFFFF00;  /* Sign extend */

        /* Delta Y (with sign extension, inverted for screen coords) */
        int32_t dy = mouse_bytes[2];
        if (mouse_bytes[0] & 0x20)
            dy |= 0xFFFFFF00;  /* Sign extend */

        mouse_dx += dx;
        mouse_dy -= dy;  /* Invert Y (mouse up = screen up = negative Y) */

        /* Update absolute position with bounds */
        mouse_x += dx;
        mouse_y -= dy;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x >= mouse_max_x) mouse_x = mouse_max_x - 1;
        if (mouse_y >= mouse_max_y) mouse_y = mouse_max_y - 1;
        break;
    }
}

void mouse_init(void) {
    mouse_cycle = 0;

    /* Enable auxiliary mouse device */
    mouse_wait_write();
    outb(MOUSE_CMD, 0xA8);

    /* Enable IRQ12 (mouse interrupt) */
    mouse_wait_write();
    outb(MOUSE_CMD, 0x20);   /* Get compaq status byte */
    mouse_wait_read();
    uint8_t status = inb(MOUSE_PORT);
    status |= 2;             /* Enable IRQ12 */
    status &= ~0x20;         /* Enable mouse clock */
    mouse_wait_write();
    outb(MOUSE_CMD, 0x60);   /* Set compaq status */
    mouse_wait_write();
    outb(MOUSE_PORT, status);

    /* Use default settings */
    mouse_write(0xF6);
    mouse_read();  /* ACK */

    /* Enable data reporting */
    mouse_write(0xF4);
    mouse_read();  /* ACK */

    /* Register IRQ12 handler */
    isr_register_handler(IRQ12, mouse_callback);

    /* Flush any pending data */
    while (inb(MOUSE_STATUS) & 1)
        inb(MOUSE_PORT);
}

void mouse_get_state(mouse_state_t* state) {
    state->x      = mouse_x;
    state->y      = mouse_y;
    state->left   = mouse_left;
    state->right  = mouse_right;
    state->middle = mouse_middle;
    state->dx     = mouse_dx;
    state->dy     = mouse_dy;
    mouse_dx = 0;
    mouse_dy = 0;
}

void mouse_set_bounds(int32_t max_x, int32_t max_y) {
    mouse_max_x = max_x;
    mouse_max_y = max_y;
    if (mouse_x >= max_x) mouse_x = max_x - 1;
    if (mouse_y >= max_y) mouse_y = max_y - 1;
}
