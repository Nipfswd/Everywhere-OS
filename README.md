# Everywhere OS

Everywhere OS is an expirimental operating with a simple goal -- do things better then the systems we use today. It's still early, but the kernel builds, boots with grub, and runs in qemu. More details and a proper website will come soon.

# Build Instructions

## Requirements

You'll need a few tools:

- `gcc`
- `ld`
- `nasm`
- `grub-mkrescue`
- `qemu-system-i386`
- `xorriso` (required by grub mkrescue on many systems)

On Debian // Ubuntu, for example:

```bash
sudo apt install gcc-multilib nasm grub-pc-bin xorisso qemu-system-i386
```

# How to Build

## 1. Build the kernel and ISO

Just run:
```code
make
```

That will:

- Assemble `entry.asm`
- compile `kernel.c`
- link them into `kernel.elf`
- Generate the ISO at `build/os.iso`

## Run in qEMU
```bash
make run
```

Or if you want to clean the build files:

```bash
make clean
```
