MULTIBOOT_MAGIC    equ 0x1BADB002
MULTIBOOT_ALIGN    equ 1 << 0
MULTIBOOT_MEMINFO  equ 1 << 1
MULTIBOOT_FLAGS    equ MULTIBOOT_ALIGN | MULTIBOOT_MEMINFO
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top
    push ebx
    push eax
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
