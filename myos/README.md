# MyOS - Bare-Metal x86 Operating System & GUI Desktop
Designed and Engineered by **Shreearu Bisoi**

MyOS is an advanced, ultra-lightweight, 100% original operating system built from scratch for the x86 (i686) architecture. It features a custom legacy BIOS bootloader stack, a high-performance 32-bit graphical window compositor, a custom flat file system, and a comprehensive set of built-in GUI applications and CLI Unix-like utilities.

---

## 🚀 Key Features

### 1. Custom 16/32-bit Legacy BIOS Boot Stack
- **Stage 1 MBR Bootloader (`boot1.asm`)**: Loaded at physical address `0x7C00` in Real Mode. Relocates to and loads Stage 2 and the C Kernel sector-by-sector using robust BIOS CHS routines.
- **Stage 2 Branded Boot Menu (`boot2.asm`)**: 1024-byte interactive boot menu at `0x8000` with ASCII branding:
  - **[1] Standard GUI Mode**: Configures VESA VBE `1024x768x32bpp` graphics mode, populates the Mock Multiboot Info structure, and launches the desktop.
  - **[2] Safe Mode**: Skips graphics initialization, flags the kernel as text-mode-only, and boots into a fast, VGA 80x25 text-mode console shell.
  - Switches to Protected Mode (32-bit) with a temporary GDT, copies the kernel to the 1MB physical boundary, and jumps to the kernel entry point.

### 2. High-Performance Core Kernel
- **Segmented Memory**: Clean GDT setup mapping flat code, data, and stack segments.Remapped PIC and interrupt handlers (IDTs) for flawless hardware execution.
- **Memory Management**: High-speed bitmap-based physical frame allocator (`pmm.c`) with page-aligned structures.
- **Optimized 32-bit Drivers**: Fast aligned double-word copy (`memcpy`) and fill (`memset`) routines in `string.c` for high-rate memory transfers.
- **Storage Subsystem (`ata.c`, `myfs.c`)**: Port-based IDE PIO controller with custom flat file system (MyFS). Features sector-by-sector reading/writing with alignment safety and soft-delete index structures.

### 3. Wayland-Inspired Graphical Compositor & GUI Applications (`gui.c`)
- **Desktop Compositor**: Aligned linear double-buffered graphics engine drawing desktop elements, floating panels, active windows, mouse cursor overlays, and smooth background gradients.
- **Start Menu Launcher**: Interactive vertical start menu containing app links, system summaries, and nested reboot, shutdown, or exit-to-shell controls.
- **Right-Click Context Menu**: floating desktop context panel providing fast shortcuts for launching terminals, managing directories, viewing system logs, or reloading configurations.
- **Clickable Desktop Icons**: Clickable, styled shortcut icons (Terminal, File Manager, Paint, Calculator, Hex Editor, Text Editor) with highlight selections and double-click launching.
- **Built-in Bare-Metal Desktop Applications**:
  1. **Terminal**: Full interactive command shell window.
  2. **File Manager**: Scrollable list of active MyFS files. Supports row highlighting, dynamic file count, single-click deletion, and file reading via a popup content viewer.
  3. **Text Editor (Notepad)**: Text editing workspace with keyboard text buffering, navigation cursor, name input field, and save routine to write directly to disk sectors (by pressing `F2`).
  4. **Hex Editor**: Read-only binary sector explorer. Reads active disk data and renders a traditional hex block side-by-side with ASCII representations.
  5. **Paint**: Click-to-draw pixel paint canvas.
  6. **Calculator**: Interactive math calculator.
  7. **System Info & About**: Displays system hardware statistics, screen modes, active memory, and developer credits.

### 4. Unix-Style Command Shell (`shell.c`)
Includes standard Unix commands in both the terminal window and the safe-mode text shell:
- `uname` / `whoami` / `hostname` — System info, user privilege level (`root@myos`), and machine hostname.
- `date` — Simulated session-based system uptime clock.
- `ps` — Prints active execution threads (kernel idle loop, command shell task, compositor manager).
- `neofetch` — Premium system logo and hardware/display statistics summary.
- `ls`, `cat`, `touch`, `rm` — Storage commands matching filesystem directory listing, file printing, creation, and deletion.

---

## 🛠️ Build and Verification Instructions

### Requirements
Ensure your Linux host or Windows Subsystem for Linux (WSL) has the standard build dependencies:
```bash
sudo apt update
sudo apt install gcc gcc-multilib nasm make grub-common grub-pc-bin xorriso mtools
```

### 1. Compile the Bare-Metal Disk Image
Run the comprehensive build script from your terminal:
```bash
wsl bash ./build_linux.sh
```
This script will:
- Clean old compilation artifacts.
- Compile Stage 1 and Stage 2 assembly bootloaders via NASM.
- Compile all kernel assembly stubs and C sources with optimized freestanding flags.
- Link the final ELF kernel and extract the raw flat kernel binary (`kernel.bin`).
- Initialize a 20MB raw disk image (`myos_disk.img`) and write Stage 1, Stage 2 (Boot Menu), and the C Kernel directly to their reserved LBA disk sectors.

### 2. Convert to VirtualBox VDI & Run
To run the OS inside Oracle VirtualBox, convert the raw image to a dynamic VDI file:
```powershell
# Remove old VDI if present
if (Test-Path myos_disk.vdi) { Remove-Item -Force myos_disk.vdi }

# Convert raw image to VDI
& "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe" convertfromraw myos_disk.img myos_disk.vdi --format VDI
```

Create a new VirtualBox VM with the following settings:
- **Operating System**: Other / Unknown (32-bit).
- **Base Memory**: 64 MB RAM (or more).
- **Storage Controller**: IDE Controller. Add Hard Disk -> Select `myos_disk.vdi`.
- Boot the VM to experience the branded boot menu and launch the GUI!

---

## 📁 Repository Structure

```
myos/
├── build_linux.sh      # WSL/Linux main build script
├── build_linux_unix.sh # Unix line endings alternative build script
├── linker.ld           # Direct 32-bit physical alignment linker script
├── README.md           # Documentation and manuals
└── src/
    ├── boot1.asm       # Stage 1 MBR Loader (Real Mode, Sector 0)
    ├── boot2.asm       # Stage 2 Custom Boot Menu (Real Mode, Sectors 1-2)
    ├── boot.S          # Protected Mode C-main loader stub
    ├── isr.asm         # CPU Exception & IRQ assembly handler routines
    ├── gdt.c           # Flat segment mapping descriptor table
    ├── idt.c           # Remapped PIC interrupt vector handlers
    ├── pmm.c           # Bitmap physical frame memory manager
    ├── ata.c           # IDE PIO sector read/write device driver
    ├── myfs.c          # Custom flat storage filesystem & sector allocation
    ├── framebuffer.c   # VESA linear framebuffer setup & color blending
    ├── font.c          # 8x16 monospace system font bitmaps
    ├── keyboard.c      # PS/2 scancode translator & keyboard buffer
    ├── mouse.c         # PS/2 mouse coordinate bounds manager
    ├── timer.c         # PIT channel 0 system clock ticks (100Hz)
    ├── shell.c         # Text command execution & console drivers
    └── gui.c           # Compositor, window routines, app launchers
```

---

## 📄 License
This project is completely original bare-metal software. Made and engineered by **Shreearu Bisoi**. All rights reserved.

