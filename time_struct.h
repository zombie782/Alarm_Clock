#ifndef TIME_STRUCT_H
#define TIME_STRUCT_H

#include <stdint.h>

#define UNSET_HOUR 25
#define UNSET_MINUTE 60

typedef struct time_t{
    uint8_t hour;
    uint8_t minute;
}time_t;

#endif
