#ifndef C_WATCHER_TIMECHECK_H_INCLUDED
#define C_WATCHER_TIMECHECK_H_INCLUDED

#include <stdint.h>


#define time_after_or_equal(a, b) (((long)((b) - (a)) <= 0))

static inline __attribute__((always_inline)) uint8_t is_expired(unsigned long start, unsigned long current,
                                                                unsigned long delay) {
    return time_after_or_equal(current, start + delay);
}


#endif