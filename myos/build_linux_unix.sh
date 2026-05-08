#!/bin/bash
# ============================================================================
# MyOS - Linux/WSL Build Script
# Compiles, links, and builds a 100% original bootable raw disk image.
# ============================================================================
set -e

cd "$(dirname "$0")"

echo "=== Staging clean build ==="
rm -f src/*.o src/*.bin kernel.elf kernel.bin myos_disk.img

echo "=== Compiling Assembly Boot Sectors ==="
nasm -f bin src/boot1.asm -o src/boot1.bin
nasm -f bin src/boot2.asm -o src/boot2.bin

echo "=== Compiling Kernel Assembly Stubs ==="
gcc -m32 -c src/boot.S -o src/boot.o
gcc -m32 -c src/isr.S -o src/isr.o

echo "=== Compiling C Sources ==="
C_FILES=(
    src/gdt.c
    src/kernel.c
    src/string.c
    src/gui.c
    src/timer.c
    src/keyboard.c
    src/vga.c
    src/idt.c
    src/font.c
    src/shell.c
    src/mouse.c
    src/pmm.c
    src/framebuffer.c
    src/ata.c
    src/myfs.c
    src/compiler.c
)

for f in "${C_FILES[@]}"; do
    echo "  [CC] $f"
    gcc -std=c99 -ffreestanding -O2 -Wall -Wextra \
        -fno-exceptions -fno-stack-protector -fno-pie \
        -nostdinc -Isrc -fno-builtin -march=i686 -m32 \
        -c "$f" -o "${f%.c}.o"
done

echo "=== Linking ELF Kernel ==="
# Link strictly under i386 ELF target respecting linker.ld
ld -m elf_i386 -T linker.ld -o kernel.elf \
    src/boot.o \
    src/isr.o \
    src/gdt.o \
    src/kernel.o \
    src/string.o \
    src/gui.o \
    src/timer.o \
    src/keyboard.o \
    src/vga.o \
    src/idt.o \
    src/font.o \
    src/shell.o \
    src/mouse.o \
    src/pmm.o \
    src/framebuffer.o \
    src/ata.o \
    src/myfs.o \
    src/compiler.o

echo "=== Extracting Flat Binary Kernel ==="
objcopy -O binary kernel.elf kernel.bin

echo "=== Pre-allocating Aligned Raw Disk Image (20MB) ==="
# 40000 sectors = exactly 20480000 bytes (aligned to 512 bytes)
dd if=/dev/zero of=myos_disk.img bs=512 count=40000

echo "=== Creating Raw Bootable Disk Image ==="
# Place Stage 1 at Sector 0 (offset 0)
dd if=src/boot1.bin of=myos_disk.img bs=512 count=1 seek=0 conv=notrunc
# Place Stage 2 at Sector 1 (offset 512 bytes)
dd if=src/boot2.bin of=myos_disk.img bs=512 count=1 seek=1 conv=notrunc
# Place C Kernel at Sector 2 (offset 1024 bytes)
dd if=kernel.bin of=myos_disk.img bs=512 seek=2 conv=notrunc

echo "=== MyOS Build Successful! ==="
ls -lh myos_disk.img kernel.elf
