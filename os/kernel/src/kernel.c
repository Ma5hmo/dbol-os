#include "drivers/vga/vga.h"
#include "cpu/gdt/gdt.h"
#include "cpu/idt/idt.h"
#include "cpu/pit/pit.h"
#include "memory/physical/multiboot.h"
#include "memory/physical/physical_memory_manager.h"
#include "memory/paging/paging.h"
#include "memory/heap/heap.h"
#include "drivers/harddisk/ata/ata.h"
#include "filesystem/fat/fat.h"
#include "drivers/keyboard/keyboard.h"
#include "process/manager/process_manager.h"
#include "process/syscalls/syscalls.h"
#include "terminal/terminal_manager.h"
#include "process/syscalls/handlers/time/time.h"

#include <fcntl.h>


extern void jump_usermode(process_registers_t *addr);

void print_logo() {
    vga_putstring_colored("____________       _ _____ _____ \n", VGA_COLOR_BLACK, VGA_COLOR_PINK);
    vga_putstring_colored("|  _  \\ ___ \\     | |  _  /  ___|\n", VGA_COLOR_BLACK, VGA_COLOR_PINK);
    vga_putstring_colored("| | | | |_/ / ___ | | | | \\ `--. \n", VGA_COLOR_BLACK, VGA_COLOR_PINK);
    vga_putstring_colored("| | | | ___ \\/ _ \\| | | | |`--. \\\n", VGA_COLOR_BLACK, VGA_COLOR_PINK);
    vga_putstring_colored("| |/ /| |_/ / (_) | \\ \\_/ /\\__/ /\n", VGA_COLOR_BLACK, VGA_COLOR_PINK);
    vga_putstring_colored("|___/ \\____/ \\___/|_|\\___/\\____/ \n", VGA_COLOR_BLACK, VGA_COLOR_PINK);
}




void kernel_physical_end(void);
void kmain(multiboot_info_t *mbi)
{
    asm ("cli");
    gdt_init();
    vga_init();

    idt_init();
    vga_putstring("IDT Initialized\n");
    ata_init();

    vga_putstring("ATA driver Initialized\n");
    if (pmm_init(mbi) != PMM_SUCCESS)
    {
        vga_printf("failed pmm init");
        return;
    }

    if (!heap_init())
    {
        vga_printf("failed heap init");
        return;
    }
    
    pit_init();

    fat_init();

    keyboard_init();
    syscall_init();

    proc_manager_init();
    set_active_terminal(create_terminal(1));

    vga_init();
    print_logo();

    create_process("/proc1", 0);
    create_process("/proc2", 0);

    asm ("sti");
    enable_processes();
    
    while (1){}
}