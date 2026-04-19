#ifndef MOVEMENT_H
#define MOVEMENT_H

#include <types.h>

// Move a value toward a target by step, returns true if arrived
static inline bool moveToward(s16 *val, s16 target, s16 step) {
    if (*val < target) {
        *val += step;
        if (*val > target) *val = target;
    } else if (*val > target) {
        *val -= step;
        if (*val < target) *val = target;
    }
    return (*val == target);
}

#endif // MOVEMENT_H
