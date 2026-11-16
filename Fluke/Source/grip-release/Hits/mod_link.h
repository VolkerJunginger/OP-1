#ifndef HITS_MOD_LINK_H
#define HITS_MOD_LINK_H

#define HITS_MOD_PADS 24

enum hits_mod_target {
    LENGTH, LEVEL, PITCH, TIMBRE, COLOR, LOW_CUT, HIGH_CUT, TARGET_COUNT
};

struct hits_mod_delta {
    short value[TARGET_COUNT];
};

/* Audio interrupt. The table is valid through the end of this block. */
void hits_mod_publish_v2(int slot, const struct hits_mod_delta pads[HITS_MOD_PADS]);

/* UI threads. Mask of FLUKE lanes and the lane currently being edited. */
unsigned hits_mod_keyboard(int *edited);
int hits_is_selected(void);
void hits_edit_key(int key, int pressed);
int hits_fluke_hold_state(void); /* -1 if FLUKE is off or not selected */
int hits_fluke_hold_key(int pressed);

#include "fluke_link.h"

#endif
