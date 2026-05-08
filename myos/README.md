# MyOS - Low Resource Operating System with GUI

A custom operating system built from scratch, targeting x86 (i686) with a graphical desktop environment. Designed to run in VMware and QEMU virtual machines with minimal resources.

## Features

- **Multiboot-compliant kernel** — boots via GRUB2
- **Protected mode (32-bit)** with GDT, IDT, and PIC
- **Physical memory manager** — bitmap-based frame allocator
- **PS/2 keyboard & mouse drivers** — full scancode translation
- **VESA framebuffer graphics** — 1024×768×32bpp with double buffering
- **Text shell** — built-in commands (help, clear, mem, uptime, gui, reboot)
- **GUI desktop** — Wayland-inspired compositor with:
  - Gradient desktop background
  - Window management (drag, focus, close)
  - Taskbar with window list and clock
  - Demo windows (About, System Info, interactive Terminal)
  - Mouse cursor rendering

## Requirements

Build on **Linux** or **WSL** (Windows Subsystem for Linux):

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install gcc gcc-multilib nasm make grub-common grub-pc-bin xorriso mtools

# Optional: QEMU for testing
sudo apt install qemu-system-x86
```

## Building

```bash
cd myos
make            # Build kernel and create bootable ISO
make run        # Build and launch in QEMU
make run-debug  # Launch with GDB stub on port 1234
make clean      # Remove build artifacts
```

## Running in VMware

1. Build the ISO: `make`
2. Create a new VM in VMware:
   - **Guest OS**: Other → Other (32-bit)
   - **RAM**: 256–512 MB
   - **CPU**: 1 vCPU
   - **Disk**: 1 GB (or skip disk, boot from CD only)
   - **CD/DVD**: Use ISO image → select `myos.iso`
3. Boot the VM — you'll see the GRUB menu, then MyOS boots

## Usage

After boot, you'll see a text shell:
```
myos:~$ help        # Show available commands
myos:~$ mem         # Display memory information
myos:~$ gui         # Launch graphical desktop
```

In GUI mode:
- **Drag windows** by their title bar
- **Close windows** with the red X button
- **Type in Terminal** window for an embedded shell
- **Press Escape** to return to text mode

## Architecture

```
┌─────────────────────────────────────────┐
│              GUI Desktop                │
│  ┌─────────┐  ┌──────────┐  ┌────────┐ │
│  │  About  │  │ Sys Info │  │Terminal│ │
│  └─────────┘  └──────────┘  └────────┘ │
├─────────────────────────────────────────┤
│         Compositor / Window Mgr         │
├─────────────────────────────────────────┤
│     Framebuffer  │ Keyboard │  Mouse    │
├─────────────────────────────────────────┤
│  PMM  │  Timer  │   IDT/PIC   │  GDT   │
├─────────────────────────────────────────┤
│          GRUB2 Multiboot Loader         │
└─────────────────────────────────────────┘
```

## Project Structure

```
myos/
├── Makefile           # Build system
├── linker.ld          # Kernel linker script
├── iso/boot/grub/
│   └── grub.cfg       # GRUB bootloader config
└── src/
    ├── boot.asm       # Multiboot entry point (NASM)
    ├── isr.asm        # ISR/IRQ assembly stubs
    ├── kernel.h       # Master header (types, declarations)
    ├── kernel.c       # kmain + initialization
    ├── string.c       # String/memory utilities
    ├── gdt.c          # Global Descriptor Table
    ├── idt.c          # IDT + ISR handlers + PIC
    ├── timer.c        # PIT timer (100 Hz)
    ├── pmm.c          # Physical memory manager
    ├── vga.c          # VGA text mode driver
    ├── keyboard.c     # PS/2 keyboard driver
    ├── mouse.c        # PS/2 mouse driver
    ├── framebuffer.c  # VESA framebuffer graphics
    ├── font.c         # 8×16 bitmap font data
    ├── shell.c        # Text command shell
    └── gui.c          # Compositor + window mgr + desktop
```

## License

All code is original. Licensed under BSD/MIT.
No Linux, Windows, or GPL code is used.
