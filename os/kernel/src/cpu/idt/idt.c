#include <stdint.h>
#include "idt.h"
#include "irq.h"
#include "cpu/gdt/gdt.h"
#include "drivers/vga/vga.h"
#include "cpu/pic/pic.h"

struct idt_entry idt[IDT_SIZE];
struct idt_pointer idt_ptr;

void idt_set_entry(uint8_t index, uint32_t handlerAddress, bool is_userspace)
{
    idt[index].offset_low = handlerAddress & 0xFFFF;
    idt[index].selector = GDT_KERNEL_CODE_INDEX;
    idt[index].zero = 0;
    idt[index].type_attributes = 0x8E | (is_userspace? 0x60 : 0x0); // Interrupt gate (0x0E), Ring (0 | 3), Present
    idt[index].offset_high = (handlerAddress >> 16) & 0xFFFF;
}

void idt_init()
{
    for (int i = 0; i < IDT_SIZE; ++i)
    {
        if (i != 0x80)
            idt_set_entry(i, (uint32_t)first_int_handlers[i], false);
        else
            idt_set_entry(i, (uint32_t)first_int_handlers[i], true);
    }

    idt_ptr.limit = sizeof(struct idt_entry) * IDT_SIZE - 1;
    idt_ptr.offset = (uint32_t)&idt;
    __asm__ volatile(
        "lidt (%0)\n" ::"r"(&idt_ptr));
    // After setting up the PIC to not get a double fault:
    init_pic(PIC1_IRQ_INDEX, PIC2_IRQ_INDEX);
    init_irqs();
    enable_interrupts();
}

inline void enable_interrupts()
{
    __asm__("sti");
}

inline void disable_interrupts()
{
    __asm__("cli");
}