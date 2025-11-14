#pragma once

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

typedef long long time_t;
typedef long long clock_t;
typedef long suseconds_t; 

struct timeval {
    time_t      tv_sec;     /* seconds */
    suseconds_t tv_usec;    /* microseconds */
};

struct timezone {
    int tz_minuteswest;     /* minutes west of Greenwich */
    int tz_dsttime;         /* type of DST correction */
};

struct tms {
    clock_t tms_utime;  /* user time */
    clock_t tms_stime;  /* system time */
    clock_t tms_cutime; /* user time of children */
    clock_t tms_cstime; /* system time of children */
};

int _gettimeofday(struct timeval *p, struct timezone *z);
clock_t _times(struct tms *buf);