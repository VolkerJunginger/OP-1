#include "opus_plugin.h"
#include "op1fw.h"
#include "../Hits/mod_link.h"
#include "panel.h"

enum {
    LFO_COUNT = 2, OSC_COUNT = HITS_MOD_PADS * LFO_COUNT,
    TARGETS_PER_WORD = 4, PARAM_COUNT = OSC_COUNT + OSC_COUNT / TARGETS_PER_WORD,
    RATE_COUNT = 64
};
enum wave { SINE, TRIANGLE, RAMP_UP, RAMP_DOWN, SQUARE, WAVE_COUNT };
static const char *const waveform_target_names[TARGET_COUNT] = {
    "length", "level", "pitch", "timbre", "color", "low cut", "high cut"
};

static const char *const target_names[TARGET_COUNT] = {
    "len", "lvl", "pitch", "timb", "col", "lo", "hi"
};

struct setting {
    unsigned rate, depth, wave, target;
};

struct oscillator {
    unsigned phase, step;
    unsigned char rate;
};

struct clock_state {
    struct oscillator osc[OSC_COUNT];
    unsigned last_block;
    unsigned started;
};

static const unsigned short periods[RATE_COUNT] = {
    20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95,
    100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300, 320, 340, 360, 380, 400,
    420, 450, 480, 510, 540, 570, 600, 650, 700, 750, 800, 850, 900, 950, 1000, 1100,
    1200, 1350, 1500, 1650, 1800, 2000, 2300, 2600, 3000, 3500, 4000, 4750, 5500, 6500, 8000, 10000
};

static const short quarter_sine[33] = {
    0, 1608, 3212, 4808, 6393, 7962, 9512, 11039,
    12539, 14010, 15446, 16846, 18204, 19519, 20787, 22005,
    23170, 24279, 25329, 26319, 27245, 28105, 28898, 29621,
    30273, 30852, 31356, 31785, 32137, 32412, 32609, 32728,
    32767,
};

#define WORD(rate, depth, wave, target) ((rate) | ((depth) << 6) | ((wave) << 11) | ((target) << 14))
#define DEFAULT_PAIR WORD(32, 0, SINE, LENGTH), WORD(43, 0, TRIANGLE, LEVEL)
static const short defaults[PARAM_COUNT] = {
    DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR,
    DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR,
    DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR,
    DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR, DEFAULT_PAIR
};
#undef DEFAULT_PAIR

static struct opl_self *plugin;
static const struct opus_lfo_type type;
static unsigned block_clock;
static struct hits_mod_delta deltas[HITS_MOD_PADS];
/* FLUKE owns the edited pad. Only the A/B selection is local to FLUX. */
static unsigned char active_lfo[8];

static struct setting unpack(short value)
{
    unsigned word = (unsigned short)value;
    unsigned wave = (word >> 11) & 7u;
    return (struct setting){word & 63u, (word >> 6) & 31u,
                            wave < WAVE_COUNT ? wave : SINE, (word >> 14) & 1u};
}

static struct setting read_setting(const volatile short *params, unsigned index)
{
    struct setting setting = unpack(params[index]);
    unsigned shift = 3 * (index % TARGETS_PER_WORD);
    unsigned target = ((unsigned)params[OSC_COUNT + index / TARGETS_PER_WORD] >> shift) & 7u;
    if (target)
        setting.target = target - 1;
    return setting;
}

static void edit_target(volatile short *params, unsigned index, unsigned target)
{
    /* Zero means use the target in the original FLUX 1.1 word. */
    unsigned shift = 3 * (index % TARGETS_PER_WORD);
    volatile short *word = &params[OSC_COUNT + index / TARGETS_PER_WORD];
    *word = (short)(((unsigned short)*word & ~(7u << shift)) | ((target + 1) << shift));
}

static int wave_value(unsigned wave, unsigned phase)
{
    unsigned half = phase >> 31;
    unsigned ramp = phase >> 16;
    switch (wave) {
    case TRIANGLE: {
        unsigned x = (phase & 0x7fffffffu) >> 15;
        unsigned y = x < 32768 ? x : 65536 - x;
        int value = (int)(y < 32768 ? y : 32767);
        return half ? -value : value;
    }
    case RAMP_UP:   return (int)(phase >> 17) * 2 - 32767;
    case RAMP_DOWN: return 32767 - (int)(phase >> 17) * 2;
    case SQUARE:    return half ? -32767 : 32767;
    default: {
        unsigned x = ramp & 32767u;
        if (x >= 16384)
            x = 32768 - x;
        unsigned i = x >> 9;
        int value = quarter_sine[i];
        if (i < 32)
            value += (quarter_sine[i + 1] - value) * (int)(x & 511u) >> 9;
        return half ? -value : value;
    }
    }
}

static unsigned lfo_phase_step(unsigned rate)
{
    /* 2^32 * 64000 / (44100 * milliseconds). Scale before division. */
    const unsigned scaled = (unsigned)((4294967296ULL * 64000) / (44100 * 4));
    return (scaled / periods[rate]) << 2;
}

static void advance_clock(void)
{
    ++block_clock;
}

static void tick(struct opus_lfo *lfo)
{
    if (lfo->instrument != OPUS_FX_DRUM)
        return;

    struct clock_state *clock = lfo->state;
    if (clock->started && clock->last_block == block_clock)
        return;
    unsigned elapsed = clock->started ? block_clock - clock->last_block : 1;
    for (unsigned pad = 0; pad < HITS_MOD_PADS; ++pad) {
        int offsets[TARGET_COUNT] = {0};
        for (unsigned index = 0; index < LFO_COUNT; ++index) {
            unsigned param = pad * LFO_COUNT + index;
            struct setting setting = read_setting(lfo->params, param);
            struct oscillator *osc = &clock->osc[param];
            unsigned step = osc->step;
            if (!step || osc->rate != setting.rate) {
                osc->step = lfo_phase_step(setting.rate);
                osc->rate = (unsigned char)setting.rate;
            }
            osc->phase += (elapsed - 1) * step + osc->step;
            if (setting.depth) {
                int delta = wave_value(setting.wave, osc->phase) * (int)setting.depth / 31;
                offsets[setting.target] += delta;
            }
        }
        for (unsigned target = 0; target < TARGET_COUNT; ++target) {
            int value = offsets[target];
            deltas[pad].value[target] = (short)(value < -32767 ? -32767 : value > 32767 ? 32767 : value);
        }
    }
    clock->started = 1;
    clock->last_block = block_clock;
    hits_mod_publish_v2(lfo->slot, deltas);
}

static int apply(struct opus_lfo *lfo)
{
    /* Pass 3 precedes the voice render. The regular tick can arrive
     * later (pass 4) and keeps the clock running on silent blocks. */
    if (lfo->pass == 3)
        tick(lfo);
    return 0;
}

static int is_mod_page(const struct opus_page *page)
{
    return page->kind == OPUS_PAGE_LFO && page->where == OPUS_FX_DRUM &&
           page->module == &type && page->on && (unsigned)page->slot < 8 &&
           page->par && page->n >= PARAM_COUNT && hits_is_selected();
}

static int flux_keyboard(int key, int pressed)
{
    struct opus_page page;
    if (opus_page_now(&page) && is_mod_page(&page))
        hits_edit_key(key, pressed);
    return 0; /* Preserve the OS drum-key selection and release. */
}

static int current_hits_pad(void)
{
    short values[8];
    int pad = opus_drum_key(values);
    return (unsigned)pad < HITS_MOD_PADS ? pad : 0;
}

static int control_pad(void)
{
    if (hits_fluke_hold_state() < 0)
        return current_hits_pad();
    int pad;
    hits_mod_keyboard(&pad);
    return (unsigned)pad < HITS_MOD_PADS ? pad : -1;
}

static int select_lfo(struct opus_page *page, int key, int pressed)
{
    if (key == HITS_FLUKE_CONNECT_KEY)
        return !pressed && page->module == &type && hits_connect_fluke(page->inst);
    if (fw_shift_held() || opus_fn_held() || !is_mod_page(page))
        return 0;
    if (key == 0x3e)
        return hits_fluke_hold_key(pressed);
    if (key != 0x3b)
        return 0;
    if (pressed && control_pad() >= 0)
        active_lfo[page->slot] ^= 1u;
    return 1;
}

static unsigned turn_value(unsigned value, int detents, int max)
{
    int next = (int)value + detents;
    return (unsigned)(next < 0 ? 0 : next > max ? max : next);
}

static int edit_lfo(struct opus_page *page, int encoder, int detents)
{
    if (fw_shift_held() || opus_fn_held() || !is_mod_page(page))
        return 0;
    if ((unsigned)encoder >= 4)
        return 0;
    int pad = control_pad();
    if (pad < 0)
        return 1;
    unsigned index = (unsigned)pad * LFO_COUNT + active_lfo[page->slot];
    struct setting setting = unpack(page->par[index]);
    switch (encoder) {
    case 0: setting.wave = turn_value(setting.wave, detents, WAVE_COUNT - 1); break;
    case 1: setting.rate = turn_value(setting.rate, detents, RATE_COUNT - 1); break;
    case 2: setting.depth = turn_value(setting.depth, detents, 31); break;
    case 3:
        setting = read_setting(page->par, index);
        edit_target(page->par, index, turn_value(setting.target, detents, TARGET_COUNT - 1));
        return 1;
    default: return 0;
    }
    page->par[index] = (short)WORD(setting.rate, setting.depth, setting.wave, setting.target);
    return 1;
}

#define FLUX_RED OPUS_RGB(0xff423f)
#define FLUX_GREY OPUS_RGB(0xb7b7b7)

static void logo(void)
{
    static const unsigned char pixels[4][7] = {
        {31,16,16,30,16,16,16}, {16,16,16,16,16,16,31},
        {17,17,17,17,17,17,14}, {17,17,10,4,10,17,17}
    };
    for (int layer = 1; layer >= 0; --layer)
        for (int letter = 0; letter < 4; ++letter)
            for (int row = 0; row < 7; ++row)
                for (int column = 0; column < 5; ++column)
                    if (pixels[letter][row] & (16 >> column)) {
                        int x = 8 + letter * 12 + column * 2 + layer;
                        int y = 7 + row * 2 + 2 * layer;
                        opus_page_fill(x, y, x + 1, y + 1,
                                       OPUS_RGB(layer ? 0x8a3819 : 0xffbc43));
                    }
}

static void keyboard(unsigned lanes, int lane_edit, unsigned pad, int show_depth, const volatile short *settings)
{
    /* FLUKE's F-based key positions rounded to the page fill grid. */
    static const unsigned char x[12] = {0,4,7,11,14,18,21,28,32,35,39,42};
    const unsigned black = (1u << 1) | (1u << 3) | (1u << 5) | (1u << 8) | (1u << 10);
    for (unsigned layer = 0; layer < 2; ++layer) {
        for (unsigned i = 0; i < HITS_MOD_PADS; ++i) {
            unsigned dark = (black >> (i % 12)) & 1u;
            if (dark != layer)
                continue;
            int left = 111 + x[i % 12] + (int)(i / 12) * 49;
            int right = left + 5;
            int bottom = dark ? 20 : 29;
            int colour = dark ? OPUS_RGB(0x101010) : OPUS_RGB(0xfff0cf);
            if (lanes & (1u << i))
                colour = OPUS_RGB(0x08cc83);
            if ((int)i == lane_edit || i == pad)
                colour = FLUX_RED;
            if (dark)
                opus_page_frame(left, 8, right, bottom, OPUS_RGB(0xfff0cf));
            opus_page_fill(left + (int)dark, 8 + (int)dark,
                           right - (int)dark, bottom - (int)dark, colour);
            if (i == pad && (int)i != lane_edit)
                opus_page_frame(left, 8, right, bottom, FLUX_RED);
            unsigned index = i * LFO_COUNT;
            if (show_depth && (unpack(settings[index]).depth || unpack(settings[index + 1]).depth))
                opus_page_fill(left + 2, bottom - 2, left + 3, bottom - 1, OPUS_RGB(0xffbc43));
        }
    }
}

static void perspective_grid(void)
{
    int colour = OPUS_RGB(0x3c294c);
    for (int i = 0; i < 9; ++i)
        opus_page_line(83 + i * 19, 31, 8 + i * 38, 91, colour);
    for (int i = 0; i < 4; ++i)
        opus_page_line(8, 45 + i * 14, 311, 45 + i * 14, OPUS_RGB(0x362740));
    opus_page_line(8, 91, 311, 91, OPUS_RGB(0x83235e));
}

static void edit_brackets(int y)
{
    for (int i = 0; i < 2; ++i) {
        int x = i ? 31 : 16;
        int inner = i ? x - 3 : x + 3;
        opus_page_line(x, y - 7, inner, y - 7, FLUX_RED);
        opus_page_line(x, y + 7, inner, y + 7, FLUX_RED);
        opus_page_line(x, y - 7, x, y - 4, FLUX_RED);
        opus_page_line(x, y + 7, x, y + 4, FLUX_RED);
    }
}

static void waveform(unsigned wave, int y, int colour)
{
    /* Two cycles, using the same waveform function as the audio engine. */
    int previous = wave_value(wave, 0) * 8 / 32767;
    for (unsigned i = 1; i <= 48; ++i) {
        unsigned phase = (unsigned)((i % 24) * 0x100000000ULL / 24);
        int next = wave_value(wave, phase) * 8 / 32767;
        opus_page_line(43 + (int)(i - 1) * 4, y - previous,
                       43 + (int)i * 4, y - next, colour);
        previous = next;
    }
}

static void *art_node, *prompt_node, *keyboard_node, *pad_nodes[HITS_MOD_PADS], *depth_nodes[HITS_MOD_PADS];
static void *wave_nodes[LFO_COUNT][WAVE_COUNT], *bracket_nodes[LFO_COUNT];
static void *red_nodes[HITS_MOD_PADS];
static unsigned char *wave_strokes[LFO_COUNT][WAVE_COUNT];

static void show(void *node, int visible)
{
    if (node)
        ((unsigned char *)node)[0x50] = (unsigned char)!!visible;
}

static void paint(unsigned char *rgba, unsigned rgb)
{
    if (rgba) {
        rgba[0] = (unsigned char)(rgb >> 16);
        rgba[1] = (unsigned char)(rgb >> 8);
        rgba[2] = (unsigned char)rgb;
    }
}

static void flux_screen_init(void *screen)
{
    if (!screen)
        return;
    void *document = *(void **)screen;
    if (!document)
        return;
    char id[12];
    art_node = fw_svg_node_find(document, "fluxart", 0);
    keyboard_node = fw_svg_node_find(document, "drumkeyboard", 0);
    prompt_node = fw_svg_node_find(document, "selectsound", 0);
    for (int pad = 0; pad < HITS_MOD_PADS; ++pad) {
        fw_snprintf(id, sizeof id, "dk%d", pad);
        pad_nodes[pad] = fw_svg_node_find(document, id, 0);
        fw_snprintf(id, sizeof id, "red%d", pad);
        red_nodes[pad] = fw_svg_node_find(document, id, 0);
        fw_snprintf(id, sizeof id, "mod%d", pad);
        depth_nodes[pad] = fw_svg_node_find(document, id, 0);
    }
    for (int lane = 0; lane < LFO_COUNT; ++lane) {
        fw_snprintf(id, sizeof id, "edit%d", lane);
        bracket_nodes[lane] = fw_svg_node_find(document, id, 0);
        for (int wave = 0; wave < WAVE_COUNT; ++wave) {
            fw_snprintf(id, sizeof id, "wave%d%d", lane, wave);
            wave_nodes[lane][wave] = fw_svg_node_find(document, id, 0);
            wave_strokes[lane][wave] = wave_nodes[lane][wave] ? fw_screen_line_style(screen, id) : 0;
        }
    }
}

static void native_art(void *screen, int valid, int prompt, int pad, unsigned lanes, int show_depth, int show_keyboard,
                       const struct opus_page *page)
{
    show(art_node, valid);
    show(prompt_node, prompt);
    show(keyboard_node, show_keyboard);
    if (valid) {
        unsigned active = active_lfo[page->slot];
        for (unsigned i = 0; i < HITS_MOD_PADS; ++i) {
            show(pad_nodes[i], (int)i != pad && (lanes & (1u << i)));
            show(red_nodes[i], (int)i == pad);
            unsigned index = i * LFO_COUNT;
            show(depth_nodes[i], show_depth && (unpack(page->par[index]).depth || unpack(page->par[index + 1]).depth));
        }
        for (unsigned lane = 0; lane < LFO_COUNT; ++lane) {
            show(bracket_nodes[lane], pad >= 0 && lane == active);
            unsigned current_wave = WAVE_COUNT;
            if (pad >= 0) {
                current_wave = unpack(page->par[(unsigned)pad * LFO_COUNT + lane]).wave;
                paint(wave_strokes[lane][current_wave], lane == active ? 0xff423f : 0x88a4ff);
            }
            for (unsigned wave = 0; wave < WAVE_COUNT; ++wave)
                show(wave_nodes[lane][wave], wave == current_wave);
        }
    }
    fw_screen_draw(screen);
}

struct native_text {
    unsigned object[41];
    unsigned char rgba[4];
    char shown[40];
    int ready;
};

static void flux_label(void *raster, int slot, int x, int y, int size, int colour,
                  int right, const char *value)
{
    if (!raster) {
        if (right)
            opus_page_text_right(plugin, slot, x, y, size, colour, value);
        else
            opus_page_text(plugin, slot, x, y, size, colour, value);
        return;
    }

    /* Separate text objects for active and inactive row colours. */
    static struct native_text texts[25];
    int index = slot;
    if ((slot == 3 || slot == 4 || slot == 6 || slot == 7) && colour == FLUX_RED)
        index = 19 + (slot >= 6 ? 2 : 0) + (slot == 4 || slot == 7);
    struct native_text *text = &texts[index];
    if (!text->ready) {
        paint(text->rgba, (unsigned)colour);
        text->rgba[3] = 255;
        fw_text_new(text->object, x * 1024, y, size, text->rgba, right ? 2 : 0);
        text->ready = 1;
    }
    unsigned i = 0;
    while (i + 1 < sizeof text->shown && value[i] && value[i] == text->shown[i])
        ++i;
    if (value[i] != text->shown[i]) {
        while (i + 1 < sizeof text->shown && value[i]) {
            text->shown[i] = value[i];
            ++i;
        }
        text->shown[i] = 0;
        fw_set_text(text->object, text->shown);
    }
    /* Same lower-band transform as FLUKE's native text. */
    int scale = ((slot >= 9 && slot <= 17) || slot >= 23) ? 983 : 1024;
    const int matrix[9] = {scale,0,0,0,scale,0,x*(1024-scale),146*(1024-scale),1024};
    fw_text_draw(text->object, raster, matrix);
}

static void draw(void *screen)
{
    static const char *const notes[] = {"c","c#","d","d#","e","f","f#","g","g#","a","a#","b"};
    static const char *const waves[] = {"sine", "tri", "ramp+", "ramp-", "square"};
    static const char *const headers[] = {"wave", "rate ms", "depth", "target"};
    static const int colours[] = {
        OPUS_RGB(0x698eff), OPUS_RGB(0x00ed95), OPUS_RGB(0xcdcdff), FLUX_RED
    };
    struct opus_page page;
    int valid = opus_page_now(&page) && is_mod_page(&page);
    int needs_hits = !valid && !hits_is_selected();
    int hold = valid ? hits_fluke_hold_state() : -1;
    int linked = hold >= 0;
    int pad = valid && !linked ? current_hits_pad() : -1;
    unsigned lanes = linked ? hits_mod_keyboard(&pad) : 0;
    if ((unsigned)pad >= HITS_MOD_PADS)
        pad = -1;
    void *raster = screen ? fw_screen_raster() : 0;
    if (screen) {
        native_art(screen, valid, needs_hits || (linked && pad < 0), pad, lanes, hold >= 0, linked, &page);
        if (!raster)
            return;
    } else {
        opus_page_fill(0, 2, 319, 146, OPUS_RGB(0x0c0913));
        opus_page_fill(0, 95, 319, 146, OPUS_RGB(0x0a0910));
        logo();
    }
    if (needs_hits) {
        if (!screen)
            opus_page_fill(29, 61, 291, 86, OPUS_RGB(0x000000));
        flux_label(raster, 1, 78, 67, 11, FLUX_GREY, 0, "this device needs hits");
    }
    if (!valid)
        return;

    if (hold >= 0)
        flux_label(raster, hold ? 23 : 24, 242, 128, 9,
                   hold ? FLUX_RED : OPUS_RGB(0x661a19), 0, "hold");

    if (!screen) {
        if (linked)
            keyboard(lanes, pad, (unsigned)pad, hold >= 0, page.par);
        perspective_grid();
    }
    if (pad < 0) {
        if (!linked)
            return;
        if (!screen)
            opus_page_fill(29, 61, 291, 86, OPUS_RGB(0x000000));
        flux_label(raster, 18, 50, 67, 11, FLUX_GREY, 0, "select sound with shift + note");
        return;
    }

    unsigned active = active_lfo[page.slot];
    int note = 53 + pad;
    char value[20];
    fw_snprintf(value, sizeof value, linked ? "edit %s%d" : "%s%d", notes[note % 12], note / 12 - 1);
    flux_label(raster, 2, 311, 9, 12, FLUX_RED, 1, value);

    for (unsigned index = 0; index < LFO_COUNT; ++index) {
        int y = 48 + (int)index * 27;
        int slot = 3 + (int)index * 3;
        int selected = index == active;
        struct setting setting = read_setting(page.par, (unsigned)pad * LFO_COUNT + index);
        if (!screen) {
            if (selected)
                edit_brackets(y);
            waveform(setting.wave, y, selected ? FLUX_RED : OPUS_RGB(0x88a4ff));
        }
        flux_label(raster, slot, 19, y - 6, 11, selected ? FLUX_RED : FLUX_GREY, 0,
                       index ? "b" : "a");
        flux_label(raster, slot + 1, 252, y - 10, 9, selected ? FLUX_RED : colours[2], 0,
                   waveform_target_names[setting.target]);
    }

    struct setting setting = read_setting(page.par, (unsigned)pad * LFO_COUNT + active);
    char rate[12], depth[12];
    fw_snprintf(rate, sizeof rate, "%d", (int)periods[setting.rate]);
    fw_snprintf(depth, sizeof depth, "%d%%", (int)((setting.depth * 100 + 15) / 31));
    const char *values[] = {waves[setting.wave], rate, depth, target_names[setting.target]};
    for (int i = 0; i < 4; ++i) {
        flux_label(raster, 9 + i, 8 + i * 78, 93, 11, colours[i], 0, headers[i]);
        flux_label(raster, 13 + i, 8 + i * 78, 107, 18, colours[i], 0, values[i]);
    }
    flux_label(raster, 17, 8, 128, 9, OPUS_RGB(0x9baee0), 0, "a/b");
}

static void page_draw(void) { draw(0); }

static void flux_screen_draw(void *screen, int dt)
{
    (void)dt;
    if (screen)
        draw(screen);
}

static const struct opus_lfo_type type = {
    .size = sizeof type,
    .name = "flux",
    .nparams = PARAM_COUNT,
    .state_size = sizeof(struct clock_state),
    .defaults = defaults,
    .tick = tick,
    .apply = apply,
    .page = page_draw,
    .svg = "/yaffs2/mod/esc.hitsmod/panel.svg",
    .screen_init = flux_screen_init,
    .screen_draw = flux_screen_draw,
    .page_key = select_lfo,
    .page_turn = edit_lfo
};

int opl_init(struct opl_self *self)
{
    plugin = self;
    opus_art_text(self, "panel.svg", flux_panel_svg, sizeof flux_panel_svg - 1);
    if (!opus_lfo_register(self, &type))
        return -1;
    opus_on_block_end(self, advance_clock);
    opus_on_key(self, flux_keyboard);
    return 0;
}
