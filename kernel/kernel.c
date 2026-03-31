#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "string.h"
#include <stdint.h>

#define CMD_BUF_SIZE 256
#define HISTORY_SIZE   8

static char history[HISTORY_SIZE][CMD_BUF_SIZE];
static int  history_count = 0;

static void kprint(const char *s) { vga_puts(s); }

static void kprint_num(uint32_t n, int base) {
    char buf[32];
    uitoa(n, buf, base);
    vga_puts(buf);
}

static void cmd_help(void) {
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    kprint("Available commands:\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprint("  help          - show this message\n");
    kprint("  clear         - clear the screen\n");
    kprint("  echo <text>   - print text to screen\n");
    kprint("  info          - show OS / hardware info\n");
    kprint("  color         - show color palette\n");
    kprint("  history       - show command history\n");
    kprint("  halt          - halt the CPU\n");
}

static void cmd_info(void) {
    vga_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    kprint("SilentKernel v0.1\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprint("  Arch  : x86 32-bit protected mode\n");
    kprint("  Video : VGA text mode 80x25\n");
    kprint("  Input : PS/2 keyboard (IRQ1)\n");
    kprint("  GDT   : 3 descriptors (null, code, data)\n");
    kprint("  IDT   : 256 entries, PIC remapped to 0x20-0x2F\n");
}

static void cmd_echo(const char *args) {
    kprint(args);
    vga_putchar('\n');
}

static void cmd_color(void) {
    for (int fg = 0; fg < 16; fg++) {
        vga_set_color((vga_color_t)fg, VGA_COLOR_BLACK);
        kprint("##");
    }
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_putchar('\n');
    for (int bg = 0; bg < 8; bg++) {
        vga_set_color(VGA_COLOR_WHITE, (vga_color_t)bg);
        kprint("  ");
    }
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_putchar('\n');
}

static void cmd_history(void) {
    for (int i = 0; i < history_count; i++) {
        char num[8];
        itoa(i + 1, num, 10);
        kprint("  ");
        kprint(num);
        kprint(": ");
        kprint(history[i]);
        vga_putchar('\n');
    }
}

static void run_command(const char *cmd) {
    while (*cmd == ' ') cmd++;
    if (!*cmd) return;

    if (history_count < HISTORY_SIZE) {
        strncpy(history[history_count++], cmd, CMD_BUF_SIZE - 1);
    } else {
        for (int i = 1; i < HISTORY_SIZE; i++)
            strcpy(history[i - 1], history[i]);
        strncpy(history[HISTORY_SIZE - 1], cmd, CMD_BUF_SIZE - 1);
    }

    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "clear") == 0) {
        vga_clear();
    } else if (strcmp(cmd, "info") == 0) {
        cmd_info();
    } else if (strcmp(cmd, "color") == 0) {
        cmd_color();
    } else if (strcmp(cmd, "history") == 0) {
        cmd_history();
    } else if (strcmp(cmd, "halt") == 0) {
        kprint("Halting CPU. Goodbye.\n");
        __asm__ volatile("cli; hlt");
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        cmd_echo(cmd + 5);
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprint("Unknown command: ");
        kprint(cmd);
        kprint("\nType 'help' for a list of commands.\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }
}

static void print_prompt(void) {
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprint("sk> ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static void readline(char *buf, int size) {
    int i = 0;
    buf[0] = '\0';
    while (1) {
        char c = keyboard_getchar();
        if (c == '\n') {
            vga_putchar('\n');
            buf[i] = '\0';
            return;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                buf[i] = '\0';
                vga_putchar('\b');
            }
        } else if (i < size - 1) {
            buf[i++] = c;
            buf[i]   = '\0';
            vga_putchar(c);
        }
    }
}

static void print_banner(void) {
    vga_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    kprint("  ____  _ _            _   _  __                    _ \n");
    kprint(" / ___|(_) | ___ _ __ | |_| |/ /___ _ __ _ __  ___| |\n");
    kprint(" \\___ \\| | |/ _ \\ '_ \\| __| ' // _ \\ '__| '_ \\/ _ \\ |\n");
    kprint("  ___) | | |  __/ | | | |_| . \\  __/ |  | | | |  __/ |\n");
    kprint(" |____/|_|_|\\___|_| |_|\\__|_|\\_\\___|_|  |_| |_|\\___|_|\n");
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    kprint("                                              v0.1\n\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprint("Type 'help' to see available commands.\n\n");
}

void kernel_main(uint32_t magic, void *mbi) {
    (void)magic; (void)mbi;

    vga_init();
    gdt_init();
    idt_init();
    keyboard_init();

    __asm__ volatile("sti");

    print_banner();

    char cmd[CMD_BUF_SIZE];
    while (1) {
        print_prompt();
        readline(cmd, CMD_BUF_SIZE);
        run_command(cmd);
    }
}
