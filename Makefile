# Makefile for Everywhere OS Kernel -- grub added

CC      = gcc
LD      = ld
NASM    = nasm

CFLAGS  = -c -ffreestanding -fno-builtin -fno-stack-protector -nostdlib \
          -fno-pic -fno-pie \
          -m32 -Wall -Wextra \
          -I./base/kernel/inc \
          -I./public/sdk/inc

LDFLAGS = -m elf_i386 -T kernel.ld
ASFLAGS = -f elf32

BUILD = build
ISO   = iso

KERNEL_DIR = base/kernel
MM_DIR     = base/kernel/mm

ENTRY_SRC = entry.asm
ENTRY_OBJ = $(BUILD)/entry.o

ISR_SRC = $(KERNEL_DIR)/isr.asm
ISR_OBJ = $(BUILD)/isr.o

KERNEL_SRCS = \
    $(KERNEL_DIR)/kernel.c  \
    $(KERNEL_DIR)/io.c      \
    $(KERNEL_DIR)/video.c   \
    $(KERNEL_DIR)/string.c  \
    $(KERNEL_DIR)/fs.c      \
    $(KERNEL_DIR)/shell.c   \
    $(KERNEL_DIR)/snake.c   \
    $(KERNEL_DIR)/box.c     \
    $(KERNEL_DIR)/gdt.c     \
    $(KERNEL_DIR)/idt.c     \
    $(KERNEL_DIR)/paging.c

MM_SRCS = \
    $(MM_DIR)/mminit.c   \
    $(MM_DIR)/pfnlist.c  \
    $(MM_DIR)/allocpag.c \
    $(MM_DIR)/allocvm.c  \
    $(MM_DIR)/freevm.c   \
    $(MM_DIR)/vadtree.c  \
    $(MM_DIR)/mmfault.c  \
    $(MM_DIR)/protect.c  \
    $(MM_DIR)/queryvm.c  \
    $(MM_DIR)/mmsup.c

KERNEL_OBJS = $(patsubst $(KERNEL_DIR)/%.c, $(BUILD)/%.o,    $(KERNEL_SRCS))
MM_OBJS     = $(patsubst $(MM_DIR)/%.c,     $(BUILD)/mm_%.o, $(MM_SRCS))

KERNEL_ELF = $(BUILD)/kernel.elf
OS_ISO     = $(BUILD)/os.iso

.PHONY: all clean run

$(shell mkdir -p $(BUILD))
$(shell mkdir -p $(ISO)/boot/grub)

all: $(OS_ISO)

$(ENTRY_OBJ): $(ENTRY_SRC)
	$(NASM) $(ASFLAGS) $< -o $@

$(ISR_OBJ): $(ISR_SRC)
	$(NASM) $(ASFLAGS) $< -o $@

$(BUILD)/%.o: $(KERNEL_DIR)/%.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/mm_%.o: $(MM_DIR)/%.c
	$(CC) $(CFLAGS) $< -o $@

$(KERNEL_ELF): $(ENTRY_OBJ) $(ISR_OBJ) $(KERNEL_OBJS) $(MM_OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

$(ISO)/boot/kernel.elf: $(KERNEL_ELF)
	cp $< $@

$(ISO)/boot/grub/grub.cfg:
	echo 'set timeout=0' > $@
	echo 'set default=0' >> $@
	echo '' >> $@
	echo 'menuentry "Everywhere OS" {' >> $@
	echo '    multiboot /boot/kernel.elf' >> $@
	echo '    boot' >> $@
	echo '}' >> $@

$(OS_ISO): $(ISO)/boot/kernel.elf $(ISO)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO)

run: $(OS_ISO)
	qemu-system-i386 -cdrom $(OS_ISO)

clean:
	rm -rf $(BUILD) $(ISO)