#pragma once
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#define VGA_BASE_ADDR 0xC00B8000     
                           

#define DEFAULT_VGA_WIDTH 80
#define DEFAULT_VGA_HEIGHT 25

typedef struct vga_char {
    unsigned char ascii_code;
    uint8_t color;
} vga_char;

enum VGA_COLOR {
    VGA_COLOR_BLACK = 0x00,
    VGA_COLOR_BLUE = 0x1,
    VGA_COLOR_GREEN = 0x2,
    VGA_COLOR_CYAN = 0x3,
    VGA_COLOR_RED = 0x4,
    VGA_COLOR_MAGENTA = 0x5,
    VGA_COLOR_BROWN = 0x6,
    VGA_COLOR_LIGHT_GREY = 0x7,
    VGA_COLOR_DARK_GREY = 0x8,
    VGA_COLOR_LIGHT_BLUE = 0x9,
    VGA_COLOR_LIGHT_GREEN = 0xA,
    VGA_COLOR_LIGHT_CYAN = 0xB,
    VGA_COLOR_LIGHT_RED = 0xC,
    VGA_COLOR_PINK = 0xD,
    VGA_COLOR_YELLOW = 0xE,
    VGA_COLOR_WHITE = 0x0F
};

void vga_init();
void vga_scroll();
void vga_line_down();
void vga_putchar(unsigned char c);
void vga_putchar_colored(unsigned char c, enum VGA_COLOR bg, enum VGA_COLOR ch_color);
void vga_putstring(const unsigned char *s);
void vga_putstring_colored(const unsigned char *s, enum VGA_COLOR bg, enum VGA_COLOR ch_color);
void vga_printf(const char* format, ...);
void panic_screen(const char *panic_message);

void move_cursor(int x, int y);
void update_cursor();