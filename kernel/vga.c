#include "vga.h"
#include "io.h"

static size_t vga_row;
static size_t vga_col;
static uint8_t vga_color;
static uint16_t *vga_buf = (uint16_t *)0xB8000;

static inline uint8_t make_color(vga_color_t fg, vga_color_t bg) {
    return fg | (bg << 4);
}

static inline uint16_t make_entry(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

static void update_cursor(void) {
    uint16_t pos = vga_row * VGA_WIDTH + vga_col;
    outb(0x3D4, 14);
    outb(0x3D5, pos >> 8);
    outb(0x3D4, 15);
    outb(0x3D5, pos & 0xFF);
}

void vga_init(void) {
    vga_row   = 0;
    vga_col   = 0;
    vga_color = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_clear();
}

void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buf[y * VGA_WIDTH + x] = make_entry(' ', vga_color);
    vga_row = 0;
    vga_col = 0;
    update_cursor();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    vga_color = make_color(fg, bg);
}

void vga_set_cursor(size_t x, size_t y) {
    vga_col = x;
    vga_row = y;
    update_cursor();
}

size_t vga_get_col(void) { return vga_col; }
size_t vga_get_row(void) { return vga_row; }

static void scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buf[y * VGA_WIDTH + x] = vga_buf[(y + 1) * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        vga_buf[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = make_entry(' ', vga_color);
    vga_row = VGA_HEIGHT - 1;
}

void vga_putchar(char c) {
    if (c == '\n') {
        vga_col = 0;
        if (++vga_row == VGA_HEIGHT) scroll();
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            vga_buf[vga_row * VGA_WIDTH + vga_col] = make_entry(' ', vga_color);
        }
    } else if (c == '\t') {
        vga_col = (vga_col + 8) & ~(size_t)7;
        if (vga_col >= VGA_WIDTH) {
            vga_col = 0;
            if (++vga_row == VGA_HEIGHT) scroll();
        }
    } else {
        vga_buf[vga_row * VGA_WIDTH + vga_col] = make_entry(c, vga_color);
        if (++vga_col == VGA_WIDTH) {
            vga_col = 0;
            if (++vga_row == VGA_HEIGHT) scroll();
        }
    }
    update_cursor();
}

void vga_puts(const char *str) {
    for (size_t i = 0; str[i]; i++)
        vga_putchar(str[i]);
}
