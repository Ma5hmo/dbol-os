#include "time.h"
#include "util/io/io.h"
#include <string.h>
#include <stddef.h>

static uint8_t cmos_read(uint8_t reg) {
    io_out_byte(CMOS_ADDRESS, reg);
    return io_in_byte(CMOS_DATA);
}

int _gettimeofday(struct timeval *p, struct timezone *z)
{
    // Get the current time from RTC
    p->tv_sec = (time_t)(
        (cmos_read(0x09) * 3600) +  // Hours
        (cmos_read(0x08) * 60) +    // Minutes
        (cmos_read(0x07))           // Seconds
    );
    p->tv_usec = 0;  // RTC doesn't provide microseconds, set to 0

    // Fill in timezone (optional, set to 0 if not needed)
    if (z != NULL) {
        z->tz_minuteswest = 0;  // No timezone support, default to GMT
        z->tz_dsttime = 0;      // No DST support
    }

    return 0;  // Success
}

clock_t _times(struct tms *buf)
{
    memset(buf, 0, sizeof(struct tms));
    return 0;
}