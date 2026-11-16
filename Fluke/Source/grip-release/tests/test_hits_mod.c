#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define opl_init hits_init
#include "../Hits/braidslive.c"
#undef opl_init

static int selected = 1;
static int test_hits_is_selected(void) { return selected; }

#define hits_is_selected test_hits_is_selected
#define plugin mod_plugin
#define opl_init hits_mod_init
#include "../HitsMod/hitsmod.c"
#undef opl_init
#undef plugin
#undef hits_is_selected

static int slot_number = 2;
static unsigned fluke_state = (1u << 31) | 5;
static int shifted, fn_held, fluke_edited = 2;
static struct opus_page visible;
static const struct opus_lfo_type *registered;
static void (*mod_block_end)(void);
static int (*mod_key_event)(int, int);
static char text_slots[32][32];
static unsigned lines;
static unsigned lane_green, lane_red;

int fluke_drum_root(void) { return 60; }
int fluke_hits_cut(int pressed) { (void)pressed; return 0; }
int fluke_hits_hold(int pressed)
{
    if (pressed)
        fluke_state ^= 1u << 30;
    return 1;
}
static int current_drum_pad;
static short mock_pad_params[24][8];
int opus_drum_select_key(int key)
{
    assert(key >= 0 && key < 24);
    current_drum_pad = key;
    return 1;
}
int opus_drum_key(short par[8])
{
    memcpy(par, mock_pad_params[current_drum_pad], sizeof mock_pad_params[0]);
    return current_drum_pad;
}
int opus_set_setting(struct opl_self *self, const char *name, int value)
{ (void)self; (void)name; (void)value; return 1; }
void opus_page_bar(int x0, int y0, int x1, int y1, unsigned value, unsigned max, int colour)
{ assert(value <= max); opus_page_fill(x0, y0, x1, y1, colour); }
int opus_sound_slot(int instrument) { assert(instrument == OPUS_FX_DRUM); return slot_number; }
unsigned fluke_drum_keys(int *edited) { *edited = fluke_edited; return fluke_state; }
void fluke_hits_select_pad(int pad) { fluke_edited = pad; }
int fw_key_to_index(int key) { assert(key == 0x35 || key == 0x0d); return key == 0x35 ? 23 : 0; }
int opus_page_now(struct opus_page *page)
{
    *page = visible;
    if (page->kind == OPUS_PAGE_DRUM && page->module == &engine) {
        page->key = current_drum_pad;
        page->par = mock_pad_params[current_drum_pad];
        page->n = 8;
    }
    return 1;
}
const void *fw_json_get(const void *value, const char *key)
{ (void)value; (void)key; return 0; }
int fw_json_type(const void *value) { (void)value; return 0; }
int fw_json_as_int(const void *value) { (void)value; return 0; }
int fw_shift_held(void) { return shifted; }
int opus_fn_held(void) { return fn_held; }
int fw_snprintf(char *dst, unsigned size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int length = vsnprintf(dst, size, fmt, args);
    va_end(args);
    return length;
}
int opus_lfo_register(struct opl_self *self, const struct opus_lfo_type *lfo)
{
    (void)self;
    registered = lfo;
    return 1;
}
void opus_on_key(struct opl_self *self, int (*callback)(int, int))
{ (void)self; mod_key_event = callback; }
void opus_on_block_end(struct opl_self *self, void (*callback)(void))
{ (void)self; mod_block_end = callback; }
void opus_page_fill(int x0, int y0, int x1, int y1, int colour)
{
    assert(x0 >= 0 && x0 <= x1 && x1 <= 319 && y0 >= 2 && y0 <= y1 && y1 <= 146);
    lane_green += colour == OPUS_RGB(0x08cc83);
    lane_red += colour == OPUS_RGB(0xff423f);
}
void opus_page_frame(int x0, int y0, int x1, int y1, int colour)
{
    opus_page_fill(x0, y0, x1, y1, colour);

}
void opus_page_line(int x0, int y0, int x1, int y1, int colour)
{
    (void)colour;
    assert(x0 >= 0 && x0 <= 319 && x1 >= 0 && x1 <= 319);
    assert(y0 >= 2 && y0 <= 146 && y1 >= 2 && y1 <= 146);
    ++lines;
}
void opus_page_text(struct opl_self *self, int slot, int x, int y, int size, int colour, const char *text)
{
    (void)self;
    (void)colour;
    assert((unsigned)slot < 32 && x >= 0 && x < 320 && y >= 2 && y + size <= 147);
    snprintf(text_slots[slot], sizeof text_slots[slot], "%s", text);
}
void opus_page_text_right(struct opl_self *self, int slot, int x, int y, int size, int colour, const char *text)
{ opus_page_text(self, slot, x, y, size, colour, text); }

static unsigned char svg_nodes[87][0x51], svg_colours[87][4];
static unsigned native_draws, edit_labels, parameter_labels, parameter_values;

static unsigned svg_index(const char *id)
{
    unsigned index;
    if (sscanf(id, "dk%u", &index) == 1) { assert(index < 24); return 1 + index; }
    if (sscanf(id, "mod%u", &index) == 1) { assert(index < 24); return 25 + index; }
    if (sscanf(id, "wave%u", &index) == 1) {
        assert(index / 10 < 2 && index % 10 < 5);
        return 49 + (index / 10) * 5 + index % 10;
    }
    if (sscanf(id, "edit%u", &index) == 1) { assert(index < 2); return 59 + index; }
    if (!strcmp(id, "selectsound")) return 61;
    if (!strcmp(id, "drumkeyboard")) return 86;
    if (sscanf(id, "red%u", &index) == 1) { assert(index < 24); return 62 + index; }
    assert(!strcmp(id, "fluxart"));
    return 0;
}
void *fw_svg_node_find(void *document, const char *id, int warn)
{ assert(document && !warn); return svg_nodes[svg_index(id)]; }
void *fw_screen_fill(void *screen, const char *id)
{ assert(screen); return svg_colours[svg_index(id)]; }
void *fw_screen_line_style(void *screen, const char *id)
{ return fw_screen_fill(screen, id); }
void fw_screen_draw(void *screen) { assert(screen); ++native_draws; }
void *fw_screen_raster(void) { return &native_draws; }
void *fw_text_new(void *mem, int x, int y, int size, const unsigned char *rgba, int align)
{
    assert(rgba[3] == 255);
    if (y == 9) {
        assert(x == 311 * 1024 && size == 12 && align == 2);
        ++edit_labels;
    }
    if (y == 93 || y == 107) {
        assert(!align && (x / 1024 - 8) % 78 == 0);
        assert(size == (y == 93 ? 11 : 18));
        if (y == 93) ++parameter_labels;
        else ++parameter_values;
    }
    unsigned *text = mem;
    text[0] = (unsigned)x;
    text[1] = (unsigned)y;
    return mem;
}
void fw_set_text(void *text, const char *value) { assert(text && value); }
void fw_text_draw(void *text, void *raster, const int *matrix)
{
    unsigned *position = text;
    assert(raster == &native_draws);
    int scale = position[1] >= 93 ? 983 : 1024;
    assert(matrix[0] == scale && matrix[4] == scale);
    assert(matrix[6] == (int)(position[0] / 1024) * (1024 - scale));
    assert(matrix[7] == 146 * (1024 - scale));
}
int opus_art_text(struct opl_self *self, const char *name, const char *svg, unsigned len)
{
    assert(self && !strcmp(name, "panel.svg") && len == strlen(svg));
    assert(strstr(svg, "id=\"drumkeyboard\""));
    return 1;
}

static void end_block(void)
{
    finish_kick_block();
    mod_block_end();
}

static int peak(const short samples[128])
{
    int maximum = 0;
    for (unsigned i = 0; i < 128; ++i) {
        int value = samples[i] < 0 ? -samples[i] : samples[i];
        if (value > maximum)
            maximum = value;
    }
    return maximum;
}

static void test_waveforms(void)
{
    const int samples[WAVE_COUNT][4] = {
        {0, 32767, 0, -32767}, {0, 32767, 0, -32767},
        {-32767, -16383, 1, 16385}, {32767, 16383, -1, -16385},
        {32767, 32767, -32767, -32767}
    };
    for (unsigned wave = 0; wave < WAVE_COUNT; ++wave)
        for (unsigned quarter = 0; quarter < 4; ++quarter)
            assert(wave_value(wave, quarter << 30) == samples[wave][quarter]);
    assert(lfo_phase_step(0) > lfo_phase_step(63));
}

static void test_transport(short params[PARAM_COUNT])
{
    struct clock_state clock = {0};
    struct opus_lfo lfo = {.instrument = OPUS_FX_DRUM, .slot = 2, .pass = 3,
                           .params = params, .state = &clock};
    params[0] = WORD(32, 31, SQUARE, LENGTH);
    params[1] = WORD(43, 31, SQUARE, LEVEL);
    short sound[8], out[128], dry[128];
    memcpy(sound, kit[0], sizeof sound);
    sound[HITS_PAD_LEVEL] = 12000;

    /* Pass 3 must publish before the first real HITS render in this block. */
    assert(registered->apply(&lfo) == 0);
    assert(mod_for_pad(0)->value[LENGTH] == 32767 && mod_for_pad(0)->value[LEVEL] == 32767);
    assert(mod_for_pad(23)->value[LENGTH] == 0 && mod_for_pad(23)->value[LEVEL] == 0);
    unsigned phase = clock.osc[0].phase;
    registered->tick(&lfo);
    assert(clock.osc[0].phase == phase);

    struct voice modulated = {0}, other = {0};
    init(&modulated, 0);
    init(&other, 0);
    note_key(&modulated, sound, 0, 127);
    note_key(&other, sound, 23, 127);
    render(&modulated, out, sound, 0);
    render(&other, dry, sound, 0);
    assert(modulated.end == hits_length_ticks(32767));
    assert(other.end == hits_length_ticks((unsigned)sound[HITS_PAD_LENGTH]));
    assert(peak(dry) > 0 && peak(out) > peak(dry));
    slot_number = 3;
    assert(!mod_for_pad(0));
    slot_number = 2;
    end_block();
    assert(!mod_for_pad(0));

    /* Run past half a cycle with no note triggers or voice passes. */
    for (unsigned i = 0; i < 500; ++i) {
        registered->tick(&lfo);
        end_block();
    }
    assert(clock.osc[0].phase == phase + 500u * lfo_phase_step(32));
    assert(clock.osc[1].phase != clock.osc[0].phase);
    registered->apply(&lfo);
    assert(mod_for_pad(0)->value[LENGTH] == -32767 && mod_for_pad(0)->value[LEVEL] == -32767);

    /* Existing hit keeps Length, but Level changes on the next voice block. */
    render(&modulated, out, sound, 0);
    assert(modulated.end == hits_length_ticks(32767) && peak(out) == 0);
    note_key(&modulated, sound, 0, 127);
    render(&modulated, out, sound, 0);
    assert(modulated.end == hits_length_ticks(0) && peak(out) == 0);
    assert(sound[HITS_PAD_LEVEL] == 12000 && sound[HITS_PAD_LENGTH] == kit[0][HITS_PAD_LENGTH]);
    end_block();

    /* Two independent offsets add when both LFOs target LEVEL. */
    params[46] = WORD(63, 7, SQUARE, LEVEL);
    params[47] = WORD(63, 11, SQUARE, LEVEL);
    registered->tick(&lfo);
    assert(mod_for_pad(23)->value[LENGTH] == 0);
    int offset_a = wave_value(SQUARE, clock.osc[46].phase) * 7 / 31;
    int offset_b = wave_value(SQUARE, clock.osc[47].phase) * 11 / 31;
    assert(mod_for_pad(23)->value[LEVEL] == offset_a + offset_b);
    params[46] = defaults[46];
    params[47] = defaults[47];
    end_block();

    /* Bypass and synth passes cannot publish drum modulation. */
    lfo.instrument = OPUS_FX_SYNTH;
    registered->tick(&lfo);
    registered->apply(&lfo);
    assert(!mod_for_pad(0));
    lfo.instrument = OPUS_FX_DRUM;
    note_key(&modulated, sound, 0, 127);
    render(&modulated, out, sound, 0);
    assert(modulated.end == hits_length_ticks((unsigned)sound[HITS_PAD_LENGTH]) && peak(out) > 0);

    /* Keep phase over a slot/toggle gap and a manual rate edit. */
    phase = clock.osc[0].phase;
    for (unsigned i = 0; i < 10; ++i)
        end_block();
    params[0] = WORD(63, 31, SINE, LENGTH);
    registered->tick(&lfo);
    assert(clock.osc[0].phase == phase + 10u * lfo_phase_step(32) + lfo_phase_step(63));
    end_block();
}

static void test_extended_targets(short params[PARAM_COUNT])
{
    short settings[PARAM_COUNT];
    memcpy(settings, params, sizeof settings);
    struct clock_state clock = {0};
    struct opus_lfo lfo = {.instrument = OPUS_FX_DRUM, .slot = 2,
                           .params = settings, .state = &clock};
    settings[0] = WORD(63, 31, SQUARE, LENGTH);
    settings[1] = WORD(63, 0, SQUARE, LEVEL);
    short base[8], dry[128], wet[128];
    memcpy(base, kit[0], sizeof base);
    base[1] = base[2] = 12000;
    struct voice voice = {0};
    init(&voice, 0);
    note_key(&voice, base, 0, 127);
    render(&voice, dry, base, 0);
    assert(peak(dry) > 0);

    for (unsigned target = PITCH; target < TARGET_COUNT; ++target) {
        edit_target(settings, 0, target);
        assert(read_setting(settings, 0).target == target && read_setting(settings, 1).target == LEVEL);
        registered->tick(&lfo);
        assert(mod_for_pad(0)->value[target] == 32767);
        assert(mod_for_pad(1)->value[target] == 0);
        struct voice modulated = {0};
        init(&modulated, 0);
        note_key(&modulated, base, 0, 127);
        render(&modulated, wet, base, 0);
        assert(memcmp(dry, wet, sizeof dry));
        end_block();
    }
}

static void test_page(short params[PARAM_COUNT])
{
    visible = (struct opus_page){.kind = OPUS_PAGE_LFO, .t = 4, .where = OPUS_FX_DRUM,
                                 .module = registered, .on = 1, .slot = 2,
                                 .par = params, .n = PARAM_COUNT};
    fluke_edited = -1;
    fluke_state = (1u << 31) | (1u << 28);
    short before[PARAM_COUNT];
    memcpy(before, params, sizeof before);
    for (int encoder = 0; encoder < 4; ++encoder)
        assert(registered->page_turn(&visible, encoder, 1));
    assert(registered->page_key(&visible, 0x3b, 1));
    assert(!memcmp(before, params, sizeof before) && active_lfo[2] == 0);
    registered->page();
    assert(!strcmp(text_slots[18], "select sound with shift + note"));

    /* Active FLUKE can pick a pad from the FLUX page. */
    shifted = 1;
    assert(!mod_key_event(0x35, 1) && fluke_edited == 23);
    assert(!mod_key_event(0x35, 0) && fluke_edited == 23);
    int direct_edit;
    hits_mod_keyboard(&direct_edit);
    assert(direct_edit == 23);
    shifted = 0;
    short direct_setting = params[46];
    assert(registered->page_turn(&visible, 1, 1));
    assert(unpack(params[46]).rate == unpack(direct_setting).rate + 1);
    params[46] = direct_setting;
    fluke_state = (1u << 31) | (1u << 28) | (1u << 23) | 5;
    shifted = 1;
    assert(!registered->page_key(&visible, 0x3b, 1));
    assert(!registered->page_turn(&visible, 1, 1));
    shifted = 0;
    assert(registered->page_key(&visible, 0x3b, 1));

    short other_pad = params[0];
    short same_pad_a = params[46];
    assert(registered->page_turn(&visible, 0, 2));
    assert(registered->page_turn(&visible, 1, 64));
    assert(registered->page_turn(&visible, 2, 32));
    assert(registered->page_turn(&visible, 3, 2));
    assert(params[0] == other_pad && params[46] == same_pad_a);
    struct setting b = read_setting(params, 47);
    assert(b.wave == RAMP_DOWN && b.rate == 63 && b.depth == 31 && b.target == TIMBRE);

    registered->page();
    assert(lines == 132 && lane_green >= 1 && lane_red == 1);
    assert(!strcmp(registered->name, "flux"));
    assert(!strcmp(text_slots[2], "edit e5") && !strcmp(text_slots[3], "a") &&
           !strcmp(text_slots[6], "b") && !text_slots[5][0] && !text_slots[8][0] &&
           !strcmp(text_slots[13], "ramp-") && !strcmp(text_slots[14], "10000") &&
           !strcmp(text_slots[15], "100%") && !strcmp(text_slots[16], "timb") &&
           !strcmp(text_slots[7], "timbre"));
    void *screen = svg_nodes;
    registered->screen_init(&screen);
    registered->screen_draw(&screen, 0);
    assert(native_draws == 1 && edit_labels == 1 && parameter_labels == 4 && parameter_values == 4);
    assert(!svg_nodes[svg_index("dk23")][0x50] && svg_nodes[svg_index("dk2")][0x50]);
    assert(svg_nodes[svg_index("red23")][0x50]);
    assert(!svg_nodes[svg_index("selectsound")][0x50]);
    assert(!svg_nodes[svg_index("dk1")][0x50] && svg_nodes[svg_index("mod23")][0x50]);
    assert(svg_nodes[svg_index("wave13")][0x50] && !svg_nodes[svg_index("wave11")][0x50]);
    assert(svg_nodes[svg_index("edit1")][0x50] && !svg_nodes[svg_index("edit0")][0x50]);
    /* FLUKE OFF hides the keyboard and edit prompt with a stale edited pad. */
    fluke_state &= ~(1u << 28);
    registered->screen_draw(&screen, 0);
    assert(!svg_nodes[svg_index("drumkeyboard")][0x50] &&
           !svg_nodes[svg_index("selectsound")][0x50]);
    memset(text_slots, 0, sizeof text_slots);
    registered->page();
    assert(!strcmp(text_slots[2], "f3") && !text_slots[18][0]);
    for (unsigned i = 9; i <= 16; ++i)
        assert(text_slots[i][0]);
    short old_rate = params[1], old_edited = params[47];
    assert(registered->page_turn(&visible, 1, 1));
    assert(params[1] != old_rate && params[47] == old_edited);
    shifted = 1;
    assert(!mod_key_event(0x0d, 1) && fluke_edited == 23);
    shifted = 0;
    for (unsigned pad = 0; pad < HITS_MOD_PADS; ++pad)
        assert(!svg_nodes[1 + pad][0x50]);

    static unsigned char hits_green[24][0x51], hits_red[24][0x51];
    for (unsigned pad = 0; pad < HITS_MOD_PADS; ++pad) {
        pad_node[pad] = hits_green[pad];
        edit_node[pad] = hits_red[pad];
    }
    show_fluke_keys(5, 23);
    assert(hits_green[0][0x50] && hits_green[2][0x50] && hits_red[23][0x50]);
    show_fluke_keys(0, 23);
    for (unsigned pad = 0; pad < HITS_MOD_PADS; ++pad) {
        assert(!hits_green[pad][0x50]);
        assert(hits_red[pad][0x50] == (pad == 23));
    }
    fluke_state |= 1u << 28;
    fluke_edited = -1;
    registered->screen_draw(&screen, 0);
    assert(svg_nodes[svg_index("selectsound")][0x50] && !svg_nodes[svg_index("wave13")][0x50]);
    int edited;
    fluke_state &= ~(1u << 31);
    assert(!hits_mod_keyboard(&edited) && edited == -1);
    selected = 0;
    assert(!registered->page_turn(&visible, 1, 1));
    registered->page();
    assert(!strcmp(text_slots[1], "this device needs hits"));
    registered->screen_draw(&screen, 0);
    assert(!svg_nodes[svg_index("fluxart")][0x50] && svg_nodes[svg_index("selectsound")][0x50]);
}

static void test_companion_activity(short params[PARAM_COUNT])
{
    end_block();
    assert(!flux_active(2));
    struct hits_mod_delta silent[HITS_MOD_PADS] = {0};
    hits_mod_publish_v2(2, silent);
    end_block();
    assert(flux_active(2) && !flux_active(3));
    end_block();
    assert(!flux_active(2));

    selected = 1;
    visible = (struct opus_page){.kind = OPUS_PAGE_LFO, .t = 4, .where = OPUS_FX_DRUM,
                                 .module = registered, .on = 1, .slot = 2,
                                 .par = params, .n = PARAM_COUNT};
    fluke_state = 1u << 31;
    fluke_edited = -1;
    assert(hits_fluke_hold_state() == -1);
    assert(!registered->page_key(&visible, 0x3e, 1));
    fluke_state |= 1u << 28;
    assert(hits_fluke_hold_state() == 0);
    registered->page();
    assert(!strcmp(text_slots[24], "hold"));
    assert(registered->page_key(&visible, 0x3e, 1));
    assert(registered->page_key(&visible, 0x3e, 0));
    assert(hits_fluke_hold_state() == 1);
    registered->page();
    assert(!strcmp(text_slots[23], "hold"));
    assert(registered->page_key(&visible, 0x3e, 1));
    assert(hits_fluke_hold_state() == 0);
}

static void test_direct_hits_selection(short params[PARAM_COUNT])
{
    fluke_state = 0;
    fluke_edited = -1;
    assert(!fluke_link && hits_fluke_hold_state() == -1);
    visible = (struct opus_page){.kind = OPUS_PAGE_DRUM, .t = 1,
                                 .where = OPUS_FX_DRUM, .module = &engine};
    shifted = 1;
    assert(!hits_keyboard(0x35, 1));
    int edited;
    assert(!hits_mod_keyboard(&edited) && edited == -1);
    visible = (struct opus_page){.kind = OPUS_PAGE_LFO, .t = 4, .where = OPUS_FX_DRUM,
                                 .module = registered, .on = 1, .slot = 2,
                                 .par = params, .n = PARAM_COUNT};
    assert(!mod_key_event(0x35, 1) && control_pad() == 0);
    shifted = 0;
    short original = params[0];
    assert(registered->page_turn(&visible, 1, -1));
    assert(unpack(params[0]).rate == unpack(original).rate - 1);
    params[0] = original;
    registered->page();
    assert(!strcmp(text_slots[2], "f3") && !text_slots[18][0]);
    assert(lane_green == 0 && lane_red == 0);
    lines = 0;

}

static void test_edit_pad_retarget(void)
{
    selected = 1;
    fluke_state = (1u << 31) | (1u << 28);
    fluke_edited = 23;
    current_drum_pad = 0;  /* Simulate playback changing the OS pad. */
    visible = (struct opus_page){.kind = OPUS_PAGE_DRUM, .t = 1,
                                 .where = OPUS_FX_DRUM, .module = &engine};
    struct opus_page page;
    assert(opus_page_now(&page) && page.key == 0);
    assert(engine.page_turn(&page, 0, 1));
    assert(current_drum_pad == 23 && page.key == 23);
    assert(mock_pad_params[0][0] == 0 && mock_pad_params[23][0] == 512);
    current_drum_pad = 0;
    assert(opus_page_now(&page));
    assert(engine.page_key(&page, 0x3b, 1));
    assert(current_drum_pad == 23 && mock_pad_params[0][4] == 0 &&
           mock_pad_params[23][4] != 0);
}

static const struct hits_grip_link *grip_companion;
static int capture_grip_link(struct opus_page *page, int code, int pressed)
{
    assert(code == HITS_GRIP_CONNECT_KEY && !pressed);
    grip_companion = page->inst;
    return 1;
}

static void test_grip_connection(void)
{
    static const struct opus_effect grip = {
        .size = sizeof(struct opus_effect), .name = "grip", .page_key = capture_grip_link
    };
    visible = (struct opus_page){.kind = OPUS_PAGE_EFFECT, .where = OPUS_FX_MASTER,
                                 .module = &grip};
    connect_visible_grip();
    assert(grip_companion && grip_companion->kick_peak == hits_kick_peak &&
           grip_companion->connect_fluke == hits_connect_fluke &&
           grip_companion->hold_state == hits_fluke_hold_state);
}

int main(void)
{
    struct opl_self self = {0};
    assert(hits_mod_init(&self) == 0);
    assert(registered->nparams == 60 && !registered->trigger);
    short params[PARAM_COUNT];
    memcpy(params, registered->defaults, sizeof params);
    test_waveforms();
    test_transport(params);
    test_extended_targets(params);
    test_direct_hits_selection(params);
    static struct hits_fluke_link link = {
        .keys = fluke_drum_keys, .edit = fluke_hits_select_pad,
        .root = fluke_drum_root, .hold = fluke_hits_hold, .cut = fluke_hits_cut
    };
    struct opus_page hits_page = {.module = &engine, .inst = &link};
    struct opus_page flux_page = {.module = registered, .inst = &link};
    assert(engine.page_key(&hits_page, HITS_FLUKE_CONNECT_KEY, 0) && fluke_link == &link);
    fluke_link = 0;
    assert(registered->page_key(&flux_page, HITS_FLUKE_CONNECT_KEY, 0) && fluke_link == &link);
    assert(engine.page_key(&hits_page, HITS_FLUKE_CONNECT_KEY, 0));
    test_page(params);
    test_companion_activity(params);
    test_edit_pad_retarget();
    test_grip_connection();
    puts("PASS waveform, per-pad HITS audio, silent phase, bypass, slot isolation, controls and display state");
}
