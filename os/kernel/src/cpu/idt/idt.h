#pragma once
#include <stdint.h>
#include <stdbool.h>

#define IDT_SIZE 256

struct idt_entry
{
   uint16_t offset_low;     // offset bits 0..15
   uint16_t selector;       // a code segment selector in GDT or LDT
   uint8_t zero;            // unused, set to 0
   uint8_t type_attributes; // gate type, dpl, and p fields
   uint16_t offset_high;    // offset bits 16..31
} __attribute__((packed));

struct idt_pointer
{
   uint16_t limit;
   uint32_t offset;
} __attribute__((packed));

void enable_interrupts();
void disable_interrupts();

void idt_set_entry(uint8_t index, uint32_t handlerAddress, bool is_userspace);
void idt_init();
// Defined in int_handlers.asm
extern void* first_int_handlers[IDT_SIZE]; // an array of all the interrupt handlers to add to the IDT
