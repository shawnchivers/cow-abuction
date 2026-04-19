#ifndef COLLIDE_H
#define COLLIDE_H

#include <types.h>

static inline int isPointInBox(s16 px, s16 py, s16 boxX, s16 boxY, s16 width, s16 height) {
    return (px >= boxX && px < boxX + width) &&
           (py >= boxY && py < boxY + height);
}

#endif // COLLIDE_H
