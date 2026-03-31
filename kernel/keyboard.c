#include "keyboard.h"
#include "idt.h"
#include "io.h"
#include <stdint.h>

#define KB_DATA_PORT 0x60

static const char sc_table[] = {
    0,    0,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
    '\b', '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    '\n', 0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,    '*',
    0,   ' '
};

static const char sc_shift_table[] = {
    0,    0,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    '\b', '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"',  '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,    '*',
    0,   ' '
};

#define SC_TABLE_SIZE ((int)(sizeof(sc_table) / sizeof(sc_table[0])))

static volatile char key_buffer = 0;
static int shift_held = 0;
static int caps_lock  = 0;

static void keyboard_handler(registers_t *regs) {
    (void)regs;
    uint8_t sc = inb(KB_DATA_PORT);

    if (sc == 0xAA || sc == 0xB6) { shift_held = 0; return; }
    if (sc == 0x2A || sc == 0x36) { shift_held = 1; return; }
    if (sc == 0x3A) { caps_lock = !caps_lock; return; }

    if (sc & 0x80) return;

    if (sc < SC_TABLE_SIZE) {
        char c;
        int use_shift = shift_held ^ caps_lock;
        if (use_shift && sc_shift_table[sc])
            c = sc_shift_table[sc];
        else
            c = sc_table[sc];
        if (c)
            key_buffer = c;
    }
}

void keyboard_init(void) {
    register_interrupt_handler(33, keyboard_handler);
}

char keyboard_getchar(void) {
    key_buffer = 0;
    while (!key_buffer)
        __asm__ volatile("hlt");
    char c = key_buffer;
    key_buffer = 0;
    return c;
}
