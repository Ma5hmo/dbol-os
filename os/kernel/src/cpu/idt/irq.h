#pragma once
#include <stdint.h>

enum IRQ_NUMBERS
{
    PIT_IRQ,
    KEYBOARD_IRQ,
    CASCADE_IRQ,
    COM2_IRQ,
    COM1_IRQ,
    LPT2_IRQ,
    FLOPPY_IRQ,
    LPT1_IRQ,
    CMOS_IRQ,
    PERIPH1_IRQ,
    PERIPH2_IRQ,
    PERIPH3_IRQ,
    MOUSE_IRQ,
    FPU_IRQ,
    PRIMARY_ATA_IRQ,
    SECONDARY_ATA_IRQ
};

void irq_exit(uint32_t interrupt_number);
void init_irqs();
