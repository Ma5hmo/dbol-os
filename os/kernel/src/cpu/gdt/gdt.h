#pragma once 

#include <stdint.h>
#include <stdbool.h>

#define GDT_SIZE 6
#define NULL_DESCRIPTOR 0, 0, 0, 0, 0
#define KERNEL_CODE_SEGMENT 1, 0, 0xFFFFF, 0x9A, 0xC
#define KERNEL_DATA_SEGMENT 2, 0, 0xFFFFF, 0x92, 0xC
#define USER_CODE_SEGMENT 3, 0, 0xFFFFF, 0xFA, 0xC
#define USER_DATA_SEGMENT 4, 0, 0xFFFFF, 0xF2, 0xC

// Describes a gdt segment
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;

    // Access byte
    bool accessed : 1;
    bool readable_writeable : 1; // for code segments - is readable, for data - is writeable
    bool direction : 1;
    bool executable : 1;
    bool segment_type : 1; // 0 -> TSS; 1 -> code or data segment
    uint8_t privilege : 2; // DPL
    bool present : 1; // always set to 1
    
    // Flags
    uint8_t limit_high : 4;
    bool reserved : 1;
    bool long_mode : 1; // is 64-bits?
    bool segment_size : 1; // 0 -> 16-bit protected mode; 1 -> 32-bit
    bool granularity : 1;

    uint8_t base_high;
}  __attribute__((packed));


struct __attribute__((packed)) tss_entry_t {
    uint32_t prev_tss;
    uint32_t esp0;      // Kernel stack pointer to load when changing to ring 0.
    uint32_t ss0;       // Kernel stack segment.
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
};

#define GDT_KERNEL_CODE_INDEX 1 * sizeof(struct gdt_entry)
#define GDT_KERNEL_DATA_INDEX 2 * sizeof(struct gdt_entry)
#define GDT_USER_CODE_INDEX 3 * sizeof(struct gdt_entry)
#define GDT_USER_DATA_INDEX 4 * sizeof(struct gdt_entry)

// Describes a pointer to a gdt table (passed into lgdt)
struct gdt_ptr {
    uint16_t limit;
    unsigned int base;
} __attribute__((packed));

void gdt_init();
void tss_fill_esp0(uint32_t esp0);
void tss_fill_entry(uint32_t esp0, uint32_t ss0, struct tss_entry_t* filled_tss);
void gdt_fill_entry(int index, uint32_t base, uint32_t limit, bool is_executable, uint8_t privilege_level);
void gdt_fill_entry_as_tss(int index, struct tss_entry_t *tss_entry);
extern void load_gdt(struct gdt_ptr* descriptor, uint16_t codeSegment, uint16_t dataSegment) __attribute__((cdecl));