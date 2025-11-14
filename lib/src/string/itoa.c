#include "string.h"

// Helper function: Convert integer to string with base (10 for decimal, 16 for hexadecimal)
void itoa(int value, char* buffer, int base) {
    const char* digits = "0123456789ABCDEF";
    char temp[32];
    int i = 0, is_negative = 0;

    // Handle negative numbers for base 10
    if (base == 10 && value < 0) {
        is_negative = 1;
        value = -value;
    }

    // Convert number to string in reverse order
    do {
        temp[i++] = digits[value % base];
        value /= base;
    } while (value > 0);

    // Add minus sign if negative
    if (is_negative) {
        temp[i++] = '-';
    }

    // Reverse the string into the buffer
    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
}