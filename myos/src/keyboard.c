/* ============================================================================
 * MyOS - PS/2 Keyboard Driver
 * IRQ1 handler with scancode set 1 translation and key buffer
 * ============================================================================ */

#include "kernel.h"

/* Scancode set 1 -> ASCII translation table (unshifted) */
static const char scancode_table[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*', 0, ' ', 0, 0,0,0,0,0,0,0,0,0,0, 0, 0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
};

/* Scancode set 1 -> ASCII translation table (shifted) */
static const char scancode_shift_table[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','"','~',
    0, '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*', 0, ' ', 0, 0,0,0,0,0,0,0,0,0,0, 0, 0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
};

/* Key buffer (circular) */
static char key_buffer[KEY_BUFFER_SIZE];
static volatile int kb_head = 0;
static volatile int kb_tail = 0;

/* Modifier state */
static bool shift_pressed = false;
static bool ctrl_pressed  = false;
static bool caps_lock     = false;

/* Scancodes for modifier keys */
#define SC_LSHIFT     0x2A
#define SC_RSHIFT     0x36
#define SC_LSHIFT_REL 0xAA
#define SC_RSHIFT_REL 0xB6
#define SC_CTRL       0x1D
#define SC_CTRL_REL   0x9D
#define SC_CAPSLOCK   0x3A

static void kb_buffer_put(char c) {
    int next = (kb_head + 1) % KEY_BUFFER_SIZE;
    if (next != kb_tail) {
        key_buffer[kb_head] = c;
        kb_head = next;
    }
}

static void keyboard_callback(registers_t* regs) {
    (void)regs;
    uint8_t scancode = inb(0x60);

    /* Handle modifier keys */
    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
        shift_pressed = true;
        return;
    }
    if (scancode == SC_LSHIFT_REL || scancode == SC_RSHIFT_REL) {
        shift_pressed = false;
        return;
    }
    if (scancode == SC_CTRL) {
        ctrl_pressed = true;
        return;
    }
    if (scancode == SC_CTRL_REL) {
        ctrl_pressed = false;
        return;
    }
    if (scancode == SC_CAPSLOCK) {
        caps_lock = !caps_lock;
        return;
    }

    /* Ignore key releases (bit 7 set) */
    if (scancode & 0x80)
        return;

    /* Translate scancode to ASCII */
    char c = 0;
    if (scancode == 0x3C) { // F2 key
        c = 0x02; // Map F2 to Ctrl+B (0x02)
    } else {
        if (shift_pressed)
            c = scancode_shift_table[scancode];
        else
            c = scancode_table[scancode];
    }

    /* Apply caps lock to letters */
    if (caps_lock && c >= 'a' && c <= 'z')
        c -= 32;
    else if (caps_lock && c >= 'A' && c <= 'Z')
        c += 32;

    /* Handle Ctrl combinations */
    if (ctrl_pressed && c) {
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 1;
        } else if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 1;
        }
    }

    if (c)
        kb_buffer_put(c);
}

void keyboard_init(void) {
    kb_head = 0;
    kb_tail = 0;
    shift_pressed = false;
    ctrl_pressed = false;
    caps_lock = false;

    /* Register IRQ1 handler */
    isr_register_handler(IRQ1, keyboard_callback);

    /* Flush keyboard buffer */
    while (inb(0x64) & 1)
        inb(0x60);
}

bool keyboard_has_key(void) {
    return kb_head != kb_tail;
}

char keyboard_poll(void) {
    if (kb_head == kb_tail)
        return 0;
    char c = key_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KEY_BUFFER_SIZE;
    return c;
}

char keyboard_getchar(void) {
    while (kb_head == kb_tail) {
        sti();
        hlt();  /* Sleep until next interrupt */
    }
    char c = key_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KEY_BUFFER_SIZE;
    return c;
}
