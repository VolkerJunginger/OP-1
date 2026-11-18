#include "opus_plugin.h"
#include "op1fw.h"
#include "tables.h"
#include "../Hits/grip_link.h"

enum { BLOCK = 64, UNITY_Q20 = 1 << 20 };

struct comp_state {
    int reduction;
    unsigned detector_power;
    int meter_in, meter_out, meter_gr;
    volatile unsigned levels;
};

struct controls {
    int threshold, slope, attack, release, makeup, knee, knee_scale, range, dry;
};

static unsigned meter_clock;
static volatile int sidechain_enabled;
static volatile int hits_selected;
static const struct hits_grip_link *volatile hits_link;
static const int knees[] = { 6, 8, 12, 0, 2, 4 };
static const int knee_scales[] = { 1365, 1024, 683, 0, 4096, 2048 };
static const int ranges[] = { 32, 3, 6, 9, 12, 18, 24 };
static const unsigned char ratios[] = { 1, 2, 3, 4, 6, 8, 12, 20 };
static const unsigned short slopes[] = { 0, 128, 171, 192, 213, 224, 235, 243 };

static int index_of(int value, int max)
{
    if (value < 0)
        return 0;
    value >>= 10;
    return value > max ? max : value;
}

static struct controls decode(const short *par)
{
    struct controls p;
    p.threshold = index_of(par[0], 30) * 64;
    p.slope = slopes[index_of(par[1], 7)];
    p.attack = attack_coef[index_of(par[2], 9)];
    p.release = release_coef[index_of(par[3], 9)];
    p.makeup = makeup_q12[index_of(par[4], 12)];
    const int knee = index_of(par[5], 5);
    p.knee = knees[knee] * 32;
    p.knee_scale = knee_scales[knee];
    p.range = ranges[index_of(par[6], 6)] * 64;
    p.dry = index_of(par[7], 20) * 410;
    if (p.dry > 8192)
        p.dry = 8192;
    return p;
}

static inline __attribute__((always_inline)) int level_loss(int peak)
{
    const int i = peak >> 7;
    int loss = loss_db64[i];
    if (i < 256)
        loss -= ((loss - loss_db64[i + 1]) * (peak & 127)) >> 7;
    return loss;
}

static int reduction_at(int loss, const struct controls *p)
{
    int over = p->threshold - loss;
    if (over <= -p->knee)
        return 0;
    if (over < p->knee) {
        const unsigned pos = (unsigned)(over + p->knee);
        over = (int)((pos * pos * (unsigned)p->knee_scale + (1u << 19)) >> 20);
    }
    const int reduction = (over * p->slope + 128) >> 8;
    return reduction < p->range ? reduction : p->range;
}

/* Integer square root of the linked mean-square detector. */
static unsigned rms_amplitude(unsigned power)
{
    unsigned root = 0;
    for (unsigned bit = 1u << 30; bit; bit >>= 2) {
        if (power >= root + bit) {
            power -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
    }
    return root;
}

static inline __attribute__((always_inline)) int gain_for_reduction(int reduction)
{
    if (reduction >= 32 * 64)
        return gain_q15[256] << 5;
    const int bin = reduction >> 3;
    const int fraction = reduction & 7;
    return (gain_q15[bin] << 5)
         - ((gain_q15[bin] - gain_q15[bin + 1]) * fraction << 2);
}

static inline __attribute__((always_inline))
short mix_saturated(int sample, int wet_gain, int dry)
{
    if (dry == 8192)
        return (short)sample;

    const int wet = (sample * wet_gain + 4096) >> 13;
    const int magnitude = wet < 0 ? -wet : wet;
    int shaped = wet;
    /* Unity below roughly half scale, then a symmetric soft knee with a flat ceiling. */
    if (magnitude > 15872) {
        const int excess = magnitude - 15872;
        const int bend = excess < 32768 ? excess : 32768;
        const int rounded = 15872 + bend - ((bend * bend + 32768) >> 16);
        shaped = wet < 0 ? -rounded : rounded;
    }
    return (short)(shaped + (((sample - shaped) * dry + 4096) >> 13));
}

/* Block-rate meter ballistics, about 185 ms up and 740 ms down at 44.1 kHz. */
static int meter_follow(int current, int peak)
{
    const int target = peak << 6;
    const int delta = target - current;
    return current + (delta >= 0 ? (delta + 127) >> 7 : -((-delta + 511) >> 9));
}

static inline __attribute__((always_inline))
void process(struct comp_state *state, short *left, short *right, const short *par, int stereo)
{
    const struct controls p = decode(par);
    unsigned sum = 0;
    for (int i = 0; i < BLOCK; ++i) {
        const int l = left[i] >> 3;
        const int r = stereo ? right[i] >> 3 : l;
        sum += (unsigned)(l * l + r * r);
    }
    const unsigned block_power = sum >> 7;
    /* About 5 ms of averaging at 44.1 kHz. */
    state->detector_power = (state->detector_power * 3 + block_power) >> 2;
    unsigned amplitude = rms_amplitude(state->detector_power) << 3;
    if (!stereo && sidechain_enabled && hits_selected)
        amplitude = hits_link->kick_peak();
    const int target = reduction_at(level_loss((int)amplitude), &p) << 10;
    int reduction = state->reduction;
    const int delta = target - reduction;
    /* Move faster into larger gain changes and recover at a steady dB rate.
     * Existing Attack and Release settings scale these rates. */
    const int drive = delta > 0 ? (delta >> 10) + 128 : 0;
    const int boost = drive > 1280 ? 3 : drive > 640 ? 2 : 1;
    const int attack = ((drive * p.attack + 31) >> 5) * boost;
    const int release = p.release * 20;
    const int step = delta > 0 ? attack : -release;
    int gain = gain_for_reduction(reduction >> 10);
    int block_peak = 0, output_peak = 0, min_gain = gain;

    for (int i = 0; i < BLOCK; ++i) {
        const int l = left[i];
        int peak = l < 0 ? -l : l;
        int r = 0;
        if (stereo) {
            r = right[i];
            const int other = r < 0 ? -r : r;
            if (other > peak)
                peak = other;
        }
        if (peak > block_peak)
            block_peak = peak;
        reduction += step;
        if (step > 0 ? reduction > target : reduction < target)
            reduction = target;
        gain = gain_for_reduction(reduction >> 10);
        const int wet_gain = (((gain + 16) >> 5) * p.makeup + 8192) >> 14;
        left[i] = mix_saturated(l, wet_gain, p.dry);
        int out = left[i] < 0 ? -left[i] : left[i];
        if (stereo) {
            right[i] = mix_saturated(r, wet_gain, p.dry);
            const int other = right[i] < 0 ? -right[i] : right[i];
            if (other > out)
                out = other;
        }
        if (out > output_peak)
            output_peak = out;
        if (gain < min_gain)
            min_gain = gain;
    }
    state->reduction = reduction;
    state->meter_in = meter_follow(state->meter_in, block_peak);
    state->meter_out = meter_follow(state->meter_out, output_peak);
    state->meter_gr = meter_follow(state->meter_gr, (UNITY_Q20 - min_gain) >> 5);
    /* Three 10-bit amplitudes in one atomic word, plus a validity bit. */
    state->levels = (1u << 30) | ((unsigned)(state->meter_in >> 12) << 20)
                | ((unsigned)(state->meter_out >> 12) << 10) | (unsigned)(512 - (state->meter_gr >> 12));
}

static void need(int stereo, struct opus_need *n)
{
    (void)stereo;
    n->state = sizeof(struct comp_state);
}

static void init(struct opus_fx *fx)
{
    struct comp_state *state = fx->state;
    state->reduction = 0;
    state->detector_power = 0;
    state->meter_in = 0;
    state->meter_out = 0;
    state->meter_gr = 0;
    state->levels = 0;
}

static void mono(struct opus_fx *fx, short *buf, const short *par)
{
    process(fx->state, buf, 0, par, 0);
}

static void stereo(struct opus_fx *fx, short *left, short *right, const short *par)
{
    process(fx->state, left, right, par, 1);
}

static int shift_knob(short *par, int encoder, int detents)
{
    static const unsigned char slot[] = {5, 6, 4, 7};
    static const unsigned char max[] = {5, 6, 12, 20};
    if ((unsigned)encoder >= 4)
        return 0;
    const int k = slot[encoder];
    int index = index_of(par[k], max[encoder]);
    if (encoder == 0) {
        /* Stored zero means 6 dB for presets from 0.1.0. */
        const int ordered = (index + 3) % 6 + detents;
        index = ((ordered < 0 ? 0 : ordered > 5 ? 5 : ordered) + 3) % 6;
        par[k] = (short)(index << 10);
        return 1;
    }
    index += encoder == 3 ? -detents : detents;
    index = index < 0 ? 0 : index > max[encoder] ? max[encoder] : index;
    par[k] = (short)(index << 10);
    return 1;
}

#include "art.h"

static void advance_clock(void)
{
    ++meter_clock;
}

static void check_hits(void)
{
    hits_selected = hits_link && hits_link->selected();
}

static const struct opus_effect effect;
#include "../Hits/encoder_speed.h"
static struct encoder_speed grip_speed;
static void grip_expire(void) { grip_speed.burst = 0; }


static int kick_sc_available(const struct opus_page *page, int hits)
{
    return page->where != OPUS_FX_MASTER && hits;
}

static int page_key(struct opus_page *pg, int key, int pressed)
{
    if (key == HITS_GRIP_CONNECT_KEY) {
        if (pressed || pg->module != &effect || !pg->inst)
            return 0;
        const struct hits_grip_link *link = pg->inst;
        if (!link->kick_peak || !link->selected || !link->hold_state ||
            !link->hold_key || !link->connect_fluke)
            return 0;
        hits_link = link;
        check_hits();
        return 1;
    }
    if (key == HITS_FLUKE_CONNECT_KEY)
        return !pressed && pg->module == &effect && hits_link &&
               hits_link->connect_fluke(pg->inst);
    if (!hits_link)
        return 0;
    if (!pg->on || fw_shift_held() || opus_fn_held())
        return 0;
    if (key == 0x3e)
        return hits_link->hold_key(pressed);
    if (key != 0x3d || !kick_sc_available(pg, hits_link->selected()))
        return 0;
    if (pressed)
        sidechain_enabled = !sidechain_enabled;
    return 1;
}

static int level_segments(int loss)
{
    return loss >= 48 * 64 ? 0 : 21 - (loss * 21 / (48 * 64));
}

static const int identity[9] = {1024,0,0,0,1024,0,0,0,1024};
static unsigned shape[13];

static void native_fill(int x, int y, int right, int bottom, unsigned color)
{
    const unsigned char rgba[] = {color >> 16, color >> 8, color, 255};
    struct vector { unsigned unused; const void *first, *last, *end; };
    const struct vector fills = {0, rgba, rgba + 4, rgba + 4}, lines = {0};
    const int pts[][2] = {{x*1024,y*1024},{(right+1)*1024,y*1024},
                          {(right+1)*1024,(bottom+1)*1024},{x*1024,(bottom+1)*1024}};
    fw_path_clear(shape);
    fw_path_move_to(shape, pts[0], 1);
    for (int i = 1; i < 4; ++i)
        fw_path_line_to(shape, pts[i], 1);
    fw_path_close(shape);
    fw_path_finish(shape, 1, 0);
    fw_draw_path(fw_screen_raster(), 0, &fills, &lines, shape, identity, 0, 0);
}

static void draw_text(int id, int x, int y, int size, unsigned color, const char *value, int align)
{
    static struct { unsigned object[41]; unsigned char rgba[4]; char text[24]; } slots[19];
    if (id == 1 && color == OPUS_RGB(0x661a19))
        id = 16;
    if (!slots[id].rgba[3]) {
        slots[id].rgba[0] = color >> 16;
        slots[id].rgba[1] = color >> 8;
        slots[id].rgba[2] = color;
        slots[id].rgba[3] = 255;
        fw_text_new(slots[id].object, x*1024, y, size, slots[id].rgba, align);
    }
    int i = 0;
    while (value[i] && slots[id].text[i] == value[i])
        ++i;
    if (slots[id].text[i] != value[i]) {
        while ((slots[id].text[i] = value[i]) != 0)
            ++i;
        fw_set_text(slots[id].object, slots[id].text);
    }
    /* FLUKE scales the parameter band around y=146, anchoring each column. */
    const int lower = id == 1 || id >= 8;
    const int matrix[9] = {983,0,0,0,983,0,x*41,146*41,1024};
    fw_text_draw(slots[id].object, fw_screen_raster(), lower ? matrix : identity);
}

#define native_text(self,id,x,y,size,color,value) draw_text(id,x,y,size,color,value,0)
#define native_text_right(self,id,x,y,size,color,value) draw_text(id,x,y,size,color,value,2)

static void screen_draw(void *screen, int dt)
{
    (void)dt;
    if (!screen)
        return;
    fw_screen_draw(screen);
    struct opus_page pg;
    if (!opus_page_now(&pg) || pg.kind != OPUS_PAGE_EFFECT)
        return;
    const int cream = OPUS_RGB(0xfff0cf);
    native_text_right(owner, 0, 311, 9, 12, cream,
                         pg.where == OPUS_FX_MASTER ? "master" : pg.where == OPUS_FX_DRUM ? "drum" : "synth");
    const struct opus_fx *fx = pg.inst;
    const struct comp_state *state = fx ? fx->state : 0;
    const unsigned levels = pg.on && state ? state->levels : 0;
    const int in = ((levels >> 20) & 1023) << 6;
    const int out = ((levels >> 10) & 1023) << 6;
    const int gr = levels ? level_loss((levels & 1023) << 6) : 0;
    const int count[] = {level_segments(level_loss(in)), (gr * 21 + 1024) / 2048,
                         level_segments(level_loss(out))};
    const int lit[] = {OPUS_RGB(0x00ed95), OPUS_RGB(0xff993c), OPUS_RGB(0x00ed95)};
    const char *const labels[] = {"in", "gr", "out"};
    static char numbers[3][12];
    static unsigned last;
    static const struct comp_state *last_state;
    static int was_live;
    const unsigned now = meter_clock;
    if (state != last_state || !!levels != was_live || now - last >= 345) {
        last = now;
        last_state = state;
        was_live = !!levels;
        const int db[] = {-((level_loss(in) + 32) / 64), (gr + 32) / 64,
                          -((level_loss(out) + 32) / 64)};
        for (int i = 0; i < 3; ++i) {
            if (!levels || (i == 0 && !in) || (i == 2 && !out))
                fw_snprintf(numbers[i], sizeof numbers[i], "--");
            else
                fw_snprintf(numbers[i], sizeof numbers[i], "%i db", db[i]);
        }
    }
    for (int row = 0; row < 3; ++row) {
        const int y = 35 + row * 17;
        native_text(owner, 2 + row, 20, y, 9, cream, labels[row]);
        native_text_right(owner, 5 + row, 304, y, 9, cream, numbers[row]);
        for (int i = 0; i < 21; ++i) {
            const int x = 47 + i * 10;
            const int active = row == 1 ? 20 - i < count[row] : i < count[row];
            native_fill(x, y, x + 8, y + 7, active ? lit[row] : OPUS_RGB(0x37293f));
        }
        if (row == 1)
            native_fill(256, y - 1, 257, y + 8, cream);
    }

    const short *par = pg.live;
    static const char *const normal[] = {"thresh", "ratio", "attack", "release"};
    static const char *const shifted[] = {"knee", "range", "makeup", "mix"};
    static char value[4][16];
    const int shift = fw_shift_held();
    if (shift) {
        fw_snprintf(value[0], 16, "%i db", knees[index_of(par[5], 5)]);
        if (par[6] < 1024)
            fw_snprintf(value[1], 16, "full");
        else
            fw_snprintf(value[1], 16, "%i db", ranges[index_of(par[6], 6)]);
        fw_snprintf(value[2], 16, "+%i db", index_of(par[4], 12));
        fw_snprintf(value[3], 16, "%i%%", 100 - index_of(par[7], 20) * 5);
    } else {
        fw_snprintf(value[0], 16, "%i db", -index_of(par[0], 30));
        fw_snprintf(value[1], 16, "%i:1", (int)ratios[index_of(par[1], 7)]);
        fw_snprintf(value[2], 16, "%i", (int)attack_ms[index_of(par[2], 9)]);
        fw_snprintf(value[3], 16, "%i", (int)release_ms[index_of(par[3], 9)]);
    }
    static const int palette[] = {OPUS_RGB(0x698eff), OPUS_RGB(0x00ed95),
                                  OPUS_RGB(0xcdcdff), OPUS_RGB(0xff423f)};
    if (kick_sc_available(&pg, hits_selected))
        native_text(owner, sidechain_enabled ? 17 : 18, 164, 128, 9,
                    sidechain_enabled ? OPUS_RGB(0xcdcdff) : OPUS_RGB(0x55556a),
                    "kick sc");
    int hold = hits_link ? hits_link->hold_state() : -1;
    if (hold >= 0)
        native_text(owner, 1, 242, 128, 9,
                    hold ? OPUS_RGB(0xff423f) : OPUS_RGB(0x661a19), "hold");
    for (int i = 0; i < 4; ++i) {
        const int x = 8 + i * 78;
        native_text(owner, 8 + i, x, 93, 11, palette[i], shift ? shifted[i] : normal[i]);
        native_text(owner, 12 + i, x, 107, 18, palette[i], value[i]);
    }
}

static struct opl_self *encoder_plugin;

static int page_turn(struct opus_page *pg, int encoder, int detents)
{
    if (!pg || pg->module != &effect || !pg->par || pg->n < 8 ||
        opus_fn_held() || (unsigned)encoder >= 4)
        return 0;
    if (!detents)
        return 1;
    if (fw_shift_held()) {
        static const unsigned char param[] = {5, 6, 4, 7};
        int k = param[encoder];
        if (encoder >= 2)
            detents = encoder_speed_turn(&grip_speed, encoder_plugin, grip_expire, pg->par, k, detents);
        else
            grip_expire();
        short values[8];
        values[k] = pg->par[k];
        shift_knob(values, encoder, detents);
        pg->par[k] = values[k];
        return 1;
    }
    if (encoder == 0 || encoder == 2 || encoder == 3)
        detents = encoder_speed_turn(&grip_speed, encoder_plugin, grip_expire, pg->par, encoder, detents);
    else
        grip_expire();
    int maximum = effect.limits[encoder][1];
    /* Match the stock 8192/8 step, including any fractional preset value. */
    if (detents > 32) detents = 32;
    if (detents < -32) detents = -32;
    int value = pg->par[encoder] + detents * 1024;
    pg->par[encoder] = (short)(value < 0 ? 0 : value > maximum ? maximum : value);
    return 1;
}

#define KNOB(max) { 0, (max) * 1024, 8192 }
static const struct opus_effect effect = {
    .size = sizeof(struct opus_effect),
    .name = "grip",
    .aliases = "cleancomp",
    .defaults = { 18 * 1024, 2 * 1024, 4 * 1024, 4 * 1024 },
    .limits = { KNOB(30), KNOB(7), KNOB(9), KNOB(9), { 0, 12 * 1024, 0 },
                { 0, 5 * 1024, 0 }, { 0, 6 * 1024, 0 }, { 0, 20 * 1024, 0 } },
    .need = need,
    .init = init,
    .mono = mono,
    .stereo = stereo,
    .svg = "/yaffs2/mod/esc.grip/grip-native.svg",
    .screen_draw = screen_draw,
    .page_key = page_key,
    .page_turn = page_turn,
};

int opl_init(struct opl_self *self)
{
    encoder_plugin = self;
    if (opus_art_text(self, "grip-native.svg", screen_svg, sizeof screen_svg - 1) < 0)
        return -1;
    if (opus_effect_register(self, &effect) < 0)
        return -1;
    fw_path_init(shape);
    opus_on_block_end(self, advance_clock);
    opus_on_main_pass(self, check_hits);
    return 0;
}
