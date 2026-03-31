# SilentKernel — Complete Beginner's Guide

A guide to understanding every file in this project, written for people new to C and assembly.

---

## Table of Contents

1. [Big Picture — What is a kernel?](#1-big-picture--what-is-a-kernel)
2. [C Basics You Need](#2-c-basics-you-need)
3. [Assembly Basics You Need](#3-assembly-basics-you-need)
4. [How the Project is Built](#4-how-the-project-is-built)
5. [Boot Process — Start to Shell](#5-boot-process--start-to-shell)
6. [File-by-File Walkthrough](#6-file-by-file-walkthrough)
   - [boot.asm — The very first code that runs](#bootasm--the-very-first-code-that-runs)
   - [linker.ld — Where things go in memory](#linkerld--where-things-go-in-memory)
   - [gdt.c + gdt.asm — Telling the CPU about memory](#gdtc--gdtasm--telling-the-cpu-about-memory)
   - [idt.c + isr.asm — Handling interrupts and exceptions](#idtc--israsm--handling-interrupts-and-exceptions)
   - [io.h — Talking to hardware ports](#ioh--talking-to-hardware-ports)
   - [vga.c — Drawing text on screen](#vgac--drawing-text-on-screen)
   - [keyboard.c — Reading key presses](#keyboardc--reading-key-presses)
   - [string.c — String utilities without the standard library](#stringc--string-utilities-without-the-standard-library)
   - [kernel.c — The shell](#kernelc--the-shell)
7. [Concepts Glossary](#7-concepts-glossary)

---

## 1. Big Picture — What is a kernel?

When you turn on a computer, no operating system exists yet. The CPU starts executing code at a fixed address. The **BIOS/UEFI** firmware runs first, does some hardware checks, then looks for a bootable disk.

On that disk is a **bootloader** — in this project we use **GRUB**, a popular one. GRUB reads the kernel file from the disk and loads it into RAM. Then it hands control over to the kernel.

A kernel is the lowest-level software. It runs directly on hardware with no safety net:

- No `printf` from the C standard library — we have to write our own text output
- No `malloc` — we manage memory ourselves
- No operating system calls — we *are* the operating system
- If we crash, the whole computer freezes

SilentKernel is a **bare-metal** kernel: it boots, sets up the CPU, and gives you a simple shell to type commands in. That's it. But doing even that requires understanding a lot of how x86 hardware works.

---

## 2. C Basics You Need

### Types and sizes

Normal C programs rely on the operating system's `int` being "whatever size is natural." In kernel code we can't afford ambiguity, so we use **exact-width types** from `<stdint.h>`:

```c
uint8_t   // unsigned,  8 bits (0 to 255)
uint16_t  // unsigned, 16 bits (0 to 65535)
uint32_t  // unsigned, 32 bits (0 to 4294967295)
int8_t    // signed,    8 bits (-128 to 127)
```

The `u` prefix means unsigned (no negative numbers). The number is how many bits wide.

### Pointers

A pointer is a variable that holds a **memory address** — the location of something in RAM, not the thing itself.

```c
uint16_t *vga_buf = (uint16_t *)0xB8000;
```

This says: `vga_buf` is a pointer to `uint16_t`. Its value is the address `0xB8000`. When we write `vga_buf[5]`, we are reading/writing the 6th 16-bit value starting at address `0xB8000` in RAM. This is how we talk directly to hardware — VGA text mode memory lives at that address.

The `(uint16_t *)` part is a **cast** — it tells the compiler "treat this raw number as a pointer to uint16_t".

### Structs and `__attribute__((packed))`

```c
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));
```

A struct groups related data together. Normally the compiler adds invisible padding between fields so they line up on convenient boundaries. `__attribute__((packed))` tells GCC to remove all padding — every byte is exactly where you say it is. This matters a lot in kernel code because the CPU hardware expects data structures in exact binary layouts.

### Inline assembly

```c
__asm__ volatile("cli; hlt");
```

This drops raw assembly instructions directly inside C code. `cli` = clear interrupts flag (disable hardware interrupts). `hlt` = halt the CPU. `volatile` tells the compiler not to move or remove this instruction.

With input/output:

```c
__asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
```

The colons separate: `"instruction"` : `outputs` : `inputs`. `"a"` means "put `val` in register `eax`". `"Nd"` means "put `port` in register `edx`".

### `static`

`static` on a global variable or function means it can only be seen within the current `.c` file. It's like making something private. You'll see `static` everywhere in this kernel to avoid name collisions between files.

### `volatile`

```c
static volatile char key_buffer = 0;
```

`volatile` tells the compiler "this variable can change at any time, even between instructions — do not cache it in a register, always read it fresh from memory." Used for `key_buffer` because the keyboard interrupt handler writes to it at unpredictable moments.

### `#pragma once` and `#include`

`#pragma once` at the top of a `.h` file means "only include this file once per compilation unit, even if `#include`d multiple times". It prevents duplicate definitions.

`#include "file.h"` pastes that file's contents here (with quotes = look in current directory first). `#include <file.h>` looks in the compiler's standard include paths.

---

## 3. Assembly Basics You Need

This project uses **NASM** assembly with **Intel syntax** (destination on the left).

### Registers

Registers are tiny storage slots inside the CPU — much faster than RAM. On 32-bit x86:

| Register | Full name | Common use |
|----------|-----------|------------|
| `eax` | Accumulator | General purpose, return values |
| `ebx` | Base | General purpose |
| `ecx` | Counter | Loops |
| `edx` | Data | General purpose, I/O ports |
| `esi` | Source Index | Source for memory copies |
| `edi` | Destination Index | Destination for memory copies |
| `esp` | Stack Pointer | Points to top of stack |
| `ebp` | Base Pointer | Points to bottom of current stack frame |

`ax` is the low 16 bits of `eax`. `al` is the low 8 bits of `eax`.

### The stack

The stack is a region of memory used for temporary storage, function arguments, and return addresses. It grows **downward** (toward lower addresses). `esp` always points to the current top.

- `push eax` — subtract 4 from `esp`, write `eax` there
- `pop eax` — read from `esp` into `eax`, add 4 to `esp`

### `call` and `ret`

`call some_function` pushes the return address (next instruction) onto the stack, then jumps to `some_function`.

`ret` pops the return address off the stack and jumps to it.

### `mov`, `add`, `sub`

```nasm
mov eax, 5       ; eax = 5
mov eax, [esp+4] ; eax = value at address (esp + 4)
add esp, 8       ; esp = esp + 8
```

Square brackets `[]` mean "the value at this address" — like dereferencing a pointer in C.

### `jmp` and labels

```nasm
.hang:
    hlt
    jmp .hang
```

Labels are just names for addresses. `jmp .hang` unconditionally jumps back to `.hang`, creating an infinite loop. `hlt` pauses the CPU until an interrupt arrives.

### `iret`

`iret` (interrupt return) is like `ret` but for interrupt handlers. It pops `eip`, `cs`, and `eflags` off the stack, restoring full CPU state before the interrupt.

### `pusha` / `popa`

`pusha` pushes all 8 general-purpose registers onto the stack at once. `popa` pops them all back. Used in interrupt handlers to save/restore state.

### `lgdt` / `lidt`

Load the address of a descriptor table into a special CPU register:
- `lgdt [addr]` — load the Global Descriptor Table
- `lidt [addr]` — load the Interrupt Descriptor Table

### NASM macros

```nasm
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common_stub
%endmacro
```

`%macro NAME num_args` defines a macro. `%1` is the first argument. So `ISR_NOERRCODE 5` expands to a function called `isr5` that pushes 0 and 5, then jumps to the common handler. This avoids writing 32 nearly-identical functions by hand.

---

## 4. How the Project is Built

### The toolchain

You can't use your Mac's regular `gcc` to build a kernel — that compiler targets macOS and links against macOS libraries. You need a **cross-compiler**: `i686-elf-gcc` targets a bare 32-bit x86 machine with no OS.

| Tool | Purpose |
|------|---------|
| `i686-elf-gcc` | Compile C for bare x86 |
| `nasm` | Assemble `.asm` files |
| `i686-elf-grub-mkrescue` | Create a bootable ISO with GRUB |
| `qemu-system-i386` | Emulate an x86 PC to test |

### Makefile walkthrough

```makefile
CFLAGS := -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
          -fno-stack-protector -fno-builtin -nostdlib -Ikernel
```

Key flags:
- `-m32` — produce 32-bit code
- `-ffreestanding` — don't assume a standard library or OS exists
- `-nostdlib` — don't link any standard libraries
- `-fno-builtin` — don't replace `memset` etc. with compiler built-ins
- `-fno-stack-protector` — no stack canaries (they require OS support)

### Build pipeline

```
kernel/*.c  ──(i686-elf-gcc)──► obj/*.o ──┐
kernel/*.asm──(nasm)──────────► obj/*.o ──┤──(ld + linker.ld)──► silentkernel.bin
                                           │
                                           └──(grub-mkrescue)──► SilentKernel.iso
```

1. Each `.c` and `.asm` file is compiled/assembled into an `.o` (object) file
2. The linker combines all `.o` files into one binary using `linker.ld`
3. GRUB wraps the binary into a bootable ISO image

---

## 5. Boot Process — Start to Shell

Here is the exact sequence of events from power-on to your `sk>` prompt:

```
Power on
  └─ BIOS/UEFI runs, finds bootable disk
       └─ GRUB reads SilentKernel.iso
            └─ GRUB finds the Multiboot header in the binary
                 └─ GRUB loads kernel into RAM at 1MB
                      └─ GRUB jumps to _start (boot.asm)
                           └─ _start sets up stack, calls kernel_main()
                                └─ vga_init()     — clear screen
                                └─ gdt_init()     — set up memory segments
                                └─ idt_init()     — set up interrupt handlers + PIC
                                └─ keyboard_init()— register keyboard IRQ
                                └─ sti            — enable hardware interrupts
                                └─ print_banner() — show ASCII art
                                └─ loop: prompt → readline → run_command
```

---

## 6. File-by-File Walkthrough

---

### `boot.asm` — The very first code that runs

```nasm
MULTIBOOT_MAGIC    equ 0x1BADB002
MULTIBOOT_ALIGN    equ 1 << 0
MULTIBOOT_MEMINFO  equ 1 << 1
MULTIBOOT_FLAGS    equ MULTIBOOT_ALIGN | MULTIBOOT_MEMINFO
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)
```

`equ` is like `#define` in C — it creates a constant. These constants define the **Multiboot specification** header that GRUB looks for to identify a bootable kernel. The checksum must make the sum of all three values equal zero — it's a sanity check for GRUB.

```nasm
section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM
```

`section .multiboot` places this data in a special section. The linker script puts this section first, so it appears very early in the binary where GRUB can find it. `dd` means "define doubleword" (4 bytes). `align 4` ensures 4-byte alignment.

```nasm
section .bss
align 16
stack_bottom:
    resb 16384    ; reserve 16384 bytes = 16KB
stack_top:
```

`.bss` is the section for uninitialized data — it doesn't take up space in the binary, the loader just zeros it out. `resb N` reserves N bytes. We reserve 16KB for our stack. `stack_bottom` and `stack_top` are just labels (addresses) — `stack_top` is at the higher address.

```nasm
_start:
    mov esp, stack_top   ; point stack pointer to top of our stack
    push ebx             ; push multiboot info pointer (arg 2 for kernel_main)
    push eax             ; push multiboot magic number (arg 1 for kernel_main)
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
```

This is where execution begins. We set up the stack, push the two arguments GRUB left in `eax` and `ebx`, then call `kernel_main`. The C function signature is `void kernel_main(uint32_t magic, void *mbi)` — C calling convention reads arguments from the stack in order. If `kernel_main` ever returns (it shouldn't), we disable interrupts and halt forever.

---

### `linker.ld` — Where things go in memory

```ld
ENTRY(_start)
```

Tells the linker that `_start` (in `boot.asm`) is the program's entry point.

```ld
. = 1M;
```

The `.` is the "location counter" — where we are in memory. `1M` = `0x100000` = 1 megabyte. We load at 1MB to avoid the BIOS data area (which occupies lower memory).

```ld
.text BLOCK(4K) : ALIGN(4K)
{
    *(.multiboot)   ; multiboot header goes first
    *(.text)        ; then all code sections from all .o files
}
```

`BLOCK(4K)` aligns to a 4KB page boundary. `*(.text)` means "collect the `.text` section from every object file". The sections in order:

| Section | Contents |
|---------|---------|
| `.text` | Executable code |
| `.rodata` | Read-only data (string literals, const arrays) |
| `.data` | Initialized global variables |
| `.bss` | Uninitialized global variables (zeroed at boot) |

---

### `gdt.c` + `gdt.asm` — Telling the CPU about memory

#### What is the GDT?

When x86 CPUs boot, they start in **real mode** — 16-bit, with only 1MB of address space (legacy from the 1980s). GRUB switches the CPU to **protected mode** — 32-bit with 4GB of address space — before handing control to our kernel.

In protected mode, memory addresses aren't used directly. Instead, segment registers (`cs`, `ds`, etc.) hold **selectors** — indices into the GDT. The GDT is an array of **descriptors** that define memory regions. When code or data is accessed, the CPU looks up the descriptor to get the real base address and size limit.

SilentKernel uses a **flat memory model**: both code and data segments span the entire 4GB address space (base=0, limit=0xFFFFFFFF). This means segments are effectively unused — any address you use is the real physical address. Most modern kernels do this.

#### The GDT entry structure

```c
struct gdt_entry {
    uint16_t limit_low;   // low 16 bits of segment size
    uint16_t base_low;    // low 16 bits of base address
    uint8_t  base_mid;    // middle 8 bits of base address
    uint8_t  access;      // permission bits
    uint8_t  granularity; // high bits of limit + flags
    uint8_t  base_high;   // high 8 bits of base address
} __attribute__((packed));
```

The fields are split up in a strange order for historical reasons (Intel compatibility with older hardware). `__attribute__((packed))` is critical — the CPU reads this structure at a specific binary layout.

#### The three descriptors

```c
gdt_set_gate(0, 0, 0,          0x00, 0x00);  // entry 0: null (required)
gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);  // entry 1: kernel code
gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);  // entry 2: kernel data
```

- Entry 0 must always be null — the CPU requires it
- `0x9A` = `10011010` in binary: present, ring 0 (kernel privilege), executable, readable
- `0x92` = `10010010` in binary: present, ring 0, writable (not executable)
- `0xCF` = `11001111`: 4KB granularity (limit is in pages, not bytes), 32-bit mode

#### Flushing the GDT (gdt.asm)

```nasm
gdt_flush:
    mov eax, [esp+4]  ; get the address of gdtp struct
    lgdt [eax]        ; load it into the GDTR register
    mov ax, 0x10      ; 0x10 = selector for entry 2 (data segment)
    mov ds, ax        ; reload all data segment registers
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush   ; 0x08 = selector for entry 1 (code segment)
.flush:               ; far jump reloads CS register
    ret
```

After `lgdt`, we must reload all segment registers. Data segments (`ds`, `es`, etc.) can be loaded directly. The code segment (`cs`) can only be changed with a **far jump** — `jmp 0x08:.flush` jumps to `.flush` with segment selector `0x08`.

---

### `idt.c` + `isr.asm` — Handling interrupts and exceptions

#### What is an interrupt?

An **interrupt** is a signal that interrupts whatever the CPU is currently doing, saves its state, runs a handler function, then resumes. There are two kinds:

- **Exceptions** (vectors 0–31): Generated by the CPU itself when something goes wrong. For example, dividing by zero triggers exception 0. A page fault (accessing unmapped memory) triggers exception 14.
- **IRQs** (hardware interrupts, vectors 32–47): Generated by hardware devices. The keyboard fires IRQ1 when a key is pressed. The timer fires IRQ0.

#### The 8259 PIC (Programmable Interrupt Controller)

The PIC is a chip that collects hardware IRQ signals and forwards them to the CPU. There are two PICs chained together (master and slave), each handling 8 IRQs.

**Problem:** By default, the PIC maps IRQ0–15 to CPU vectors 0–15 — which overlap with the CPU exception vectors (0–31). This would make it impossible to tell whether vector 14 is a page fault exception or an IRQ.

**Solution:** Remap the PIC. `pic_remap()` in `idt.c` reinitializes both PICs and maps:
- IRQ0–7 → vectors 0x20–0x27 (32–39)
- IRQ8–15 → vectors 0x28–0x2F (40–47)

Now exceptions are 0–31 and hardware IRQs are 32–47 — no overlap.

```c
static void pic_remap(void) {
    outb(PIC1_CMD,  0x11);  // start init sequence
    outb(PIC2_CMD,  0x11);
    outb(PIC1_DATA, 0x20);  // PIC1 vector offset = 32
    outb(PIC2_DATA, 0x28);  // PIC2 vector offset = 40
    outb(PIC1_DATA, 0x04);  // tell PIC1 about PIC2 on IRQ2
    outb(PIC2_DATA, 0x02);  // tell PIC2 its cascade identity
    outb(PIC1_DATA, 0x01);  // 8086 mode
    outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, 0x00);  // unmask all IRQs on PIC1
    outb(PIC2_DATA, 0x00);  // unmask all IRQs on PIC2
}
```

#### The IDT entry structure

```c
struct idt_entry {
    uint16_t base_low;   // low 16 bits of handler address
    uint16_t sel;        // code segment selector (0x08)
    uint8_t  zero;       // always 0
    uint8_t  flags;      // type and attributes (0x8E = interrupt gate, ring 0)
    uint16_t base_high;  // high 16 bits of handler address
} __attribute__((packed));
```

Each entry points to a handler function and specifies what segment the handler runs in.

`flags = 0x8E` = `10001110` = present, ring 0, 32-bit interrupt gate.

#### The interrupt stubs (isr.asm)

Every interrupt needs its own tiny assembly stub before reaching the C handler, because some exceptions push an error code and some don't — the C handler expects a consistent stack layout either way.

```nasm
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0     ; push fake error code (this exception has none)
    push dword %1    ; push interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1    ; push interrupt number (CPU already pushed error code)
    jmp isr_common_stub
%endmacro
```

After the stubs normalize the stack, `isr_common_stub` saves all registers, sets up kernel data segments, and calls the C `isr_handler`:

```nasm
isr_common_stub:
    pusha           ; push eax, ecx, edx, ebx, esp, ebp, esi, edi
    push ds         ; save segment registers
    push es
    push fs
    push gs
    mov ax, 0x10    ; load kernel data segment
    mov ds, ax
    ...
    push esp        ; pass pointer to the saved register state as argument
    call isr_handler
    add esp, 4      ; clean up the argument
    pop gs          ; restore segment registers
    ...
    popa            ; restore general-purpose registers
    add esp, 8      ; skip the int_no and err_code we pushed earlier
    iret            ; return from interrupt (restores eip, cs, eflags)
```

The `push esp` trick is clever — by pushing the stack pointer, we pass a pointer to all the saved registers to `isr_handler`. That's why `isr_handler(registers_t *regs)` works — `regs` points right to the stack where `pusha` saved everything.

#### The registers_t struct

```c
typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // saved by pusha
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;             // saved by CPU
} registers_t;
```

This struct matches exactly what's on the stack when `isr_handler` is called — the fields are in the reverse order of how they were pushed.

#### Exception handler

```c
void isr_handler(registers_t *regs) {
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_puts("\n*** EXCEPTION: ");
    if (regs->int_no < 20)
        vga_puts(exception_names[regs->int_no]);
    else
        vga_puts("Unknown");
    vga_puts(" ***\n");
    __asm__ volatile("cli; hlt");  // halt — no recovery
}
```

If a CPU exception occurs (divide by zero, etc.), the kernel prints it in red and halts. There's no recovery — in a real OS you'd print a more detailed crash dump.

#### IRQ handler

```c
void irq_handler(registers_t *regs) {
    if (interrupt_handlers[regs->int_no])
        interrupt_handlers[regs->int_no](regs);  // call registered C handler

    if (regs->int_no >= 40)
        outb(PIC2_CMD, 0x20);  // send EOI to slave PIC if IRQ8-15
    outb(PIC1_CMD, 0x20);      // always send EOI to master PIC
}
```

After calling the handler, we must send an **End of Interrupt (EOI)** signal back to the PIC (`0x20` to command port). If we forget this, the PIC won't send any more interrupts of that type.

---

### `io.h` — Talking to hardware ports

Hardware devices on x86 are controlled through **I/O ports** — a separate address space from RAM with 65536 ports. You communicate with the PIC, VGA cursor, keyboard controller, etc. through ports.

```c
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
```

- `outb` — write one byte to a port (`out` instruction)
- `inb` — read one byte from a port (`in` instruction)
- `static inline` — the function body is copy-pasted at every call site (no function call overhead)

```c
static inline void io_wait(void) {
    outb(0x80, 0);  // port 0x80 is used for POST codes — writing to it wastes ~1 microsecond
}
```

Some old hardware needs a brief delay between commands. Writing to port `0x80` (a diagnostic port that does nothing harmful) takes about 1 microsecond on real hardware — just long enough.

---

### `vga.c` — Drawing text on screen

#### The VGA text buffer

In VGA text mode, the screen is 80 columns × 25 rows = 2000 characters. The hardware maps this directly to RAM at physical address **0xB8000**. Every character takes 2 bytes:

```
Byte 1: ASCII character code
Byte 2: Color attribute (high nibble = background, low nibble = foreground)
```

So to put a green 'A' at position (col=5, row=2):
```c
uint16_t *vga_buf = (uint16_t *)0xB8000;
uint8_t color = VGA_COLOR_GREEN | (VGA_COLOR_BLACK << 4);
vga_buf[2 * 80 + 5] = (uint16_t)'A' | ((uint16_t)color << 8);
```

#### Scrolling

When the cursor reaches row 25, `scroll()` is called:

```c
static void scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buf[y * VGA_WIDTH + x] = vga_buf[(y + 1) * VGA_WIDTH + x];
    // clear the last row
    for (size_t x = 0; x < VGA_WIDTH; x++)
        vga_buf[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = make_entry(' ', vga_color);
    vga_row = VGA_HEIGHT - 1;
}
```

It copies every row up by one, then blanks the last row.

#### Hardware cursor

The blinking cursor position is controlled via I/O ports `0x3D4`/`0x3D5` (VGA CRT controller):

```c
static void update_cursor(void) {
    uint16_t pos = vga_row * VGA_WIDTH + vga_col;  // linear position
    outb(0x3D4, 14);          // select register 14 (cursor high byte)
    outb(0x3D5, pos >> 8);    // write high 8 bits
    outb(0x3D4, 15);          // select register 15 (cursor low byte)
    outb(0x3D5, pos & 0xFF);  // write low 8 bits
}
```

---

### `keyboard.c` — Reading key presses

#### Scancodes

The PS/2 keyboard doesn't send ASCII characters — it sends **scancodes**. Each physical key has a number. When a key is pressed, the keyboard sends that number. When released, it sends the number with the high bit set (`| 0x80`).

A lookup table converts scancodes to ASCII:

```c
static const char sc_table[] = {
    0,    0,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
    '\b', '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    ...
};
```

Index 2 = `'1'` means scancode 2 maps to the character `'1'`.

#### The keyboard interrupt handler

```c
static void keyboard_handler(registers_t *regs) {
    uint8_t sc = inb(KB_DATA_PORT);  // read scancode from port 0x60

    if (sc == 0xAA || sc == 0xB6) { shift_held = 0; return; }  // shift released
    if (sc == 0x2A || sc == 0x36) { shift_held = 1; return; }  // shift pressed
    if (sc == 0x3A) { caps_lock = !caps_lock; return; }         // caps lock

    if (sc & 0x80) return;  // key release — ignore

    if (sc < SC_TABLE_SIZE) {
        int use_shift = shift_held ^ caps_lock;  // XOR: shift OR caps, not both
        char c = use_shift ? sc_shift_table[sc] : sc_table[sc];
        if (c) key_buffer = c;
    }
}
```

`^` is XOR — `shift_held ^ caps_lock` is 1 if exactly one is true. Holding shift while caps lock is on cancels out (types lowercase again), just like a real keyboard.

#### Waiting for input

```c
char keyboard_getchar(void) {
    key_buffer = 0;
    while (!key_buffer)
        __asm__ volatile("hlt");  // sleep until next interrupt
    char c = key_buffer;
    key_buffer = 0;
    return c;
}
```

`hlt` pauses the CPU until an interrupt fires. Since interrupts are enabled (`sti` was called in `kernel_main`), when the user presses a key, the keyboard IRQ fires, the handler runs and fills `key_buffer`, then `hlt` returns, and the `while` loop sees the new value. This is efficient — the CPU isn't burning power in a tight empty loop.

---

### `string.c` — String utilities without the standard library

A freestanding kernel can't call `strlen`, `strcmp`, `memset`, etc. from the C standard library — those come from `libc`, which isn't linked. So we implement our own:

```c
size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len]) len++;  // keep going until we hit the null terminator '\0'
    return len;
}
```

C strings are **null-terminated** — a sequence of bytes ending with `'\0'` (byte value 0). This is a convention, not hardware-enforced. `strlen` counts bytes until it hits that zero.

```c
int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}
```

Walk both strings simultaneously until they differ or one ends. Return the difference of the differing characters: 0 means equal, negative means `a < b`, positive means `a > b`.

```c
void itoa(int value, char *buf, int base) { ... }
```

Converts an integer to a string in any base (10 for decimal, 16 for hex, etc.). The algorithm extracts digits in reverse order into a temporary buffer, then reverses them into `buf`.

---

### `kernel.c` — The shell

This is the heart of what the user sees. After hardware is initialized, the kernel runs a simple **read-eval-print loop (REPL)**:

```
while (1) {
    print "sk> "
    read a line of input
    parse and run the command
}
```

#### readline

```c
static void readline(char *buf, int size) {
    int i = 0;
    buf[0] = '\0';
    while (1) {
        char c = keyboard_getchar();
        if (c == '\n') {
            vga_putchar('\n');
            buf[i] = '\0';
            return;
        } else if (c == '\b') {    // backspace
            if (i > 0) {
                i--;
                buf[i] = '\0';
                vga_putchar('\b');  // move cursor back and erase
            }
        } else if (i < size - 1) {
            buf[i++] = c;
            buf[i]   = '\0';
            vga_putchar(c);        // echo the character
        }
    }
}
```

Reads characters one by one, handles backspace, and returns when Enter is pressed.

#### Command dispatch

```c
static void run_command(const char *cmd) {
    while (*cmd == ' ') cmd++;  // skip leading spaces
    if (!*cmd) return;          // empty line — do nothing

    // save to history ...

    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        cmd_echo(cmd + 5);  // pass everything after "echo " as argument
    } ...
}
```

`strncmp(cmd, "echo ", 5)` checks if the command *starts with* `"echo "`. Then `cmd + 5` is a pointer 5 bytes forward — pointing to the text after `"echo "`.

#### Command history

```c
static char history[HISTORY_SIZE][CMD_BUF_SIZE];  // 8 slots of 256 chars each
static int  history_count = 0;
```

This is a 2D array — `history[i]` is the i-th command string. When it fills up, entries shift forward (oldest dropped) to make room for the new one:

```c
for (int i = 1; i < HISTORY_SIZE; i++)
    strcpy(history[i - 1], history[i]);  // shift everything left
strncpy(history[HISTORY_SIZE - 1], cmd, CMD_BUF_SIZE - 1);  // add new at end
```

---

## 7. Concepts Glossary

| Term | Meaning |
|------|---------|
| **Bare metal** | Running directly on hardware, no OS underneath |
| **Protected mode** | 32-bit CPU mode with memory protection and full 4GB addressing |
| **Real mode** | 16-bit legacy CPU mode BIOS uses (only 1MB address space) |
| **Segment** | A region of memory defined by a GDT descriptor |
| **Flat memory model** | All segments span full 4GB — addresses are just addresses |
| **Interrupt** | A signal that pauses the CPU and runs a handler |
| **Exception** | A CPU-generated interrupt triggered by an error (div by zero, etc.) |
| **IRQ** | Hardware Interrupt Request — from a device like keyboard or timer |
| **PIC** | Programmable Interrupt Controller — chip that routes hardware IRQs |
| **EOI** | End of Interrupt — signal you must send to PIC after handling an IRQ |
| **IDT** | Interrupt Descriptor Table — maps vector numbers to handler functions |
| **GDT** | Global Descriptor Table — defines memory segments for the CPU |
| **Selector** | Index into the GDT, stored in a segment register |
| **Scancode** | Number sent by keyboard for each physical key |
| **VGA text mode** | Hardware mode where 0xB8000 maps to screen character cells |
| **Multiboot** | Standard letting GRUB identify and load kernels |
| **Cross-compiler** | Compiler that runs on your OS but produces code for a different target |
| **Freestanding** | C compilation mode with no standard library or OS |
| **I/O port** | Hardware communication channel separate from regular RAM (x86-specific) |
| **null terminator** | The `'\0'` byte that marks the end of a C string |
| **IRET** | x86 instruction to return from an interrupt handler |
| **Far jump** | Jump that changes both address and code segment register (`cs`) |
| **Volatile** | C keyword: variable may change outside normal program flow — don't cache it |
| **Packed struct** | Struct with no padding bytes between fields |
