#include "string.h"

char *strncpy(char *destination, const char *source, size_t num)
{
    char *str = destination;
    if (num == 0)
        return str;
    do
    {
        *destination = *source;
        source++,destination++,num--;
    }while(*source && num);
    return str;
}