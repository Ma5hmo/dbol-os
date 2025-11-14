#include "pit.h"

#include "cpu/pic/pic.h"
#include "cpu/idt/irq.h"
#include "util/io/io.h"
#include "process/manager/process_manager.h"
#include "drivers/vga/vga.h"

uint32_t expected_clock_fraction = 0;
uint16_t reload_time = 0;

uint32_t system_time = 0;
uint32_t system_clock_fractions = 0;
uint32_t context_switch = 0;

void pit_init()
{
    io_out_byte(MODE_COMMAND_REGISTER, 0x36);
    // Bits 6 and 7 (00): Select channel 0.
    // Bits 4 and 5 (11): Access mode - lobyte/hibyte.
    // Bits 1 to 3 (011): Operating mode - Mode 3 (Square Wave Generator).
    // Bit 0 (0): Binary mode (16-bit binary).

    reload_time = get_reload_time();
    expected_clock_fraction = get_expected_clock_fraction(); // calculate the rounding error

    io_out_byte(CHANNEL0_PORT, reload_time & 0xFF);
    io_out_byte(CHANNEL0_PORT, reload_time >> 8);
    
    register_isr_handler(PIC1_IRQ_INDEX + PIT_IRQ, timer_irq);
    pic_toggle_irq(PIT_IRQ, true);
}


static void timer_irq(int_registers* regs)
{
    system_time++;
    context_switch++;
    system_clock_fractions += expected_clock_fraction;

    if (system_clock_fractions > TARGET_FREQ_HZ) // compensate for rounding error
    {
        system_time++;
        system_clock_fractions -= TARGET_FREQ_HZ;
    }

    irq_exit(PIT_IRQ);

    if (context_switch == 1)
    {
        context_switch = 0;
        //vga_printf("switch ");
        if (is_schduling())
            switch_process(regs);
    }
}

inline uint32_t get_system_time()
{
    return system_time;
}

inline uint32_t get_expected_clock_fraction()
{
    return FREQ_HZ - (reload_time * TARGET_FREQ_HZ);
}

inline uint16_t get_reload_time()
{
    return FREQ_HZ / TARGET_FREQ_HZ;
}