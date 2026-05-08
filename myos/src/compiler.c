/* ============================================================================
 * MyOS - Custom Native Forth Compiler & Scripting Engine Implementation
 * ============================================================================ */

#include "compiler.h"
#include "myfs.h"

#define FORTH_STACK_MAX 64
static int32_t forth_stack[FORTH_STACK_MAX];
static int forth_sp = 0;

// Dynamic compiled x86 code execution buffer in safe physical memory
static uint8_t* exec_buffer = (uint8_t*)0x2000000;
static uint32_t exec_ptr = 0;

static void forth_push(int32_t val) {
    if (forth_sp < FORTH_STACK_MAX) {
        forth_stack[forth_sp++] = val;
    } else {
        vga_puts("Forth Error: Stack Overflow!\n");
    }
}

static int32_t forth_pop(void) {
    if (forth_sp > 0) {
        return forth_stack[--forth_sp];
    } else {
        vga_puts("Forth Error: Stack Underflow!\n");
        return 0;
    }
}

// Helper to determine if a string is a numeric constant
static bool is_numeric(const char* str) {
    if (!str || *str == '\0') return false;
    int i = 0;
    if (str[0] == '-') {
        if (str[1] == '\0') return false;
        i = 1;
    }
    for (; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') return false;
    }
    return true;
}

// Standard parsing helpers
static int parse_int(const char* str) {
    int val = 0;
    int sign = 1;
    int i = 0;
    if (str[0] == '-') {
        sign = -1;
        i = 1;
    }
    for (; str[i] != '\0'; i++) {
        val = val * 10 + (str[i] - '0');
    }
    return val * sign;
}

// Convert two hexadecimal characters to a single byte
static uint8_t hex_to_byte(char h1, char h2) {
    uint8_t b = 0;
    if (h1 >= '0' && h1 <= '9') b += (h1 - '0') << 4;
    else if (h1 >= 'A' && h1 <= 'F') b += (h1 - 'A' + 10) << 4;
    else if (h1 >= 'a' && h1 <= 'f') b += (h1 - 'a' + 10) << 4;

    if (h2 >= '0' && h2 <= '9') b += (h2 - '0');
    else if (h2 >= 'A' && h2 <= 'F') b += (h2 - 'A' + 10);
    else if (h2 >= 'a' && h2 <= 'f') b += (h2 - 'a' + 10);
    return b;
}

void forth_init(void) {
    forth_sp = 0;
    exec_ptr = 0;
    memset(exec_buffer, 0, 4096); // Clear compilation buffer
}

void forth_run_command(const char* line) {
    char temp[256];
    strncpy(temp, line, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char* tokens[64];
    int token_count = 0;
    
    // Split by whitespace
    char* tok = temp;
    char* start = temp;
    while (*tok != '\0') {
        if (*tok == ' ' || *tok == '\t' || *tok == '\r' || *tok == '\n') {
            *tok = '\0';
            if (strlen(start) > 0 && token_count < 64) {
                tokens[token_count++] = start;
            }
            start = tok + 1;
        }
        tok++;
    }
    if (strlen(start) > 0 && token_count < 64) {
        tokens[token_count++] = start;
    }

    // Process tokens sequentially
    for (int t = 0; t < token_count; t++) {
        char* token = tokens[t];

        if (is_numeric(token)) {
            forth_push(parse_int(token));
        } else if (strcmp(token, "+") == 0) {
            int32_t b = forth_pop();
            int32_t a = forth_pop();
            forth_push(a + b);
        } else if (strcmp(token, "-") == 0) {
            int32_t b = forth_pop();
            int32_t a = forth_pop();
            forth_push(a - b);
        } else if (strcmp(token, "*") == 0) {
            int32_t b = forth_pop();
            int32_t a = forth_pop();
            forth_push(a * b);
        } else if (strcmp(token, "/") == 0) {
            int32_t b = forth_pop();
            int32_t a = forth_pop();
            if (b != 0) {
                forth_push(a / b);
            } else {
                vga_puts("Forth Error: Divide by zero!\n");
                forth_push(0);
            }
        } else if (strcmp(token, "dup") == 0) {
            int32_t a = forth_pop();
            forth_push(a);
            forth_push(a);
        } else if (strcmp(token, "drop") == 0) {
            forth_pop();
        } else if (strcmp(token, "swap") == 0) {
            int32_t b = forth_pop();
            int32_t a = forth_pop();
            forth_push(b);
            forth_push(a);
        } else if (strcmp(token, "over") == 0) {
            if (forth_sp >= 2) {
                forth_push(forth_stack[forth_sp - 2]);
            } else {
                vga_puts("Forth Error: Stack Underflow!\n");
            }
        } else if (strcmp(token, ".") == 0) {
            int32_t val = forth_pop();
            vga_put_dec(val);
            vga_putchar(' ');
        } else if (strcmp(token, ".s") == 0) {
            vga_puts("<");
            vga_put_dec(forth_sp);
            vga_puts("> ");
            for (int i = 0; i < forth_sp; i++) {
                vga_put_dec(forth_stack[i]);
                vga_putchar(' ');
            }
            vga_puts("\n");
        } else if (strcmp(token, "@") == 0) {
            // Peek: pop address, read 32-bit value
            uint32_t* addr = (uint32_t*)forth_pop();
            forth_push(*addr);
        } else if (strcmp(token, "!") == 0) {
            // Poke: pop address, pop value, store it
            uint32_t* addr = (uint32_t*)forth_pop();
            uint32_t val = (uint32_t)forth_pop();
            *addr = val;
        } else if (strcmp(token, "list") == 0) {
            myfs_list();
        } else if (strcmp(token, "save") == 0) {
            // save: filename should be parsed as the next token
            if (t + 1 < token_count) {
                char* filename = tokens[++t];
                int32_t size = forth_pop();
                uint8_t* ptr = (uint8_t*)forth_pop();
                if (myfs_write(filename, ptr, size)) {
                    vga_puts("Forth: File saved successfully.\n");
                } else {
                    vga_puts("Forth Error: Write failed.\n");
                }
            } else {
                vga_puts("Forth Error: Filename parameter missing!\n");
            }
        } else if (strcmp(token, "load") == 0) {
            // load: filename should be parsed as the next token
            if (t + 1 < token_count) {
                char* filename = tokens[++t];
                uint8_t script_buf[1024];
                memset(script_buf, 0, 1024);
                int32_t read_bytes = myfs_read(filename, script_buf, 1023);
                if (read_bytes >= 0) {
                    vga_puts("Forth: Loading script: ");
                    vga_puts(filename);
                    vga_puts("\n");
                    forth_run_command((const char*)script_buf);
                } else {
                    vga_puts("Forth Error: File not found.\n");
                }
            } else {
                vga_puts("Forth Error: Filename parameter missing!\n");
            }
        } else if (strcmp(token, "asm") == 0) {
            // Compile subsequent hex tokens into exec_buffer
            t++;
            exec_ptr = 0;
            memset(exec_buffer, 0, 4096);
            
            while (t < token_count) {
                char* hex_tok = tokens[t];
                if (strcmp(hex_tok, "endasm") == 0) {
                    break;
                }
                
                // Expecting 2 char hex values
                if (strlen(hex_tok) == 2) {
                    uint8_t b = hex_to_byte(hex_tok[0], hex_tok[1]);
                    if (exec_ptr < 4096) {
                        exec_buffer[exec_ptr++] = b;
                    }
                } else {
                    vga_puts("Forth ASM Error: Invalid hex token: ");
                    vga_puts(hex_tok);
                    vga_puts("\n");
                }
                t++;
            }
            vga_puts("Forth: Assembled ");
            vga_put_dec(exec_ptr);
            vga_puts(" bytes of machine code.\n");
        } else if (strcmp(token, "run") == 0) {
            // Execute the compiled x86 buffer and push return value from EAX
            if (exec_ptr > 0) {
                typedef int (*forth_asm_func_t)(void);
                forth_asm_func_t fn = (forth_asm_func_t)exec_buffer;
                int ret_val = fn();
                forth_push(ret_val);
            } else {
                vga_puts("Forth Error: No code compiled to run!\n");
            }
        } else {
            vga_puts("Forth Error: Unknown word: ");
            vga_puts(token);
            vga_puts("\n");
        }
    }
}

void forth_shell(void) {
    char input_buf[256];
    
    vga_puts("\n=============================================\n");
    vga_puts("  MyOS Native Forth Compiler Shell           \n");
    vga_puts("  Available words: + - * / dup drop swap over\n");
    vga_puts("                   @ ! .s list save load      \n");
    vga_puts("                   asm ... endasm, run        \n");
    vga_puts("  Type 'exit' to return to shell             \n");
    vga_puts("=============================================\n");

    for (;;) {
        vga_puts("forth> ");
        
        // Custom readline
        memset(input_buf, 0, 256);
        int len = 0;
        while (len < 255) {
            char c = keyboard_getchar();
            if (c == '\n') {
                vga_putchar('\n');
                break;
            } else if (c == '\b') {
                if (len > 0) {
                    len--;
                    input_buf[len] = '\0';
                    vga_putchar('\b');
                }
            } else if (c >= 32 && c <= 126) {
                input_buf[len++] = c;
                vga_putchar(c);
            }
        }
        
        if (len == 0) continue;
        
        if (strcmp(input_buf, "exit") == 0) {
            vga_puts("Returning to MyOS Shell.\n\n");
            break;
        }
        
        forth_run_command(input_buf);
        vga_puts(" OK\n");
    }
}
