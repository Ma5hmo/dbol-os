#include "isr.h"
#include "idt.h"
#include "cpu/pic/pic.h"
#include "drivers/vga/vga.h"
#include "process/manager/process_manager.h"
#include "util/io/io.h"

isr_handler handlers[IDT_SIZE] = {NULL};
const char *const exception_messages[] = {
    "Divide by zero error",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception ",
    "",
    "",
    "",
    "",
    "",
    "",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    ""};

void isr_wrapper(int_registers *regs)
{
    if (handlers[regs->interrupt] != NULL)
    {
        handlers[regs->interrupt](regs);
    }
    else if (regs->interrupt >= PIC1_IRQ_INDEX)
    {
        vga_printf("Unhandled interrupt %d\n", regs->interrupt);
    }
    else
    {
        // indexes 0-31 are CPU exceptions
        if (regs->interrupt == 14) // if page fault, close that probably caused it
         {  
            vga_printf("Segmentation fault");
            exit_current_process();
        }

        panic_screen(exception_messages[regs->interrupt]);
    }
}

bool register_isr_handler(uint32_t index, isr_handler handler)
{
    if (index >= IDT_SIZE) return false;
    handlers[index] = handler;
    return true;
}
