OUTPUT_FORMAT(elf32-i386)

STARTUP_PHYSICAL_ADDRESS = 0x0000F000;
KERNEL_PHYSICAL_ADDRESS  = 0x00010000;
KERNEL_VIRTUAL_ADDRESS   = 0xC0000000;

ENTRY(_start)
SECTIONS
{

	. = KERNEL_PHYSICAL_ADDRESS;

    .multiboot ALIGN(4K):
	{
		*(.multiboot)
	}

    .note.gnu.build-id ALIGN(4K): {
        *(.note.gnu.build-id)
    }

    .boot ALIGN(4K):
    {
        *(.boot)
    }
    
    . += KERNEL_VIRTUAL_ADDRESS;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRTUAL_ADDRESS)
    {
        *(.text)
    }

    .rodata ALIGN(4K): AT(ADDR(.rodata) - KERNEL_VIRTUAL_ADDRESS)
    {
        *(.rodata)
    }

    .data ALIGN(4K): AT(ADDR(.data) - KERNEL_VIRTUAL_ADDRESS)
    {
        *(.data)
    }

    .bss ALIGN(4K): AT(ADDR(.bss) - KERNEL_VIRTUAL_ADDRESS)
    {
        *(COMMON)
        *(.bss)
    }

    _kernel_end = .;	
    kernel_physical_end = . - KERNEL_VIRTUAL_ADDRESS;
}   