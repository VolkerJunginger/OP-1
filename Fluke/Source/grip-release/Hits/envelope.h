#ifndef HITS_ENVELOPE_H
#define HITS_ENVELOPE_H

/* HITS routing IDs, independent of the OP-1 page and encoder numbers. */
enum hits_env_target {
    HITS_ENV_ATTACK = 0x100,
    HITS_ENV_DECAY = 0x101,
    HITS_ENV_LENGTH = 0x102,
    HITS_ENV_PAD_LEVEL = 0x103
};

enum hits_pad_word {
    HITS_PAD_LENGTH = 3,
    HITS_PAD_LEVEL = 6
};

static inline unsigned hits_length_ticks(unsigned length)
{
    return (643 + length + (length >> 1)) >> 2;
}

#endif
