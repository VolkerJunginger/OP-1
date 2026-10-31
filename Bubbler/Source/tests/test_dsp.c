#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bubbler_dsp.h"

static short ring[BB_STORAGE];
static struct bb_state s;
static uint32_t rnd = 123;
static uint32_t rng(void) { rnd = rnd * 1664525u + 1013904223u; return rnd; }

static void zero_history_test(void)
{
    short p[8] = { 0, 32767, 32767, 32767, 32767, 32767, -12, 32767 };
    for (unsigned i = 0; i < BB_SAMPLES; ++i) ring[i] = (short)rng();
    bb_init(&s);
    for (int b = 0; b < 3000; ++b) {
        short x[64] = { 0 };
        bb_render(&s, ring, x, 0, 64, p);
        for (int i = 0; i < 64; ++i) assert(x[i] == 0);
    }
}

static void endpoint_test(void)
{
    short p[8] = { 16384, 0, 8192, 0, 0, 32767, 0, 0 };
    short l[64], r[64], before_l[64], before_r[64];
    for (int i = 0; i < 64; ++i) {
        l[i] = before_l[i] = (short)rng();
        r[i] = before_r[i] = (short)rng();
    }
    bb_init(&s); bb_render(&s, ring, l, r, 64, p);
    assert(!memcmp(l, before_l, sizeof l));
    assert(!memcmp(r, before_r, sizeof r));
    p[3] = 32767;
    bb_init(&s); bb_render(&s, ring, l, r, 64, p);
    for (int i = 0; i < 64; ++i) assert(l[i] == 0 && r[i] == 0);
}

static void stress_test(void)
{
    short p[8] = { 0 };
    unsigned max_active = 0, nonzero = 0;
    bb_init(&s);
    for (int b = 0; b < 30000; ++b) {
        if (!(b % 29)) {
            for (int k = 0; k < 8; ++k) p[k] = (short)rng();
            p[6] = (short)((int)(rng() % 25) - 12);
        }
        if (!(b % 997)) bb_init(&s);
        short l[64], r[64];
        for (int i = 0; i < 64; ++i) { l[i] = (short)rng(); r[i] = (short)rng(); }
        bb_render(&s, ring, l, b & 1 ? r : 0, 64, p);
        unsigned active = 0;
        for (int k = 0; k < BB_GRAINS; ++k) {
            struct bb_grain *g = &s.grain[k];
            active += (unsigned)g->active;
            assert(g->pos <= BB_PHASE_MASK);
            assert(g->env <= 256u << 16);
            assert(!g->active || g->age <= g->length);
        }
        if (active > max_active) max_active = active;
        for (int i = 0; i < 64; ++i) nonzero += l[i] != 0;
        assert(s.write < BB_SAMPLES && s.filled <= BB_SAMPLES);
    }
    assert(max_active == 8 && nonzero > 1000);
    printf("stress: 1,920,000 samples; all 8 grains exercised; mono/stereo, resets and invalid parameters\n");
}

static void ring_wrap_test(void)
{
    short p[8] = { 32767, 32767, 32767, 32767, 32767, 32767, 12, 32767 };
    struct bb_params q={0,127,0,32767,100,32767,100,0,32767,63};
    bb_encode(p,&q);bb_set_transpose(0);bb_init(&s);
    unsigned nonzero = 0;
    for (int b = 0; b < 12000; ++b) {
        if (b == 6000) { q.reverse=0;bb_encode(p,&q); }
        short x[64];
        for (int i = 0; i < 64; ++i) x[i] = (short)(rng() >> 17);
        bb_render(&s, ring, x, 0, 64, p);
        if (b < 6000) for (int k = 0; k < BB_GRAINS; ++k)
            if (s.grain[k].active) assert(s.grain[k].step < 0);
        if (b > 7000) for (int k = 0; k < BB_GRAINS; ++k)
            if (s.grain[k].active) assert(s.grain[k].step > 0);
        if (b > 3000) for (int i = 0; i < 64; ++i) nonzero += x[i] != 0;
    }
    assert(s.filled == BB_SAMPLES && nonzero > 1000);
    puts("ring wrap: 768,000 samples, high feedback, +/-12 semitones, reverse extremes");
}

static void extra_controls_test(void)
{
    short p[8]={0,0,0,32767,0,0,0,0};
    struct bb_params config={0,127,0,0,0,0,0,0,8192,0};
    bb_encode(p,&config);bb_init(&s);
    for(int b=0;b<100;++b){
        short x[64];for(int i=0;i<64;++i)x[i]=12000;
        bb_render(&s,ring,x,0,64,p);
        for(int i=0;i<64;++i)assert(x[i]==0);
    }
    config.density=100;config.pitch_var=100;config.size_var=32767;config.jitter=32767;
    bb_encode(p,&config);bb_init(&s);
    int varied_pitch=0,nonzero=0;
    for(int b=0;b<3000;++b){
        short x[64];for(int i=0;i<64;++i)x[i]=(short)rng();
        bb_render(&s,ring,x,0,64,p);
        for(int k=0;k<BB_GRAINS;++k)if(s.grain[k].active && s.grain[k].step!=4096)varied_pitch=1;
        for(int i=0;i<64;++i)nonzero+=x[i]!=0;
    }
    assert(varied_pitch && nonzero>1000);
    puts("extra controls: zero-density silence, maximum density/variation/jitter and randomized pitch");
}

static void fast_memory_test(void)
{
    short expected[64*512];
    short tables[BB_FAST_TABLE_BYTES/2u] __attribute__((aligned(32)));
    int work[64];
    short p[8]={20480,16000,8192,25000,8192,30000,7,12000};
    short clip[BB_FAST_CLIP_BYTES/2u] __attribute__((aligned(32)));
    for(int mode=0;mode<8;++mode) {
        bb_fast_tables((mode&1)?tables:0);bb_fast_clip((mode&2)?clip:0);bb_init(&s);rnd=42;
        for(int block=0;block<512;++block) {
            short x[64];for(int i=0;i<64;++i)x[i]=(short)rng();
            if(mode&4) bb_render_work(&s,ring,x,0,64,p,work);
            else bb_render(&s,ring,x,0,64,p);
            if(!mode) memcpy(expected+block*64,x,sizeof x);
            else assert(!memcmp(expected+block*64,x,sizeof x));
        }
    }
    bb_fast_tables(0);bb_fast_clip(0);
    puts("fast memory: all 8 grain-table / clip-table / scratch configurations match exactly");
}

int main(void)
{
    zero_history_test();
    endpoint_test();
    stress_test();
    ring_wrap_test();
    extra_controls_test();
    fast_memory_test();
    puts("PASS: stale-buffer silence, dry/wet endpoints, bounds and arithmetic stress");
    return 0;
}
