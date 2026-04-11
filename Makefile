# Makefile for Everywhere OS Kernel

CC      = gcc
LD      = ld
OBJCOPY = objcopy
NASM    = nasm

CFLAGS  = -c -fno-builtin -fno-stack-protector -m32 -Wall -Wextra -Iinclude -I./public/sdk/inc
LDFLAGS = -m elf_i386 -T kernel.ld
ASFLAGS_BIN = -f bin
ASFLAGS_ELF = -f elf32

BUILD = build

BOOTLOADER_SRC = bootloader.asm
BOOTLOADER_BIN = $(BUILD)/bootloader.bin

ENTRY_SRC = entry.asm
ENTRY_OBJ = $(BUILD)/entry.o

KERNEL_SRC = kernel.c
KERNEL_OBJ = $(BUILD)/kernel.o

KERNEL_ELF = $(BUILD)/kernel.elf

OS_IMAGE = $(BUILD)/os.img

.PHONY: all clean run help

# This line must NOT be indented
$(shell mkdir -p $(BUILD))

all: $(OS_IMAGE)

$(BOOTLOADER_BIN): $(BOOTLOADER_SRC)
	$(NASM) $(ASFLAGS_BIN) $(BOOTLOADER_SRC) -o $(BOOTLOADER_BIN)

$(ENTRY_OBJ): $(ENTRY_SRC)
	$(NASM) $(ASFLAGS_ELF) $(ENTRY_SRC) -o $(ENTRY_OBJ)

$(KERNEL_OBJ): $(KERNEL_SRC)
	$(CC) $(CFLAGS) -c $(KERNEL_SRC) -o $(KERNEL_OBJ)

$(KERNEL_ELF): $(ENTRY_OBJ) $(KERNEL_OBJ)
	$(LD) $(LDFLAGS) $(ENTRY_OBJ) $(KERNEL_OBJ) -o $(KERNEL_ELF)

$(OS_IMAGE): $(BOOTLOADER_BIN) $(KERNEL_ELF)
	cat $(BOOTLOADER_BIN) $(KERNEL_ELF) > $(OS_IMAGE)

run: $(KERNEL_ELF)
	qemu-system-i386 -kernel $(KERNEL_ELF) -m 256M

clean:
	rm -rf $(BUILD)

help:
	@echo "Everywhere OS Kernel Makefile"
	@echo "Targets: all, run, clean, help"
