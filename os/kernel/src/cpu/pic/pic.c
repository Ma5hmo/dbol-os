#include "pic.h"
#include "util/io/io.h"

void init_pic(uint8_t irq_offset1, uint8_t irq_offset2)
{
    uint8_t irq_mask_1 = io_in_byte(PIC1_DATA);
    io_wait();
    uint8_t irq_mask_2 = io_in_byte(PIC2_DATA);
    io_wait();
    
    // ICW1 - initialize and wait for IC4 (so we can set 80x86 mode on)
    uint8_t icw1 = PIC_ICW1_MASK_INIT | PIC_ICW1_MASK_IC4;
    io_out_byte(PIC1_CMD, icw1);
    io_wait();
    io_out_byte(PIC2_CMD, icw1);
    io_wait();

    // ICW2 - tell the PICs which offset they have control of
    io_out_byte(PIC1_DATA, irq_offset1);
    io_wait();
    io_out_byte(PIC2_DATA, irq_offset2);
    io_wait();

    // ICW3 - tell the PICs which IRQ connects them (IRQ2)
    io_out_byte(PIC1_DATA, 0b00000100); // PIC1 (master) read a bit mask (each bit is an IRQ index)
    io_wait();
    io_out_byte(PIC2_DATA, 2); // PIC2 (slave) reads a single numeric value
    io_wait();

    // ICW4 - just set up 80x86 mode
    io_out_byte(PIC1_DATA, PIC_ICW4_MASK_8086);
    io_wait();
    io_out_byte(PIC2_DATA, PIC_ICW4_MASK_8086);
    io_wait();

    // Restore masks
    io_out_byte(PIC1_DATA, irq_mask_1);
    io_wait();
    io_out_byte(PIC2_DATA, irq_mask_2);
    io_wait();
}

void pic_toggle_irq(uint8_t irq_number, bool toggle_on)
{
    uint8_t port = (irq_number > 7) ? PIC2_DATA : PIC1_DATA;
    uint8_t irq_mask = io_in_byte(port);
    io_wait();

    irq_number %= 8; // ensure it stays between 0-7
    if (toggle_on)
    {
        irq_mask &= ~(1 << irq_number);
    }
    else
    {
        irq_mask |= (1 << irq_number);
    }
    io_out_byte(port, irq_mask);
    io_wait();
}
