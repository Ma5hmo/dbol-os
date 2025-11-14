# Compiler and assembler flags
CC = i386-elf-gcc
CFLAGS = -fno-stack-protector -ffreestanding -g -Ilib/src -Ios/kernel/src
AS = nasm
ASFLAGS = -f elf32
LD = i386-elf-ld
LDFLAGS = -m elf_i386 -T os/linker.ld

# Directories
KERNEL_SRCDIR = os/kernel/src
LIB_SRCDIR = lib/src
OBJDIR = obj
GRUBDIR = os/DBolOS
BINDIR = os/DBolOS/boot

# Source files
KERNEL_SOURCES = $(shell find $(KERNEL_SRCDIR) -name '*.c')
LIB_SOURCES = $(shell find $(LIB_SRCDIR) -name '*.c')
ASM_SOURCES = $(shell find $(KERNEL_SRCDIR) -name '*.asm')

# Object files
KERNEL_C_OBJECTS = $(patsubst $(KERNEL_SRCDIR)/%.c,$(OBJDIR)/kernel/%.o,$(KERNEL_SOURCES))
LIB_OBJECTS = $(patsubst $(LIB_SRCDIR)/%.c,$(OBJDIR)/lib/%.o,$(LIB_SOURCES))
ASM_OBJECTS = $(patsubst $(KERNEL_SRCDIR)/%.asm,$(OBJDIR)/kernel/%.o,$(ASM_SOURCES))

# Combined objects
ALL_OBJECTS = $(KERNEL_C_OBJECTS) $(LIB_OBJECTS) $(ASM_OBJECTS)

# Output files
OUTPUT_BIN = kernel.bin
OUTPUT_ELF = kernel.elf

# Target to create the kernel binary
all: $(OUTPUT_BIN).iso $(OUTPUT_ELF)

# Compile kernel C source files
$(OBJDIR)/kernel/%.o: $(KERNEL_SRCDIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile library C source files
$(OBJDIR)/lib/%.o: $(LIB_SRCDIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble the assembly files
$(OBJDIR)/kernel/%.o: $(KERNEL_SRCDIR)/%.asm
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# Link all object files to create the kernel binary
$(OUTPUT_BIN): $(ALL_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJECTS)

# Create the ELF file with debugging symbols
$(OUTPUT_ELF): $(ALL_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJECTS)

# Move the kernel binary to the boot directory
$(BINDIR)/$(OUTPUT_BIN): $(OUTPUT_BIN)
	mkdir -p $(BINDIR)
	mv $(OUTPUT_BIN) $(BINDIR)/$(OUTPUT_BIN)

# Create the ISO image using GRUB and move it to the main directory
$(OUTPUT_BIN).iso: $(BINDIR)/$(OUTPUT_BIN)
	grub-mkrescue -o kernel.iso $(GRUBDIR)

# Clean up the build files
clean:
	rm -f $(BINDIR)/*.bin $(BINDIR)/*.iso
	rm -rf $(OBJDIR)/*
	rm -f *.iso
	rm -f $(OUTPUT_ELF)

disk:
	rm fs.img
	qemu-img create -f raw fs.img 64M
	mkfs.fat -F 16 fs.img

.PHONY: all clean
