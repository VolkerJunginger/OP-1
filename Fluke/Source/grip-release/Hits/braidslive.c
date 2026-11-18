// Copyright 2012 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
#include "opus_plugin.h"
#include "pitch.h"
#include "op1fw.h"
#include "panel.h"
#include "fighters.h"
#include "tables.h"
#include "envelope.h"
#include "mod_link.h"
#include "grip_link.h"

enum model {
    KICK, SNARE, CYMBAL, BELL, DRUM, PLUCK, NOISE, TWIN, CLOCKED, CLOUD,
    PARTICLE, WTFM, QPSK, FM, FBFM, TOY, RING, MODEL_COUNT
};
static const char *const model_names[] = {
    "kick", "snare", "cymbal", "bell", "drum", "pluck", "noise", "twin noise",
    "clock noise", "cloud", "particle", "wild fm", "qpsk", "fm", "feedback fm",
    "toy", "ring"
};

static int model_value(int model)
{
    // Retain the original three preset encodings.
    return model < 3 ? model * 10922 + 5461 : model - 2;
}

static int model_index(int value)
{
    return value > 0 && value <= MODEL_COUNT - 3 ? value + 2 : value * 3 >> 15;
}
struct filter { int low, band, f, damp; };
struct voice {
    struct filter filters[3];
    unsigned phase[6], clock, random, seed, age, end, choke_left, envelope, fall;
    int pulse[4], low, model, velocity, choked, pad;
    int hp_state[2], lp_state[2];
    short string[256];
    unsigned string_index, string_fill;
};

/* Factory pad parameters from the OP-1 snapshot 20261111_2116. */
static const short kit[24][8] = {
    {-12800, 4804, 0, 7872, 5461, 0, 24576, 0},
    {-11156, 2848, 0, 9500, 5461, 0, 24576, 0},
    {-12966, 12464, 6561, 0, 16383, 0, 24576, 0},
    {-6348, 6232, 17516, 984, 16383, 0, 24576, 0},
    {-996, 19024, 13152, 4592, 2, 0, 24576, 0},
    {12288, 24000, 28000, 4100, 4, 0, 24576, 0},
    {8192, 26000, 9500, 2500, 27305, 2048, 24576, 0},
    {4096, 21000, 16500, 4100, 27305, 2048, 24576, 0},
    {7610, 32767, 10156, 984, 27305, 2048, 24576, 0},
    {4608, 23900, 5000, 1640, 27305, 0, 24576, 0},
    {10752, 22500, 23500, 29000, 27305, 0, 24576, 0},
    {-1216, 19144, 23284, 2296, 4, 0, 24576, 0},
    {0, 27000, 1000, 15000, 1, 0, 24576, 0},
    {2560, 13000, 12000, 11000, 3, 0, 24576, 0},
    {12800, 17000, 1500, 6200, 5, 0, 24576, 0},
    {11776, 31000, 3500, 9000, 6, 0, 24576, 0},
    {4096, 14000, 30000, 12500, 7, 0, 24576, 0},
    {15360, 24500, 18000, 15500, 8, 0, 24576, 0},
    {4452, 12612, 24048, 1193, 9, 0, 24576, 0},
    {5120, 12000, 16000, 984, 10, 0, 24576, 0},
    {-6656, 15500, 10500, 13000, 11, 0, 24576, 0},
    {-2560, 25000, 19000, 11000, 12, 0, 24576, 0},
    {8704, 26000, 23000, 4300, 13, 0, 24576, 0},
    {3584, 13500, 14500, 9500, 14, 0, 24576, 0},
};
static struct opl_self *plugin;
#include "encoder_speed.h"
static struct encoder_speed hits_speed;
static void hits_expire(void) { hits_speed.burst = 0; }


static inline int clamp(int x, int lo, int hi)
{
    return x < lo ? lo : x > hi ? hi : x;
}

static unsigned phase_step(int note, unsigned ratio)
{
    note = clamp(note, 0, 126*512);
    unsigned a = phases[note >> 9], b = phases[(note >> 9) + 1];
    unsigned base = a + (((b - a) >> 9) * (note & 511));
    // Cap modulation before multiplication and keep oscillators below Nyquist.
    unsigned step = (base >> 15) * (ratio >> 4);
    return step < (0x70000000u >> 5) ? step << 5 : 0x70000000u;
}

static int coefficient(int note, unsigned ratio)
{
    // Small-angle 2*sin(pi*f/fs) approximation, bounded for SVF stability.
    return clamp((int)(phase_step(note, ratio) >> 12) * 201 >> 10, 4, 23000);
}

static inline int svf(struct filter *f, int input, int cutoff, int damp, int high)
{
    int hp = input - (f->band * damp >> 15) - f->low;
    f->low = clamp(f->low + (cutoff * f->band >> 15), -32768, 32767);
    f->band = clamp(f->band + (cutoff * clamp(hp, -90000, 90000) >> 15), -32768, 32767);
    return high ? hp : f->band;
}

static void init(void *state, int index)
{
    struct voice *v = state;
    v->end = v->age = 0;
    v->seed = 0x0f3211a9u + (unsigned)index * 46783u;
}

static void note_key(void *state, const short *par, int key, int velocity)
{
    struct voice *v = state;
    v->pad = key;
    v->model = model_index(par[4]);
    v->string_fill = v->string_index = 0;
    v->velocity = velocity;
    v->age = 0;
    v->hp_state[0] = v->hp_state[1] = 0;
    v->lp_state[0] = v->lp_state[1] = 0;
    v->end = hits_length_ticks((unsigned)par[HITS_PAD_LENGTH]);
    v->envelope = 1u << 24;

    v->choked = 0;
    v->choke_left = 32;
    v->low = 0;
    v->clock = 0;
    v->seed = v->seed * 1664525u + 1013904223u;
    v->random = v->seed ^ ((unsigned)key << 16);
    for (int i = 0; i < 3; ++i)
        v->filters[i].low = v->filters[i].band = 0;
    for (int i = 0; i < 6; ++i)
        v->phase[i] = 0;
    v->pulse[0] = v->model == KICK ? 65536 : 98304;
    v->pulse[1] = v->pulse[2] = 0;
    v->pulse[3] = 512 + 2 * clamp(par[2], 0, 14336);
}

static int group(const short *par, int key)
{
    (void)key;
    return par[5] >> 11;
}

static void choke(void *state) { ((struct voice *)state)->choked = 1; }

static int finished(void *state)
{
    const struct voice *v = state;
    return v->age >= v->end || !v->choke_left;
}

static inline int kick(struct voice *v, int base, int sweep, int damp, int lp)
{
    unsigned age = v->age;
    if (age == 11)
        v->pulse[1] = -13763;
    int in = v->pulse[0] + v->pulse[1] + (age < 11 ? 16384 : 0);
    v->pulse[0] = v->pulse[0] * 1652 >> 12;
    v->pulse[1] = -((-v->pulse[1] * 1178) >> 12);
    int cutoff = age < 44 ? sweep : base;
    int punch = v->filters[0].low > 4096 ? v->filters[0].low : 2048;
    cutoff += punch >> 8;
    damp += (punch - 2048) >> 3;
    int resonator = (in >> 4) + svf(v->filters, in, cutoff, damp, 0);
    v->low += ((resonator - v->low) >> 1) * lp >> 14;
    return v->low * 2;
}

static inline int snare(struct voice *v, int g1, int g2, int decay)
{
    unsigned age = v->age;
    if (age == 11) {
        v->pulse[1] = -24576;
        v->pulse[2] = 13107;
    }
    int a = v->pulse[0] + v->pulse[1] + (age < 11 ? 2621 : 0);
    int b = v->pulse[2] + (age < 11 ? 13107 : 0);
    v->pulse[0] = v->pulse[0] * 56 >> 12;
    v->pulse[1] = -((-v->pulse[1] * 1178) >> 12);
    v->pulse[2] = v->pulse[2] * 18 >> 12;
    v->random = v->random * 1664525u + 1013904223u;
    int noise = ((int)(v->random >> 16) - 32768) * v->pulse[3] >> 15;
    // Round toward zero without losing the last few bits of the tail early.
    v->pulse[3] -= (v->pulse[3] * decay + 4095) >> 12;
    struct filter *f = v->filters;
    return ((svf(f, a, f[0].f, f[0].damp, 0) + (a >> 4)) * g1 >> 15)
         + ((svf(f+1, b, f[1].f, f[1].damp, 0) + (b >> 4)) * g2 >> 15)
         + svf(f+2, noise, f[2].f, f[2].damp, 0);
}

static inline int cymbal(struct voice *v, const unsigned *step, unsigned clock_step, int color)
{
    v->clock += clock_step;
    if (v->clock < clock_step)
        v->random = v->random * 1664525u + 1013904223u;
    int squares = -3;
    for (int i = 0; i < 6; ++i) {
        v->phase[i] += step[i];
        squares += v->phase[i] >> 31;
    }
    struct filter *f = v->filters;
    int metal = svf(f, squares * 5461, f[0].f, 19000, 0);
    int noise = svf(f+1, ((int)(v->random >> 16) - 32768) >> 1, f[0].f, 28000, 1);
    noise = clamp(noise, -32768, 32767);
    return metal + ((noise - metal) * color >> 15);
}

#include "models.h"

static volatile unsigned decimate_global, global_depth;
static const char *const decimate_labels[] = {"dec off", "dec 12 bit", "dec 8 bit", "dec 4 bit"};

static inline int quantize(int sample, int step)
{
    // Signed rounding with ties away from zero. The lower rail is already a code.
    int rounded = (sample + (step >> 1) - (sample < 0)) & -step;
    int ceiling = 32768 - step;
    return rounded > ceiling ? ceiling : rounded;
}

/* Low 12 bits of pad word 7 hold filter positions; the upper two
 * retain the saved Decimate depth. Zero is bypass for each filter. */
static unsigned filter_position(short word, int high_pass)
{
    return ((unsigned short)word >> (high_pass ? 0 : 6)) & 63;
}

static int filter_index(unsigned position, int high_pass)
{
    return !position ? 0 : high_pass ? (int)position : 64 - (int)position;
}

static inline int filter_two_poles(int sample, int state[2], int alpha, int high_pass)
{
    for (int i = 0; i < 2; ++i) {
        state[i] += ((sample - state[i]) * alpha) >> 15;
        sample = high_pass ? clamp(sample - state[i], -32768, 32767) : state[i];
    }
    return sample;
}

/* The low 11 bits of word 5 are independent of the choke group.
 * Zero encodes the center so older kits retain their original sound. */
static int extra_value(short word)
{
    return (((unsigned short)word + 1024) & 2047) - 1024;
}

static const char *extra_label(int model)
{
    switch (model) {
    case KICK: return "sweep";
    case SNARE: return "snap";
    case CYMBAL: return "spread";
    case BELL:
    case DRUM: return "overtone";
    case PLUCK: return "pick";
    case PARTICLE: return "reson";
    default: return "attack";
    }
}

static inline short output(struct voice *v, int sample, int level, int shaped, int fading, int choked, int depth_step, int hp, int lp, unsigned attack, unsigned attack_step)
{
    int gain = level;
    if (attack && v->age < attack)
        gain = gain * (int)(v->age * attack_step >> 10) >> 6;
    if (shaped) {
        int env = v->envelope >> 12;
        gain = gain * (env * env >> 12) >> 12;
        v->envelope -= v->envelope > v->fall ? v->fall : v->envelope;
    }
    unsigned remaining = v->end - v->age;
    int fade = fading && remaining < 32 ? (int)remaining : 32;
    if (choked)
        fade = fade * (int)v->choke_left-- >> 5;
    ++v->age;
    sample = clamp(sample, -32768, 32767);
    if (depth_step > 1)
        sample = quantize(sample, depth_step);
    if (!hp && !lp)
        return (short)(sample * (gain * fade >> 5) >> 15);
    sample = sample * gain >> 15;
    if (hp)
        sample = filter_two_poles(sample, v->hp_state, hp, 1);
    if (lp)
        sample = filter_two_poles(sample, v->lp_state, lp, 0);
    return (short)(sample * fade >> 5);
}

// Render one 11.025 kHz tick for eight output samples. Bound the work once per block.
#define RENDER_TICKS(expression, shaped) \
    for (; i < limit; i += 8) { \
        short sample = output(v, (expression), level, shaped, fading, choked, depth_step, hp, lp, attack, attack_step); \
        out[i] = out[i + 1] = out[i + 2] = out[i + 3] = sample; \
        out[i + 4] = out[i + 5] = out[i + 6] = out[i + 7] = sample; \
    }

static int hits_engine_mark;

static unsigned kick_peak;
static unsigned previous_kick;
static volatile int kick_slot = -1;
static int previous_slot = -1;
static const struct hits_mod_delta *mod_pads;
static int mod_slot;
static volatile int previous_mod_slot = -1;

void hits_mod_publish_v2(int slot, const struct hits_mod_delta pads[HITS_MOD_PADS])
{
    mod_pads = pads;
    mod_slot = slot;
}

static const struct hits_mod_delta *mod_for_pad(int pad)
{
    if (mod_pads && (unsigned)pad < HITS_MOD_PADS &&
        mod_slot == opus_sound_slot(OPUS_FX_DRUM))
        return &mod_pads[pad];
    return 0;
}

static void finish_kick_block(void)
{
    previous_kick = kick_peak;
    previous_slot = kick_slot;
    kick_peak = 0;
    previous_mod_slot = mod_pads ? mod_slot : -1;
    mod_pads = 0;
    kick_slot = -1;
}

/* Use the current block when available and one prior block as a bridge
 * for effects that render before the drum voices. */
unsigned hits_kick_peak(void)
{
    const int slot = opus_sound_slot(OPUS_FX_DRUM);
    if (kick_slot == slot)
        return kick_peak;
    return previous_slot == slot ? previous_kick : 0;
}

int hits_is_selected(void)
{
    const int slot = opus_sound_slot(OPUS_FX_DRUM);
    if (slot < 0 || slot > 7)
        return 0;
    /* Firmware #246 drum patch object: slot word followed by JSON. */
    const unsigned *patch = (const unsigned *)(0x01ddd9a8u + (unsigned)slot * 0x38u);
    if (patch[0] != (unsigned)slot || fw_json_type(patch + 1) != 7)
        return 0;
    const void *meta = fw_json_get(patch + 1, "opus");
    if (fw_json_type(meta) != 7)
        return 0;
    const void *mark = fw_json_get(meta, "mark");
    const int type = fw_json_type(mark);
    return hits_engine_mark > 0 && (type == 1 || type == 2)
        && fw_json_as_int(mark) == hits_engine_mark;
}

static int mod_delta(const struct hits_mod_delta *mod, enum hits_mod_target target)
{
    return mod ? mod->value[target] : 0;
}

static void render(void *state, short *out, const short *par, const unsigned *pitch)
{
    struct voice *v = state;
    if (finished(v)) {
        for (int i = 0; i < 128; ++i)
            out[i] = 0;
        return;
    }
    const struct hits_mod_delta *mod = mod_for_pad(v->pad);
    // Sample modulated Length at the first block of each hit.
    // Later blocks keep the same end and fade even if the LFO moves on.
    if (!v->age) {
        int length = clamp(par[HITS_PAD_LENGTH] + mod_delta(mod, LENGTH), 0, 32767);
        v->end = hits_length_ticks((unsigned)length);
        v->fall = (1u << 24) / v->end + 1;
    }
    unsigned ratio = pitch ? *pitch : 65536;
    if (ratio > 524288)
        ratio = 524288;
    int note = 60*512 + clamp(par[0] + (mod_delta(mod, PITCH) * 6144 >> 15), -24320, 24832);
    int timbre = clamp(par[1] + mod_delta(mod, TIMBRE), 0, 32767);
    int color = clamp(par[2] + mod_delta(mod, COLOR), 0, 32767);
    int pad_level = clamp(par[HITS_PAD_LEVEL] + mod_delta(mod, LEVEL), 0, 32767);
    int level = pad_level * v->velocity >> 7;
    int low_cut = clamp((int)filter_position(par[7], 1) + mod_delta(mod, LOW_CUT) * 63 / 32767, 0, 63);
    int high_cut = clamp((int)filter_position(par[7], 0) + mod_delta(mod, HIGH_CUT) * 63 / 32767, 0, 63);
    int hp = cutoff_alpha[filter_index((unsigned)low_cut, 1)];
    int lp = cutoff_alpha[filter_index((unsigned)high_cut, 0)];
    int extra = extra_value(par[5]);
    unsigned attack = 0;
    if (v->model == NOISE || v->model == TWIN || v->model == CLOCKED ||
        v->model == CLOUD || v->model >= WTFM)
        attack = (unsigned)par[5] & 2047;
    attack >>= 3;
    unsigned attack_step = attack ? 65536 / attack : 0;
    int depth_step = decimate_global ? 1 : 1 << ((par[7] >> 13) * 4);
    unsigned remaining = v->end - v->age;
    unsigned ticks = remaining < 16 ? remaining : 16;
    int choked = v->choked;
    if (choked && v->choke_left < ticks)
        ticks = v->choke_left;
    int limit = (int)ticks * 8;
    int fading = remaining < ticks + 32;
    int i = 0;

    switch (v->model) {
    case KICK: {
        int base = coefficient(note, ratio);
        int sweep = coefficient(note + 17*512 + extra * 8, ratio);
        int tail = 32767 - timbre;
        tail = tail * tail >> 15;
        int damp = 70 + (tail * tail >> 17);
        int bright = color * color >> 15;
        bright = bright * bright >> 15;
        int tone = clamp(256 + (bright * 3 >> 1), 0, 32767);
        tone += tone - (tone * tone >> 15);
        tone += tone - (tone * tone >> 15);
        RENDER_TICKS(kick(v, base, sweep, damp, tone), 0)
        if (i) {
            kick_slot = opus_sound_slot(OPUS_FX_DRUM);
            for (int tick = 0; tick < i; tick += 8) {
                const int sample = out[tick];
                const unsigned peak = (unsigned)(sample < 0 ? -sample : sample);
                if (peak > kick_peak)
                    kick_peak = peak;
            }
        }
        break;
    }
    case SNARE: {
        struct filter *f = v->filters;
        f[0].f = coefficient(note + 12*512, ratio);
        f[1].f = coefficient(note + 24*512, ratio);
        f[2].f = coefficient(note + 60*512, ratio);
        // Bound cutoff for the heavily damped noise filter.
        if (f[2].f > 20000)
            f[2].f = 20000;
        f[0].damp = 1500;
        f[1].damp = 3500;
        f[2].damp = 28000;
        int decay = clamp(32 - 4 * (color >> 13) - extra / 40, 3, 64);
        int g1 = 22000 - (timbre >> 1), g2 = 22000 + (timbre >> 1);
        RENDER_TICKS(snare(v, g1, g2, decay), 0)
        break;
    }
    case CYMBAL: {
        unsigned step[6];
        step[0] = phase_step(40*512 + (note >> 1), ratio);
        if (step[0] > 0x14000000u)
            step[0] = 0x14000000u;
        unsigned root = step[0] >> 14;
        static const int ratios[] = {24273, 12561, 18417, 22452, 31858};
        for (int i = 0; i < 5; ++i)
            step[i + 1] = root * (unsigned)(ratios[i] + (ratios[i] - 16384) * extra / 4096);
        int filter_note = 56*512 + (timbre >> 1);
        v->filters[0].f = clamp(coefficient(filter_note, 65536), 200, 19000);
        unsigned clock_step = step[0] > 0x0aaaaaaau ? 0xffffffffu : step[0] * 24;
        RENDER_TICKS(cymbal(v, step, clock_step, color), 1)
        break;
    }
    case BELL:
    case DRUM: {
        int drum = v->model == DRUM;
        unsigned root = phase_step(note, ratio) >> 4;
        unsigned step[] = {root * 16u, root * (drum ? 23u : 37u),
                          root * ((drum ? 33u : 51u) + (timbre >> 12))};
        RENDER_TICKS(modal(v, step, color, drum, extra), 1)
        break;
    }
    case PLUCK: {
        unsigned hz_step = phase_step(note, ratio) >> 16;
        unsigned length = (unsigned)clamp(65536 / (int)(hz_step ? hz_step : 1), 3, 256);
        // A new delay span must be initialized before reading it.
        RENDER_TICKS(pluck(v, length, 31500 + (timbre >> 5), clamp(color + extra * 16, 0, 32767)), 0)
        break;
    }
    case NOISE: {
        int cutoff = coefficient(note, ratio);
        int damp = 24000 - (timbre >> 1);
        RENDER_TICKS(filtered_noise(v, cutoff, damp, color), 1)
        break;
    }
    case TWIN:
    case PARTICLE: {
        int f1 = coefficient(note - 7*512, ratio);
        int f2 = coefficient(note + 512 + (color >> 2), ratio);
        int particle = v->model == PARTICLE;
        int damp = particle ? clamp(5300 - extra * 4, 500, 10000) : 15000 - (timbre >> 2);
        int density = 128 + (timbre >> 3);
        RENDER_TICKS(resonant_noise(v, f1, f2, damp, density, particle), 1)
        break;
    }
    case CLOCKED:
    case QPSK: {
        unsigned clock = phase_step(note + (timbre >> 2), ratio);
        int qpsk = v->model == QPSK;
        RENDER_TICKS(clocked(v, clock, color, qpsk), 1)
        break;
    }
    case CLOUD:
    case RING: {
        unsigned root = phase_step(note, ratio) >> 4;
        unsigned steps[] = {root * 16u, root * (17u + (color >> 12)),
                           root * (13u + (timbre >> 11))};
        if (v->model == CLOUD) {
            unsigned grain = 4000000u + (unsigned)timbre * 512u;
            RENDER_TICKS(cloud(v, steps, grain), 1)
        } else {
            RENDER_TICKS(ring_hit(v, steps, color), 1)
        }
        break;
    }
    case FM:
    case FBFM:
    case WTFM: {
        unsigned root = phase_step(note, ratio);
        unsigned mod = (root >> 4) * (8u + (color >> 10));
        int index = 8192 + timbre * 3;
        int feedback = v->model == FM ? 0 : (v->model == FBFM ? color : 2 * color);
        RENDER_TICKS(fm_hit(v, root, mod, index, feedback), 1)
        break;
    }
    case TOY: {
        unsigned step = phase_step(note, ratio);
        int bits = 5 + (timbre >> 12);
        RENDER_TICKS(toy_hit(v, step, bits, color), 1)
        break;
    }
    }
    for (; i < 128; ++i)
        out[i] = 0;
}

#include "decimate.h"

static int is_drum_t1(const struct opus_page *pg)
{
    return pg->kind == OPUS_PAGE_DRUM && pg->t == 1 && pg->key >= 0 &&
           pg->key < 24 && pg->par && pg->n >= 8;
}

static const struct hits_fluke_link *volatile fluke_link;

int hits_connect_fluke(const struct hits_fluke_link *link)
{
    if (link && fluke_link == link)
        return 1;
    if (!link || fluke_link || !link->keys || !link->edit || !link->root || !link->hold || !link->cut)
        return 0;
    fluke_link = link;
    return 1;
}

static unsigned companion_keys(int *edited)
{
    if (fluke_link)
        return fluke_link->keys(edited);
    *edited = -1;
    return 0;
}

static int fluke_active(unsigned keys)
{
    return (keys & ((1u << 31) | (1u << 28))) == ((1u << 31) | (1u << 28));
}

int hits_fluke_hold_state(void)
{
    int edited;
    unsigned keys = companion_keys(&edited);
    return fluke_active(keys) ? !!(keys & (1u << 30)) : -1;
}

int hits_fluke_hold_key(int pressed)
{
    return hits_fluke_hold_state() >= 0 ? fluke_link->hold(pressed) : 0;
}

static int flux_active(int slot)
{
    /* FLUX publishes even at zero depth. The completed-block marker expires
     * when T4 goes off and never carries activity to another drum slot. */
    return slot >= 0 && previous_mod_slot == slot;
}

void hits_edit_key(int key, int pressed)
{
    if (!pressed || opus_fn_held())
        return;
    if (!((key >= 0x0d && key <= 0x0f) || (key >= 0x16 && key <= 0x20) ||
          (key >= 0x27 && key <= 0x2a) || (key >= 0x30 && key <= 0x35)))
        return;
    if (hits_fluke_hold_state() >= 0)
        fluke_link->edit(fw_shift_held() ? fw_key_to_index(key) : -1);
}

static const struct opus_engine engine;

static void connect_visible_grip(void)
{
    static int connected;
    if (connected)
        return;
    struct opus_page page;
    if (!opus_page_now(&page) || page.kind != OPUS_PAGE_EFFECT || !page.module)
        return;
    const struct opus_effect *effect = page.module;
    if (effect->size < sizeof(*effect) || !effect->page_key)
        return;
    const char *name = effect->name;
    if (!name || name[0] != 'g' || name[1] != 'r' || name[2] != 'i' ||
        name[3] != 'p' || name[4])
        return;
    static struct hits_grip_link link = {
        .kick_peak = hits_kick_peak,
        .selected = hits_is_selected,
        .hold_state = hits_fluke_hold_state,
        .hold_key = hits_fluke_hold_key,
        .connect_fluke = hits_connect_fluke,
    };
    page.inst = &link;
    connected = effect->page_key(&page, HITS_GRIP_CONNECT_KEY, 0);
}

static int hits_keyboard(int key, int pressed)
{
    if (pressed)
        connect_visible_grip();
    struct opus_page page;
    if (opus_page_now(&page) && page.kind == OPUS_PAGE_DRUM && page.where == OPUS_FX_DRUM &&
        page.t == 1 && page.module == &engine)
        hits_edit_key(key, pressed);
    return 0; /* The drum controller still handles the key and updates its pad pointer. */
}


/* Playback can move the OS's current pad after FLUKE selects it. Refresh
 * the page's writable pad pointer before changing any T1 parameter. */
static int retarget_edit_page(struct opus_page *page)
{
    int pad;
    unsigned keys = companion_keys(&pad);
    if (!fluke_active(keys) || (unsigned)pad >= HITS_MOD_PADS || page->key == pad)
        return 1;
    struct opus_page current;
    if (!opus_drum_select_key(pad) || !opus_page_now(&current) ||
        !is_drum_t1(&current) || current.module != page->module ||
        current.slot != page->slot || current.key != pad)
        return 0;
    *page = current;
    return 1;
}

static int page_key(struct opus_page *pg, int key, int pressed)
{
    if (key == HITS_FLUKE_CONNECT_KEY)
        return !pressed && pg->module == &engine && hits_connect_fluke(pg->inst);
    if (key == 0x0c && pg->kind == OPUS_PAGE_DRUM && pg->t == 1 &&
        !opus_fn_held())
        return fluke_link ? fluke_link->cut(pressed) : 0;
    if (key == 0x3e && pg->kind == OPUS_PAGE_DRUM && pg->t == 1 &&
        !fw_shift_held() && !opus_fn_held())
        return hits_fluke_hold_key(pressed);
    if (!is_drum_t1(pg) || fw_shift_held() || opus_fn_held() ||
        (key != 0x3b && key != 0x3c && key != 0x3d))
        return 0;
    if (pressed) {
        if (!retarget_edit_page(pg))
            return 1;
        volatile short *par = pg->par;
        if (key == 0x3b) {
            hits_expire();
            int model = model_index(par[4]) + 1;
            par[4] = (short)model_value(model == MODEL_COUNT ? 0 : model);
        } else if (key == 0x3c) {
            par[5] = (short)((par[5] & 2047) | ((((par[5] >> 11) + 1) & 15) * 2048));
        } else {
            if (decimate_global) {
                global_depth = (global_depth + 1) & 3;
                opus_set_setting(plugin, "decimate_depth", (int)global_depth);
            } else {
                par[7] = (short)((par[7] & 8191) | ((((par[7] >> 13) + 1) & 3) << 13));
            }
        }
    }
    return 1;
}

static int page_turn(struct opus_page *pg, int encoder, int detents)
{
    static const int step[] = {512, 512, 512, 768};
    if (!is_drum_t1(pg) || encoder < 0 || encoder > 7 || opus_fn_held())
        return 0;
    // The stock drum route can pass the Shift bank as encoder IDs 4–7.
    int shifted = encoder >= 4 || fw_shift_held();
    encoder &= 3;
    if (!detents || !retarget_edit_page(pg))
        return 1;
    int tag = encoder + 4 * shifted + 8 * pg->key + 192 * pg->slot;
    detents = clamp(encoder_speed_turn(&hits_speed, plugin, hits_expire, pg->par, tag, detents), -64, 64);
    volatile short *par = pg->par;
    if (!shifted) {
        int lo = encoder == 0 ? -24320 : 0;
        int hi = encoder == 0 ? 24832 : 32767;
        if (encoder == 2) {
            int model = model_index(par[4]);
            int bipolar = model == KICK || model == SNARE || model == CYMBAL ||
                          model == BELL || model == DRUM || model == PLUCK || model == PARTICLE;
            int extra = bipolar ? extra_value(par[5]) : par[5] & 2047;
            extra = clamp(extra + detents * 32, bipolar ? -1024 : 0, bipolar ? 1023 : 2047);
            par[5] = (short)((par[5] & ~2047) | (extra & 2047));
        } else if (encoder == 3) {
            int value = par[HITS_PAD_LENGTH];
            int direction = detents > 0 ? 1 : -1;
            for (int remaining = detents * direction; remaining > 0; --remaining)
                value = clamp(value + direction * (32 + (value >> 5)), 0, 32767);
            par[HITS_PAD_LENGTH] = (short)value;
        } else {
            par[encoder] = (short)clamp(par[encoder] + detents * step[encoder], lo, hi);
        }
    } else if (encoder < 2) {
        int param = encoder == 0 ? 2 : HITS_PAD_LEVEL;
        par[param] = (short)clamp(par[param] + detents * 512, 0, 32767);
    } else {
        int high_pass = encoder == 2;
        int shift = high_pass ? 0 : 6;
        int delta = high_pass ? detents : -detents;
        int position = clamp((int)filter_position(par[7], high_pass) + delta, 0, 63);
        par[7] = (short)((par[7] & ~(63 << shift)) | (position << shift));
    }
    return 1;
}

static struct {
    unsigned object[41];
    char shown[16];
} panel_text[16];

static void label(void *raster, int slot, int x, int y, int size, int color, const char *value)
{
    static const int identity[9] = {1024, 0, 0, 0, 1024, 0, 0, 0, 1024};
    if (!raster) {
        if (slot == 1 || slot == 11)
            opus_page_text_right(plugin, slot, x, y, size, color, value);
        else
            opus_page_text(plugin, slot, x, y, size, color, value);
        return;
    }

    unsigned *object = panel_text[slot].object;
    char *shown = panel_text[slot].shown;
    unsigned i = 0;
    while (i + 1 < sizeof panel_text[slot].shown && value[i] && value[i] == shown[i])
        ++i;
    if (value[i] != shown[i]) {
        while (i + 1 < sizeof panel_text[slot].shown && value[i]) {
            shown[i] = value[i];
            ++i;
        }
        shown[i] = 0;
        fw_set_text(object, shown);
    }
    /* Match FLUKE's parameter-band transform on the native screen. */
    const int lower = (slot >= 3 && slot <= 10) || slot == 13 || slot == 14;
    const int matrix[9] = {983,0,0,0,983,0,x*41,146*41,1024};
    fw_text_draw(object, raster, lower ? matrix : identity);
}

static unsigned char *sprite_fill[sizeof sprite_nodes / sizeof sprite_nodes[0]];
static int shown_fighter = -1;

static void select_fighter(int model)
{
    if (model == shown_fighter)
        return;
    shown_fighter = model;
    unsigned pose = (unsigned)model % 3;
    for (unsigned i = 0; i < sizeof sprite_nodes / sizeof sprite_nodes[0]; ++i) {
        unsigned char *fill = sprite_fill[i];
        if (!fill)
            continue;
        unsigned group = sprite_nodes[i].group;
        fill[3] = group == pose || group == (unsigned)model + 3 ? 255 : 0;
        if (fill[3] && sprite_nodes[i].layer >= 3) {
            unsigned color = sprite_nodes[i].layer == 3 ? fighters[model].outfit : fighters[model].accent;
            fill[0] = (unsigned char)(color >> 16);
            fill[1] = (unsigned char)(color >> 8);
            fill[2] = (unsigned char)color;
        }
    }
}

static void *fluke_keyboard;
static void *pad_node[24], *edit_node[24];
static unsigned shown_fluke_keys;
static int shown_fluke_edit;
static int fluke_keyboard_visible;

unsigned hits_mod_keyboard(int *edited)
{
    unsigned state = companion_keys(edited);
    return fluke_active(state) ? state & 0xffffffu : 0;
}

static void show_key(void *node, int visible)
{
    if (node)
        ((unsigned char *)node)[0x50] = (unsigned char)!!visible;
}

static void show_fluke_keys(unsigned keys, int edited)
{
    if (keys == shown_fluke_keys && edited == shown_fluke_edit)
        return;
    shown_fluke_keys = keys;
    shown_fluke_edit = edited;
    for (int key = 0; key < 24; ++key) {
        show_key(pad_node[key], key != edited && (keys & (1u << key)));
        show_key(edit_node[key], key == edited);
    }
}

static void screen_init(void *screen)
{
    static const unsigned char colors[][4] = {
        {105, 142, 255, 255}, {0, 237, 149, 255},
        {205, 205, 255, 255}, {255, 66, 63, 255}
    };
    for (unsigned i = 0; i < sizeof sprite_nodes / sizeof sprite_nodes[0]; ++i)
        sprite_fill[i] = screen ? fw_screen_fill(screen, sprite_nodes[i].id) : 0;
    void *document = screen ? *(void **)screen : 0;
    fluke_keyboard = document ? fw_svg_node_find(document, "flukekeys", 0) : 0;
    fluke_keyboard_visible = -1;
    shown_fluke_keys = 0;
    shown_fluke_edit = -1;
    for (int i = 0; i < 24; ++i) {
        char id[8];
        fw_snprintf(id, sizeof id, "pad%02d", i);
        pad_node[i] = document ? fw_svg_node_find(document, id, 0) : 0;
        fw_snprintf(id, sizeof id, "red%d", i);
        edit_node[i] = document ? fw_svg_node_find(document, id, 0) : 0;
        show_key(pad_node[i], 0);
        show_key(edit_node[i], 0);
    }
    shown_fighter = -1;
    select_fighter(KICK);
    static const unsigned char cream[] = {255, 240, 207, 255};
    fw_text_new(panel_text[11].object, 311 * 1024, 9, 12, colors[3], 2);
    fw_text_new(panel_text[0].object, 8 * 1024, 69, 8, colors[2], 0);
    fw_text_new(panel_text[1].object, 311 * 1024, 9, 12, cream, 2);
    fw_text_new(panel_text[2].object, 8 * 1024, 34, 16, colors[0], 0);
    for (int i = 0; i < 4; ++i) {
        int x = (8 + i * 78) * 1024;
        fw_text_new(panel_text[3+i].object, x, 93, 11, colors[i], 0);
        fw_text_new(panel_text[7+i].object, x, 107, 18, colors[i], 0);
    }
    fw_text_new(panel_text[13].object, 242 * 1024, 128, 9, colors[3], 0);
    fw_text_new(panel_text[12].object, 8 * 1024, 57, 8, colors[1], 0);
    static const unsigned char hold_dim[] = {102, 26, 25, 255};
    fw_text_new(panel_text[14].object, 242 * 1024, 128, 9, hold_dim, 0);
    static const unsigned char white[] = {255, 255, 255, 255};
    fw_text_new(panel_text[15].object, 244 * 1024, 9, 12, white, 2);
    for (int i = 0; i < 16; ++i)
        panel_text[i].shown[0] = 0;
}

static void format_note(char *value, unsigned size, int note, int editing)
{
    static const char *const notes[] = {"c","c#","d","d#","e","f","f#","g","g#","a","a#","b"};
    fw_snprintf(value, size, editing ? "edit %s%d" : "%s%d",
                notes[note % 12], note / 12 - 1);
}

static void draw_controls(const short *par, int key, int note, int editing, void *raster)
{
    static const char *const labels[] = {"pitch", "timbre", "color", "length"};
    static const char *const shift_labels[] = {"color", "pad level", "low cut", "high cut"};
    static const int colors[] = {OPUS_COLOR_E1, OPUS_COLOR_E2, OPUS_COLOR_E3, OPUS_COLOR_E4};
    char value[16];
    int model = model_index(par[4]);
    int shifted = fw_shift_held();
    if (!raster) opus_page_fill(0, 0, 319, 146, OPUS_RGB(0x0c0913));
    if (!raster)
        opus_page_text(plugin, 14, 8, 7, 14, OPUS_RGB(0xffc446), "hits");
    unsigned depth = decimate_global ? global_depth : (unsigned)par[7] >> 13;
    label(raster, 0, 8, 69, 8, OPUS_COLOR_E3, decimate_labels[depth]);
    if (note >= 0) format_note(value, sizeof value, note, editing);
    else fw_snprintf(value, sizeof value, "play a pad");
    label(raster, editing ? 11 : 1, 311, 9, 12, editing ? OPUS_COLOR_E4 : OPUS_COLOR_E3, value);
    label(raster, 2, 8, 34, 16, OPUS_COLOR_E1, key >= 0 ? model_names[model] : "select pad");
    if (key >= 0)
        fw_snprintf(value, sizeof value, "choke %02d", par[5] >> 11);
    else
        fw_snprintf(value, sizeof value, "choke -");
    label(raster, 12, 8, 57, 8, OPUS_COLOR_E2, value);
    for (int i = 0; !raster && i < 24; ++i) {
        int x = 14 + i * 12;
        opus_page_fill(x, 75, x + 8, 79, i == key ? OPUS_COLOR_E1 : OPUS_RGB(0x303840));
    }
    for (int i = 0; i < 4; ++i) {
        int x = 8 + i * 78;
        unsigned bar;
        if (shifted && i >= 2) {
            unsigned position = filter_position(par[7], i == 2);
            unsigned hz = cutoff_hz[filter_index(position, i == 2)];
            bar = position * 32767 / 63;
            if (hz) fw_snprintf(value, sizeof value, "%u", hz);
            else fw_snprintf(value, sizeof value, "off");
        } else if (!shifted && i == 0) {
            fw_snprintf(value, sizeof value, "%d", 60 + par[0] / 512);
            bar = (unsigned)(par[0] + 24320) * 2 / 3;
        } else if (!shifted && i == 3) {
            fw_snprintf(value, sizeof value, "%d", hits_length_ticks((unsigned)par[HITS_PAD_LENGTH]) * 40 / 441);
            bar = (unsigned)par[HITS_PAD_LENGTH];
        } else {
            if (!shifted && i == 2) {
                int extra = extra_value(par[5]);
                int bipolar = model == KICK || model == SNARE || model == CYMBAL ||
                              model == BELL || model == DRUM || model == PLUCK || model == PARTICLE;
                bar = (unsigned)(bipolar ? extra + 1024 : extra & 2047) << 4;
            } else {
                bar = (unsigned)par[shifted ? (i == 0 ? 2 : HITS_PAD_LEVEL) : i];
            }
            fw_snprintf(value, sizeof value, "%u", (bar * 100 + 16383) >> 15);
        }
        label(raster, 3 + i, x, 93, 11, colors[i], shifted ? shift_labels[i] : (i == 2 ? extra_label(model) : labels[i]));
        label(raster, 7 + i, x, 107, 18, colors[i], key >= 0 ? value : "-");
        if (!raster) opus_page_bar(x, 132, x + 61, 135, bar, 32767, colors[i]);
    }
}

static void page(void)
{
    short par[8] = {0};
    int key = opus_drum_key(par);
    key = key >= 0 && key < 24 ? key : -1;
    draw_controls(par, key, key < 0 ? -1 : 53 + key, 0, 0);
}

static void screen_draw(void *screen, int dt)
{
    (void)dt;
    if (!screen)
        return;
    void *raster = fw_screen_raster();
    short par[8] = {0};
    struct opus_page pg;
    int key = -1;
    int edited;
    unsigned keys = companion_keys(&edited);
    int active = fluke_active(keys);
    if (active && (unsigned)edited < HITS_MOD_PADS) {
        key = opus_drum_key(par);
        if (key != edited && opus_drum_select_key(edited))
            key = opus_drum_key(par);
        if (key != edited)
            key = -1;
    } else if (opus_page_now(&pg) && is_drum_t1(&pg)) {
        key = pg.key;
        for (int i = 0; i < 8; ++i)
            par[i] = pg.par[i];
    } else {
        int current = opus_drum_key(par);
        if ((unsigned)current < HITS_MOD_PADS)
            key = current;
    }
    int model = key >= 0 ? model_index(par[4]) : KICK;
    select_fighter(model);
    int lanes_visible = active && (keys & (1u << 29));
    int visible = active && (lanes_visible || flux_active(opus_sound_slot(OPUS_FX_DRUM)));
    if (visible != fluke_keyboard_visible) {
        fluke_keyboard_visible = visible;
        if (fluke_keyboard)
            ((unsigned char *)fluke_keyboard)[0x50] = (unsigned char)visible; /* Same node visibility byte as Fluke. */
    }
    if (visible)
        show_fluke_keys(lanes_visible ? keys & 0xffffffu : 0, edited);
    fw_screen_draw(screen);
    if (raster) {
        int editing = active && edited >= 0;
        int note = editing ? 53 + edited : active ? fluke_link->root() : key < 0 ? -1 : 53 + key;
        draw_controls(par, key, note, editing, raster);
        if (visible && active && editing && (keys & (1u << 30))) {
            char root[12];
            format_note(root, sizeof root, fluke_link->root(), 0);
            label(raster, 15, 244, 9, 12, OPUS_RGB(0xffffff), root);
        }
        if (visible && active) {
            int held = (keys & (1u << 30)) != 0;
            label(raster, held ? 13 : 14, 242, 128, 9, OPUS_COLOR_E4,
                  "hold");
        }
    }
}

static const struct opus_engine engine = {
    .size = sizeof(struct opus_engine),
    .name = "hits",
    .voice = sizeof(struct voice),
    .init = init,
    .render = render,
    .page = page,
    .svg = "/yaffs2/mod/esc.hits/panel.svg",
    .screen_init = screen_init,
    .screen_draw = screen_draw,
    .page_turn = page_turn,
    .page_key = page_key,
    .flags = OPUS_ENGINE_DRUM,
    .finished = finished,
    .keys = kit,
    .note_key = note_key,
    .group = group,
    .choke = choke,
};

int opl_init(struct opl_self *self)
{
    plugin = self;
    opus_art_text(self, "panel.svg", panel_svg, sizeof panel_svg - 1);
    hits_engine_mark = opus_engine_add(self, &engine);
    if (hits_engine_mark < 0)
        return -1;
    set_decimate("decimate_scope", opus_setting(self, "decimate_scope"));
    set_decimate("decimate_depth", opus_setting(self, "decimate_depth"));
    opus_on_setting(self, set_decimate);
    opus_on_post_fx(self, drum_bus);
    opus_on_block_end(self, finish_kick_block);
    opus_on_key(self, hits_keyboard);
    opus_on_main_pass(self, connect_visible_grip);
    return 0;
}
