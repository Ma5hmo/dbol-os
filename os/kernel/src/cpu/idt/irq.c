#include "irq.h"
#include "isr.h"
#include "drivers/vga/vga.h"
#include "cpu/pic/pic.h"
#include "util/io/io.h"

inline void irq_exit(uint32_t interrupt_number)
{
    // Send an End Of Interrupt command to the PIC to acknowledge we are done
    if (interrupt_number >= PIC2_IRQ_INDEX)
    {
        io_out_byte(PIC2_CMD, PIC_EOI);
    }
    io_out_byte(PIC1_CMD, PIC_EOI);
}

void example_irq_handler(int_registers* regs)
{
    vga_printf("IRQ TRIGGERED %d\n", regs->interrupt - PIC1_IRQ_INDEX);
}

void init_irqs()
{
    // turn off all interrupts
    for (int i = 0; i < 16; ++i)
    {
        pic_toggle_irq(i, false);
    }

    // ... and the cascade interrupt to allow communication between the two PICs
    pic_toggle_irq(CASCADE_IRQ, true);
}

