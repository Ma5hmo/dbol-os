#pragma once
#include <stdint.h>
#include <stdbool.h>

// ICW1 bit masks
#define PIC_ICW1_MASK_IC4  0x1   // Expect ICW 4 bit
#define PIC_ICW1_MASK_INIT 0x10  // Initialization Command

// ICW4 bit mask
#define PIC_ICW4_MASK_8086 0x1   // 80x86 mode

// PIC's registers' ports
#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1

// The interrupt index where the IRQS will be mapped to
#define PIC1_IRQ_INDEX 0x20
#define PIC2_IRQ_INDEX (PIC1_IRQ_INDEX + 0x8)

// End Of Interrupt command
#define PIC_EOI 0x20

// Initialize the 8259 PIC Microcontroller
void init_pic(uint8_t irq_offset1, uint8_t irq_offset2);
void pic_toggle_irq(uint8_t irq_number, bool toggle_on);
