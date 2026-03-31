TARGET  := i686-elf
CC      := $(TARGET)-gcc
AS      := nasm
LD      := $(TARGET)-gcc

CFLAGS  := -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
           -fno-stack-protector -fno-builtin -nostdlib -Ikernel
ASFLAGS := -f elf32
LDFLAGS := -m32 -ffreestanding -O2 -nostdlib -lgcc

KERNEL_SRC_C := \
    kernel/kernel.c   \
    kernel/vga.c      \
    kernel/string.c   \
    kernel/gdt.c      \
    kernel/idt.c      \
    kernel/keyboard.c

KERNEL_SRC_ASM := \
    kernel/boot.asm  \
    kernel/gdt.asm   \
    kernel/isr.asm

OBJDIR  := obj
OBJ_C   := $(patsubst kernel/%.c,   $(OBJDIR)/%.o,     $(KERNEL_SRC_C))
OBJ_ASM := $(patsubst kernel/%.asm, $(OBJDIR)/%.asm.o, $(KERNEL_SRC_ASM))
OBJS    := $(OBJ_ASM) $(OBJ_C)

ISO_DIR  := isodir
GRUB_CFG := $(ISO_DIR)/boot/grub/grub.cfg
KERNEL   := $(ISO_DIR)/boot/silentkernel.bin
ISO      := SilentKernel.iso

.PHONY: all clean run

all: $(ISO)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: kernel/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.asm.o: kernel/%.asm | $(OBJDIR)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL): $(OBJS) linker.ld
	mkdir -p $(ISO_DIR)/boot/grub
	$(LD) $(LDFLAGS) -T linker.ld -o $@ $(OBJS)
	i686-elf-grub-file --is-x86-multiboot $@ && echo "Multiboot check OK"

$(GRUB_CFG): grub.cfg
	mkdir -p $(ISO_DIR)/boot/grub
	cp grub.cfg $@

$(ISO): $(KERNEL) $(GRUB_CFG)
	i686-elf-grub-mkrescue -o $(ISO) $(ISO_DIR)
	@echo ""
	@echo "ISO ready: $(ISO)"

clean:
	rm -rf $(OBJDIR) $(ISO_DIR)/boot/silentkernel.bin $(ISO)

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO)
