#include "io.h"

inline uint8_t io_in_byte(uint16_t port)
{
    uint8_t value;

    asm("inb %1, %0"
            : "=a"(value)
            : "Nd"(port));

    return value;
}

inline void io_out_byte(uint16_t port, uint8_t val)
{
    asm("outb %0, %1"
            :
            : "a"(val), "Nd"(port));
}

inline uint16_t io_in_word(uint16_t port)
{
    uint16_t value;

    asm("inw %1, %0"
            : "=a"(value)
            : "Nd"(port));

    return value;
}

inline void io_out_word(uint16_t port, uint16_t val)
{
    asm("outw %0, %1"
            :
            : "a"(val), "Nd"(port));
}

inline uint32_t io_in_long(uint16_t port)
{
    uint32_t value;

    asm("inl %1, %0"
            : "=a"(value)
            : "Nd"(port));

    return value;
}

inline void io_out_long(uint16_t port, uint32_t val)
{
    asm("outl %0, %1"
            :
            : "a"(val), "Nd"(port));
}

inline void io_wait()
{
    asm("outb %%al, $0x80"
            :
            : "a"(0));
}