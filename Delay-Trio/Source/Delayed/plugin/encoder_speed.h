#ifndef OP1_ENCODER_SPEED_H
#define OP1_ENCODER_SPEED_H

#include "opus_plugin.h"

/* TILT's 120 ms burst, owned by one plugin's KeyHandler timer. */
struct encoder_speed {
    const volatile void *owner;
    int tag, direction, burst;
};

static inline int encoder_speed_turn(struct encoder_speed *speed,
                                     struct opl_self *self, void (*expire)(void),
                                     const volatile void *owner, int tag, int detents)
{
    if (!self || !detents)
        return detents;
    if (detents > 256) detents = 256;
    if (detents < -256) detents = -256;
    int direction = detents < 0 ? -1 : 1;
    if (speed->owner != owner || speed->tag != tag || speed->direction != direction) {
        speed->burst = 0;
        speed->owner = owner;
        speed->tag = tag;
        speed->direction = direction;
    }
    int burst = speed->burst + direction * detents;
    speed->burst = burst > 16 ? 16 : burst;
    opus_key_timer(self, 120, expire);
    return detents * (burst >= 10 ? 8 : burst >= 6 ? 4 : burst >= 3 ? 2 : 1);
}

#endif
