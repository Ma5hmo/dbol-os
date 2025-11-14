#include "keyboard.h"
#include "util/io/io.h"
#include "../../cpu/idt/irq.h"
#include "../../cpu/pic/pic.h"
#include "../vga/vga.h"
#include "process/manager/process_manager.h"
#include "terminal/terminal_manager.h"

unsigned char key_map[128] =
    {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8',    /* 9 */
        '9', '0', '-', '=', '\b',                         /* Backspace */
        '\t',                                             /* Tab */
        'q', 'w', 'e', 'r',                               /* 19 */
        't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',     /* Enter key */
        0,                                                /* 29   - Control */
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 39 */
        '\'', '`', 0,                                     /* Left shift */
        '\\', 'z', 'x', 'c', 'v', 'b', 'n',               /* 49 */
        'm', ',', '.', '/', 0,                            /* Right shift */
        '*',
        0,   /* Alt */
        ' ', /* Space bar */
        0,   /* Caps lock */
        0,   /* 59 - F1 key ... > */
        0, 0, 0, 0, 0, 0, 0, 0,
        0, /* < ... F10 */
        0, /* 69 - Num lock*/
        0, /* Scroll Lock */
        0, /* Home key */
        0, /* Up Arrow */
        0, /* Page Up */
        '-',
        0, /* Left Arrow */
        0,
        0, /* Right Arrow */
        '+',
        0, /* 79 - End key*/
        0, /* Down Arrow */
        0, /* Page Down */
        0, /* Insert Key */
        0, /* Delete Key */
        0, 0, 0,
        0, /* F11 Key */
        0, /* F12 Key */
        0, /* All other keys are undefined */
};

unsigned char key_map_shift[128] =
    {
        0, 27, '!', '@', '#', '$', '%', '^', '&', '*',    /* 9 */
        '(', ')', '_', '+', '\b',                         /* Backspace */
        '\t',                                             /* Tab */
        'Q', 'W', 'E', 'R',                               /* 19 */
        'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',     /* Enter key */
        0,                                                /* 29   - Control */
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', /* 39 */
        '\"', '~', 0,                                     /* Left shift */
        '|', 'Z', 'X', 'C', 'V', 'B', 'N',                /* 49 */
        'M', '<', '>', '?', 0,                            /* Right shift */
        '*',
        0,   /* Alt */
        ' ', /* Space bar */
        0,   /* Caps lock */
        0,   /* 59 - F1 key ... > */
        0, 0, 0, 0, 0, 0, 0, 0,
        0,   /* < ... F10 */
        0,   /* 69 - Num lock*/
        0,   /* Scroll Lock */
        '7', /* Home key */
        '8', /* Up Arrow */
        '9', /* Page Up */
        '-',
        '4', /* Left Arrow */
        '5',
        '6', /* Right Arrow */
        '+',
        '1', /* 79 - End key*/
        '2', /* Down Arrow */
        '3', /* Page Down */
        '0', /* Insert Key */
        ',', /* Delete Key */
        0, 0, 0,
        0, /* F11 Key */
        0, /* F12 Key */
        0, /* All other keys are undefined */
};

static keyboard_state_flags keyboard_state = {0};

static void keyboard_irq(int_registers *regs);
static void keyboard_handler(uint8_t scan_code);
static uint8_t get_scan_code();

void keyboard_init()
{
    pic_toggle_irq(KEYBOARD_IRQ, true);
    register_isr_handler(PIC1_IRQ_INDEX + KEYBOARD_IRQ, keyboard_irq);
}

static void keyboard_irq(int_registers *regs)
{
    keyboard_handler(get_scan_code());
    irq_exit(KEYBOARD_IRQ);
}

static void keyboard_handler(uint8_t scan_code)
{
    if (scan_code & 0x80)
    {
        switch (scan_code)
        {
        case 0xAA:
            keyboard_state.left_shift_pressed = 0;
            break;
        case 0xB6:
            keyboard_state.right_shift_pressed = 0;
            break;
        case 0x9D:
            keyboard_state.ctrl_pressed = 0;
            break;
        case 0xB8:
            keyboard_state.alt_pressed = 0;
            break;
        case 0x45:
            keyboard_state.num_lock_pressed = 0;
            break;
        case 0x3A:
            keyboard_state.caps_lock_pressed = 0;
            break;
        case 0x46:
            keyboard_state.scroll_lock_pressed = 0;
            break;
        }
    }
    else
    {
        switch (scan_code)
        {
        case 0x2A:
            keyboard_state.left_shift_pressed = 1;
            break;
        case 0x36:
            keyboard_state.right_shift_pressed = 1;
            break;
        case 0x1D:
            keyboard_state.ctrl_pressed = 1;
            break;
        case 0x38:
            keyboard_state.alt_pressed = 1;
            break;
        case 0x45:
            keyboard_state.num_lock_pressed = 1;
            break;
        case 0x3A:
            keyboard_state.caps_lock_pressed ^= 1;
            break;
        case 0x46:
            keyboard_state.scroll_lock_pressed = 1;
            break;
        default:
        {
            terminal_struct_t* active_terminal = get_active_terminal_struct();
            if (key_map[scan_code] == '\b')
            {
                if (active_terminal->input_len > 0)
                {
                    active_terminal->input_buf[--active_terminal->input_len] = '\0';
                    vga_putchar('\b');
                }
                break;
            }

            if ((keyboard_state.caps_lock_pressed 
                && (key_map[scan_code] >= 'a' && key_map[scan_code] <= 'z') 
                && !(keyboard_state.left_shift_pressed || keyboard_state.right_shift_pressed)) 
                || keyboard_state.left_shift_pressed || keyboard_state.right_shift_pressed)
            {
                active_terminal->input_buf[active_terminal->input_len++] = key_map_shift[scan_code];
                vga_putchar(key_map_shift[scan_code]);
            }
            else
            {
                active_terminal->input_buf[active_terminal->input_len++] = key_map[scan_code];
                vga_putchar(key_map[scan_code]);
            }

            if (key_map[scan_code] == '\n')
            {
                active_terminal->is_input_ready = true;
                wake_up_terminal_processes(get_active_terminal_id());
            }

            break;
        }
        }
    }
}

static uint8_t get_scan_code()
{
    return io_in_byte(0x60);
}