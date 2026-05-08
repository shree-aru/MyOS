# ============================================================================
# MyOS - Minimal Operating System with GUI
# Build System (MinGW on Windows)
# ============================================================================
# Uses MinGW GCC (i686) and GNU as (GAS syntax)
# No NASM or cross-compiler required
# ============================================================================

CC      = C:/MinGW/bin/gcc.exe
AS      = C:/MinGW/bin/gcc.exe
LD      = C:/MinGW/bin/ld.exe
OBJCOPY = C:/MinGW/bin/objcopy.exe
MAKE    = C:/MinGW/bin/mingw32-make.exe

CFLAGS  = -std=c99 -ffreestanding -O2 -Wall -Wextra \
          -fno-exceptions -fno-stack-protector -fno-pie \
          -nostdinc -Isrc -fno-builtin -march=i686 -m32 \
          -fno-leading-underscore
ASFLAGS = -c -m32
LDFLAGS = -T linker.ld -nostdlib --nmagic -m i386pe

# Source files
C_SOURCES   = $(wildcard src/*.c)
S_SOURCES   = src/boot.S src/isr.S

# Object files
C_OBJECTS   = $(C_SOURCES:.c=.o)
S_OBJECTS   = $(S_SOURCES:.S=.o)

# Boot object must come first for multiboot header placement
OBJECTS = src/boot.o src/isr.o $(filter-out src/boot.o src/isr.o, $(C_OBJECTS))

# Output files
KERNEL_PE  = kernel.pe
KERNEL     = kernel.elf

# ============================================================================
# Targets
# ============================================================================

.PHONY: all clean help kernel

all: $(KERNEL)
	copy /Y kernel.elf iso\boot\kernel.elf >nul
	@echo.
	@echo ============================================
	@echo   MyOS built successfully!
	@echo   Kernel: $(KERNEL)
	@echo ============================================

$(KERNEL): $(KERNEL_PE)
	$(OBJCOPY) -O elf32-i386 $< $@
	@echo [ELF]  $@

$(KERNEL_PE): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo [LINK] $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<
	@echo [CC]   $<

src/%.o: src/%.S
	$(CC) $(ASFLAGS) -o $@ $<
	@echo [AS]   $<

clean:
	del /Q src\*.o $(KERNEL_PE) $(KERNEL) 2>NUL

help:
	@echo MyOS Build System (MinGW)
	@echo   mingw32-make          - Build the kernel ELF
	@echo   mingw32-make clean    - Remove build artifacts
