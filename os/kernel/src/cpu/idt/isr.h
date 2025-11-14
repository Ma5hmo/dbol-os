#pragma once
#include <stdint.h>
#include <stdbool.h>

// Pushed by first ISRs before calling the wrapper
typedef struct int_registers
{
   uint32_t ds;
   uint32_t edi, esi, ebp, kern_esp, ebx, edx, ecx, eax;
   uint32_t interrupt, error;
   uint32_t eip, cs, eflags, esp, ss;
} __attribute__((packed)) int_registers;

typedef void(*isr_handler)(int_registers*);

extern void isr_wrapper(int_registers* regs) __attribute__((cdecl));
bool register_isr_handler(uint32_t index, isr_handler handler);
