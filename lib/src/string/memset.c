#include "string.h"
#include <stddef.h>

void *memset (void *dest, register int val, register int len)
{
  register unsigned char *ptr = (unsigned char*)dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}