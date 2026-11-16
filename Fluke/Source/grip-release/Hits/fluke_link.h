#ifndef HITS_FLUKE_LINK_H
#define HITS_FLUKE_LINK_H

/* Private request sent directly to the HITS or FLUX page callback.
 * It is never posted to Opus's key queue. The page copy's inst carries the
 * FLUKE callbacks for the duration of the registration call. */
#define HITS_FLUKE_CONNECT_KEY (-0x464c)

struct hits_fluke_link {
    unsigned (*keys)(int *edited);
    void (*edit)(int pad);
    int (*root)(void);
    int (*hold)(int pressed);
    int (*cut)(int pressed);
};
int hits_connect_fluke(const struct hits_fluke_link *link);

#endif
