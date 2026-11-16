#ifndef HITS_GRIP_LINK_H
#define HITS_GRIP_LINK_H

#include "fluke_link.h"

/* Private page callback request. Never sent to the key queue. */
#define HITS_GRIP_CONNECT_KEY (-0x4752)

struct hits_grip_link {
    unsigned (*kick_peak)(void);
    int (*selected)(void);
    int (*hold_state)(void);
    int (*hold_key)(int pressed);
    int (*connect_fluke)(const struct hits_fluke_link *link);
};

#endif
