#include "gdt.h"

struct gdt_entry gdt_table[GDT_SIZE] = {0};
struct gdt_ptr info = {0};
struct tss_entry_t tss = {0};

void gdt_init() {
    gdt_table[0] = (struct gdt_entry){0};
    gdt_fill_entry(1, 0, 0xFFFFF, true, 0); // Kernel code segment
    gdt_fill_entry(2, 0, 0xFFFFF, false, 0); // Kernel data segment
    gdt_fill_entry(3, 0, 0xFFFFF, true, 3); // User code segment
    gdt_fill_entry(4, 0, 0xFFFFF, false, 3); // User data segment

    tss_fill_entry(0xC1100000, 0x10, &tss);
    gdt_fill_entry_as_tss(5, &tss);

    info.limit = sizeof(struct gdt_entry) * GDT_SIZE;
    info.base = (uint32_t)&gdt_table;
    
    load_gdt(&info, GDT_KERNEL_CODE_INDEX, GDT_KERNEL_DATA_INDEX);
    __asm__("mov $0x28, %%ax\n"
        "ltr %%ax" ::);
}

void tss_fill_esp0(uint32_t esp0)
{
    tss_fill_entry(esp0, 0x10, &tss);
    gdt_fill_entry_as_tss(5, &tss);
}


void tss_fill_entry(uint32_t esp0, uint32_t ss0, struct tss_entry_t* filled_tss)
{
    filled_tss->prev_tss = 0;
    filled_tss->esp0 = esp0;
    filled_tss->ss0 = ss0;
    filled_tss->esp1 = 0;
    filled_tss->ss1 = 0;
    filled_tss->esp2 = 0;
    filled_tss->ss2 = 0;
    filled_tss->cr3 = 0;
    filled_tss->eip = 0;
    filled_tss->eflags = 0;
    filled_tss->eax = 0;
    filled_tss->ecx = 0;
    filled_tss->edx = 0;
    filled_tss->ebx = 0;
    filled_tss->esp = 0;
    filled_tss->ebp = 0;
    filled_tss->esi = 0;
    filled_tss->edi = 0;
    filled_tss->es = 0;
    filled_tss->cs = 0;
    filled_tss->ss = 0;
    filled_tss->ds = 0;
    filled_tss->fs = 0;
    filled_tss->gs = 0;
    filled_tss->ldt = 0;
    filled_tss->trap = 0;
    filled_tss->iomap_base = 0;
}

void gdt_fill_entry(int index, uint32_t base, uint32_t limit, bool is_executable, uint8_t privilege_level) {
    gdt_table[index].limit_low = (limit & 0xFFFF); // 0 - 15
    gdt_table[index].limit_high = ((limit >> 16) & 0x0F);

    gdt_table[index].base_low = (base & 0xFFFF); // 16 - 31
    gdt_table[index].base_mid = (base >> 16) & 0xFF; // 32 - 39
    gdt_table[index].base_high = (base >> 24) & 0xFF; // 56 - 63

    gdt_table[index].accessed = 0;
    gdt_table[index].readable_writeable = 1;
    gdt_table[index].direction = 0;
    gdt_table[index].executable = is_executable;
    gdt_table[index].segment_type = 1;
    gdt_table[index].privilege = privilege_level;
    gdt_table[index].present = 1;

    gdt_table[index].reserved = 0;
    gdt_table[index].segment_size = 1;
    gdt_table[index].granularity = 1;
}


void gdt_fill_entry_as_tss(int index, struct tss_entry_t *tss_entry)
{
    uint32_t tss_entry_size = sizeof(struct tss_entry_t);
    uint32_t tss_entry_address = (uint32_t)tss_entry;

    gdt_table[index].limit_low = tss_entry_size;
    gdt_table[index].base_low = tss_entry_address;
    gdt_table[index].base_mid = tss_entry_address >> 16;

    gdt_table[index].accessed = 1;
    gdt_table[index].readable_writeable = 0;
    gdt_table[index].direction = 0;
    gdt_table[index].executable = 1;
    gdt_table[index].segment_type = 0;
    gdt_table[index].privilege = 0;
    gdt_table[index].present = 1;

    gdt_table[index].limit_high = tss_entry_size >> 16;
    gdt_table[index].reserved = 0;
    gdt_table[index].segment_size = 0;
    gdt_table[index].granularity = 0;

    gdt_table[index].base_high = tss_entry_address >> 24;
}