#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../Grip/cleancomp.c"

static struct opus_page shown;
static int hits, hold = -1, shifted, fn;
static int sc_labels, hold_labels;
static const struct hits_fluke_link *connected;

unsigned hits_kick_peak(void) { return 0; }
int hits_is_selected(void) { return hits; }
int hits_fluke_hold_state(void) { return hold; }
int hits_fluke_hold_key(int pressed)
{
    if (hold < 0)
        return 0;
    if (pressed)
        hold = !hold;
    return 1;
}
int hits_connect_fluke(const struct hits_fluke_link *link)
{ connected = link; return 1; }
int fw_shift_held(void) { return shifted; }
int opus_fn_held(void) { return fn; }
int opus_page_now(struct opus_page *page) { *page = shown; return 1; }
void *fw_screen_raster(void) { return &shown; }
void fw_screen_draw(void *screen) { assert(screen); }
int fw_snprintf(char *dst, unsigned size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int count = vsnprintf(dst, size, format, args);
    va_end(args);
    return count;
}
void *fw_text_new(void *mem, int x, int y, int size, const unsigned char *rgba, int align)
{
    (void)size; (void)rgba; (void)align;
    unsigned *text = mem;
    text[0] = x / 1024;
    text[1] = y;
    return mem;
}
void fw_set_text(void *mem, const char *text)
{
    unsigned *entry = mem;
    entry[2] = !strcmp(text, "kick sc") ? 1 : !strcmp(text, "hold") ? 2 : 0;
}
void fw_text_draw(void *mem, void *raster, const int *matrix)
{
    assert(raster && matrix);
    unsigned *entry = mem;
    if (entry[2] == 1) {
        assert(entry[0] == 164 && entry[1] == 128);
        ++sc_labels;
    } else if (entry[2] == 2) {
        assert(entry[0] == 242 && entry[1] == 128);
        ++hold_labels;
    }
}
void fw_path_clear(void *path) { (void)path; }
void fw_path_move_to(void *path, const int *xy, int absolute)
{ (void)path; (void)xy; (void)absolute; }
void fw_path_line_to(void *path, const int *xy, int absolute)
{ fw_path_move_to(path, xy, absolute); }
void fw_path_close(void *path) { (void)path; }
void fw_path_finish(void *path, int fill, int line)
{ (void)path; (void)fill; (void)line; }
void fw_draw_path(void *raster, int cache, const void *fills, const void *lines,
                  void *path, const int *m, int override, int extra)
{
    (void)raster; (void)cache; (void)fills; (void)lines;
    (void)path; (void)m; (void)override; (void)extra;
}

static void check_labels(int sc, int hold_label)
{
    sc_labels = hold_labels = 0;
    check_hits();
    effect.screen_draw(&shown, 0);
    assert(sc_labels == sc && hold_labels == hold_label);
}

int main(void)
{
    shown = (struct opus_page){.kind = OPUS_PAGE_EFFECT, .t = 3, .where = OPUS_FX_DRUM,
                               .module = &effect, .on = 1};
    static const struct hits_grip_link companion = {
        .kick_peak = hits_kick_peak, .selected = hits_is_selected,
        .hold_state = hits_fluke_hold_state, .hold_key = hits_fluke_hold_key,
        .connect_fluke = hits_connect_fluke
    };
    static struct hits_fluke_link link;
    struct opus_page request = shown;
    request.inst = &link;
    hits = 1;
    hold = 1;
    check_labels(0, 0);
    assert(!effect.page_key(&shown, 0x3d, 1) && !effect.page_key(&shown, 0x3e, 1));
    assert(!effect.page_key(&request, HITS_FLUKE_CONNECT_KEY, 0));

    request.inst = (void *)&companion;
    assert(effect.page_key(&request, HITS_GRIP_CONNECT_KEY, 0));
    request.inst = &link;
    assert(effect.page_key(&request, HITS_FLUKE_CONNECT_KEY, 0) && connected == &link);
    hold = -1;
    check_labels(1, 0);
    assert(effect.page_key(&shown, 0x3d, 1) && sidechain_enabled);
    assert(effect.page_key(&shown, 0x3d, 0) && sidechain_enabled);
    assert(!effect.page_key(&shown, 0x3e, 1) && sidechain_enabled);
    hold = 0;
    check_labels(1, 1);
    assert(effect.page_key(&shown, 0x3e, 1) && hold == 1 && sidechain_enabled);
    assert(effect.page_key(&shown, 0x3e, 0) && hold == 1);
    assert(effect.page_key(&shown, 0x3d, 1) && !sidechain_enabled && hold == 1);
    check_labels(1, 1);

    hits = 0;
    check_labels(0, 1);
    shown.where = OPUS_FX_MASTER;
    hits = 1;
    check_labels(0, 1);
    assert(!effect.page_key(&shown, 0x3d, 1));
    assert(effect.page_key(&shown, 0x3e, 1) && hold == 0);
    shifted = 1;
    assert(!effect.page_key(&shown, 0x3e, 1) && hold == 0);
    puts("PASS GRIP white Kick SC, orange Hold, conditional labels and FLUKE connection");
}
