#pragma once

#include "../idt/isr.h"

#define CHANNEL0_PORT 0x40
#define MODE_COMMAND_REGISTER 0x43

#define FREQ_HZ 1193182
#define TARGET_FREQ_HZ 1000 // This will mean that every 1/1000 seconds will be an update

void pit_init();

static void timer_irq(int_registers* regs);

uint32_t get_system_time();
uint32_t get_expected_clock_fraction();
uint16_t get_reload_time();
