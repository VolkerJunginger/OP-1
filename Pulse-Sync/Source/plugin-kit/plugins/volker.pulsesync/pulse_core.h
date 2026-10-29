/* GPL-3.0-or-later. Signed 16-bit, one chosen edge per pulse. */
#ifndef PULSE_CORE_H
#define PULSE_CORE_H
struct pulse_detector { unsigned armed, blank; };
static void pulse_reset(struct pulse_detector *d) { d->armed = 0; d->blank = 0; }
static unsigned pulse_sample(struct pulse_detector *d, int sample, unsigned threshold, unsigned negative)
{
    int x = negative ? -sample : sample;
    if (d->blank) --d->blank;
    if (x <= (int)(threshold / 4u)) d->armed = 1;
    if (d->armed && x >= (int)threshold) {
        d->armed = 0;
        if (!d->blank) {
            d->blank = 88; /* 2 ms at 44.1 kHz; 4 ms at 22.05 kHz. */
            return 1;
        }
    }
    return 0;
}
static int pulse_ring_room(unsigned wr, unsigned rd)
{
    return (unsigned)(wr - rd) < 1023u;
}
#endif
