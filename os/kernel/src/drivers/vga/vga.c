#include "vga.h"
#include "string.h"
#include "../../util/io/io.h"

vga_char *vga_buffer = (vga_char *)VGA_BASE_ADDR;

static const uint16_t screen_width = DEFAULT_VGA_WIDTH;
static const uint16_t screen_height = DEFAULT_VGA_HEIGHT;

uint16_t vga_curr_column = 0;
uint16_t vga_curr_row = 0;

void vga_init()
{
    vga_char emptyChar = (vga_char){' ', VGA_COLOR_WHITE};
    for (uint16_t row = 0; row < screen_height; ++row)
    {
        for (uint16_t col = 0; col < screen_width; ++col)
        {
            vga_buffer[row * DEFAULT_VGA_WIDTH + col] = emptyChar;
        }
    }
    vga_curr_column = 0;
    vga_curr_row = 0;
}

void vga_scroll()
{
    for (uint16_t row = 1; row < screen_height; ++row)
    {
        for (uint16_t col = 0; col < screen_width; ++col)
        {
            vga_buffer[(row - 1) * screen_width + col] = vga_buffer[row * screen_width + col];
        }
    }

    vga_char emptyChar = (vga_char){' ', VGA_COLOR_WHITE};

    for (uint16_t col = 0; col < screen_width; ++col)
    {
        vga_buffer[(screen_height - 1) * screen_width + col] = emptyChar;
    }
}

void vga_line_down()
{
    vga_curr_column = 0;

    if (vga_curr_row == screen_height - 1)
    {
        vga_scroll();
        return;
    }

    ++vga_curr_row;
}

void vga_putchar(unsigned char c)
{
    vga_putchar_colored(c, VGA_COLOR_BLACK, VGA_COLOR_WHITE);
}

void vga_del_char()
{
    if (vga_curr_column == 0) {
        // If we're at the start of a line, move up a line if possible
        if (vga_curr_row > 0) {
            vga_curr_row--;
            vga_curr_column = screen_width - 1;
        }
    } else {
        // Move cursor back one space
        vga_curr_column--;
    }

    // Clear the character at the new cursor position
    vga_char emptyChar = (vga_char){' ', VGA_COLOR_WHITE};
    vga_buffer[vga_curr_column + vga_curr_row * screen_width] = emptyChar;

    // Update the cursor position
    update_cursor();
}

void vga_putchar_colored(unsigned char c, enum VGA_COLOR bg, enum VGA_COLOR ch_color) {
    if (c == '\n')
    {
        vga_line_down();
        update_cursor();
        return;
    }

    if (c == '\b')
    {
        vga_del_char();
        return;
    }

    // check if need to go line down
    if (vga_curr_column == screen_width)
    {
        vga_line_down();
    }
    else if (vga_curr_row == screen_height)
    {
        vga_curr_column = 0;
        vga_scroll();
        --vga_curr_row;
    }

    vga_char printChar = (vga_char){c, ch_color | (bg << 4)};

    vga_buffer[vga_curr_column + vga_curr_row * screen_width] = printChar;
    ++vga_curr_column;
    update_cursor();
}

void vga_putstring(const unsigned char *s)
{
    vga_putstring_colored(s, VGA_COLOR_BLACK, VGA_COLOR_WHITE);
}

void vga_putstring_colored(const unsigned char *s, enum VGA_COLOR bg, enum VGA_COLOR ch_color) {
    while (*s)
    {
        vga_putchar_colored(*s, bg, ch_color);
        ++s;
    }
}

// `printf` implementation
void vga_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    char buffer[32]; // Temporary buffer for integer/string conversions

    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd': // Decimal integer
                    itoa(va_arg(args, int), buffer, 10);
                    vga_putstring(buffer);
                    break;
                case 'x': // Hexadecimal integer
                    itoa(va_arg(args, int), buffer, 16);
                    vga_putstring(buffer);
                    break;
                case 's': // String
                    vga_putstring(va_arg(args, char*));
                    break;
                case '%': // Escaped '%'
                    vga_putstring("%");
                    break;
                default: // Unknown specifier
                    vga_putstring("?");
                    break;
            }
        } else {
            // Print literal characters one at a time
            char literal[2] = {*format, '\0'};
            vga_putstring(literal);
        }
        format++;
    }

    va_end(args);
}


void panic_screen(const char *panic_message) {
    // Clear screen with red background
    for (int i = 0; i < 25; i++) {
        for (int j = 0; j < 80; j++) {
            vga_putstring_colored(" ", VGA_COLOR_RED, VGA_COLOR_WHITE);
        }
    }
    int logo_start_row = 5;
    int logo_start_col = (80 - 35) / 2;

    const char* logo_lines[] = {
        "____________       _ _____ _____ ",
        "|  _  \\ ___ \\     | |  _  /  ___|",
        "| | | | |_/ / ___ | | | | \\ `--. ",
        "| | | | ___ \\/ _ \\| | | | |`--. \\",
        "| |/ /| |_/ / (_) | \\ \\_/ /\\__/ /",
        "|___/ \\____/ \\___/|_|\\___/\\____/ "
    };

    // Draw logo
    for (int i = 0; i < 6; i++) {
        // Move cursor to specific position and draw logo line
        for (int j = 0; j < logo_start_col; j++) {
            vga_putstring_colored(" ", VGA_COLOR_RED, VGA_COLOR_WHITE);
        }
        vga_putstring_colored((unsigned char*)logo_lines[i], VGA_COLOR_RED, VGA_COLOR_WHITE);

        for (int j = 0; j < 80 - logo_start_col - strlen((logo_lines[i])); j++) {
            vga_putstring_colored(" ", VGA_COLOR_RED, VGA_COLOR_WHITE);
        }
    }

    // Position panic message
    int message_row = 15;
    int message_start_col = (80 - strlen(panic_message)) / 2;

    // Move cursor to message position
    for (int j = 0; j < message_row; j++) {
        for (int k = 0; k < 80; k++) {
            vga_putstring_colored(" ", VGA_COLOR_RED, VGA_COLOR_WHITE);
        }
    }

    // For message positioning, move cursor
    for (int j = 0; j < message_start_col; j++) {
        vga_putstring_colored(" ", VGA_COLOR_RED, VGA_COLOR_WHITE);
    }

    // Display panic message
    vga_putstring_colored((unsigned char*)panic_message, VGA_COLOR_RED, VGA_COLOR_WHITE);

    for (int j = 0; j < 80 - message_start_col - strlen(panic_message); j++) {
        vga_putstring_colored(" ", VGA_COLOR_RED, VGA_COLOR_WHITE);
    }

    // Optional: Halt or hang the system
    while(1) {
        // Typically, you'd use a CPU halt instruction here
        __asm__("cli; hlt");
    }
}

void move_cursor(int x, int y)
{
	uint16_t pos = y * DEFAULT_VGA_WIDTH + x;

	io_out_byte(0x3D4, 0x0F);
	io_out_byte(0x3D5, (uint8_t) (pos & 0xFF));
	io_out_byte(0x3D4, 0x0E);
	io_out_byte(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void update_cursor()
{
    move_cursor(vga_curr_column, vga_curr_row);
}