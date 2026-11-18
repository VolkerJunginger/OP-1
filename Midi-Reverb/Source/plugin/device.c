/* SPDX-License-Identifier: GPL-3.0-only */
#include "opus_plugin.h"
#include "encoder_speed.h"
static struct encoder_speed reverb_speed;
static void reverb_expire(void) { reverb_speed.burst = 0; }

#include "op1fw.h"
#include "dsp.h"
#include "screen_data.h"
#include "screen_file.h"
#include "rom_file.h"
#include "names.h"

#define INSTANCE_MAGIC 0x4d525631u
struct instance {
    struct tank tank;
    unsigned magic, active_program, active_bank, active_rom;
    int active_engine, pending, fresh;
    short heard[8], baseline[8];
    unsigned held_mask;
};
static struct opus_effect effect;
static char screen_path[256];
static volatile int reconstruction = 1, low_cpu;
static const unsigned bank_order[] = {0, 2, 1};
static const short low[4] = {64, 0, 0, 8192};
static const short high[4] = {14528, 24576, 22528, 32767};
static const unsigned defaults[4] = {0, 90, 0, 50};
static const unsigned control_word[4] = {2, 1, 3, 0};
/* Words 0..3 retain their old meanings; 4 selects ROM, 5..7 its remembered presets. */
static unsigned current_bank(const short *par)
{
    unsigned bank = (unsigned)par[4];
    if (bank < ROM_BANKS && (rom_ready & (1u << bank))) return bank;
    for (unsigned i = 0; i < ROM_BANKS; ++i)
        if (rom_ready & (1u << bank_order[i])) return bank_order[i];
    return 0;
}
static unsigned decode(int word, short value, unsigned bank)
{
    unsigned max = word ? 100 : banks[bank].programs - 1;
    int range = high[word] - low[word], offset = value - low[word];
    if (offset <= 0) return 0;
    if (offset >= range) return max;
    return (unsigned)((offset * (int)max + range / 2) / range);
}
static short encode(int word, unsigned value, unsigned bank)
{
    unsigned max = word ? 100 : banks[bank].programs - 1;
    return (short)(low[word] + ((high[word] - low[word]) * (int)value + (int)max / 2) / (int)max);
}
static unsigned pack_controls(const short *par, unsigned bank)
{
    unsigned result = 0;
    for (int i=0;i<4;++i) result |= decode(i,par[i],bank) << (i*7);
    return result;
}
static void effective(const struct instance *s, short *par)
{
    for (int i=0;i<8;++i)
        if ((s->held_mask & (1u<<i)) && par[i] == s->baseline[i]) par[i]=s->heard[i];
}
static void need(int stereo, struct opus_need *n)
{
    n->state = stereo ? 0 : sizeof(struct instance);
    n->bulk = stereo ? 0 : RING_WORDS * sizeof(int16_t);
    n->fast = n->fast_moves = 0;
}
static void initialize(struct opus_fx *fx)
{
    if (fx->stereo || !fx->state || !fx->bulk) return;
    struct instance *s=fx->state;
    if (s->magic != INSTANCE_MAGIC) {
        s->magic=INSTANCE_MAGIC;
        s->active_bank=s->active_program=s->active_rom=~0u;
        s->active_engine=-1;
        s->held_mask=0;
        tank_reset(&s->tank);
    } else {
        s->pending=1;
        s->fresh=opus_fx_list_shown();
    }
    s->tank.ring=fx->bulk;
}
static void mono(struct opus_fx *fx, short *buf, const short *input)
{
    if (fx->stereo || !fx->state || !fx->bulk) return;
    struct instance *s=fx->state;
    short par[8];
    for (int i=0;i<8;++i) par[i]=input[i];
    if (s->pending) {
        int fresh=s->fresh;
        for (int i=0;i<8;++i) if (par[i]!=effect.defaults[i]) fresh=0;
        if (fresh) {
            s->held_mask=0;
            s->active_bank=~0u;
        } else {
            for (int i=0;i<8;++i) s->baseline[i]=par[i];
            s->held_mask=255;
        }
        s->pending=s->fresh=0;
    }
    effective(s,par);
    for (int i=0;i<8;++i) {
        if (input[i]!=s->baseline[i]) s->held_mask &= ~(1u<<i);
        s->heard[i]=par[i];
    }
    unsigned bank=current_bank(par), controls[4];
    for (int i=0;i<4;++i) controls[i]=decode(i,par[i],bank);
    if (!(rom_ready & (1u<<bank))) return;
    int engine=low_cpu;
    if (controls[0]!=s->active_program || bank!=s->active_bank || rom_revision!=s->active_rom || engine!=s->active_engine) {
        tank_reset(&s->tank);
        if (bank) extra_reset(s->tank.extra,bank,controls[0]);
        s->active_program=controls[0];s->active_bank=bank;
        s->active_rom=rom_revision;s->active_engine=engine;
    }
    tank_render(&s->tank,buf,controls,bank,reconstruction,engine);
}
static void master(struct opus_fx *fx, short *left, short *right, const short *par)
{ (void)fx;(void)left;(void)right;(void)par; }
static int page_values(struct opus_page *pg, short *par)
{
    if (!pg || pg->module!=&effect || pg->kind!=OPUS_PAGE_EFFECT || !pg->par || pg->n<8) return 0;
    for (int i=0;i<8;++i) par[i]=pg->par[i];
    struct instance *s=pg->inst ? ((struct opus_fx *)pg->inst)->state : 0;
    if (s && s->magic==INSTANCE_MAGIC) effective(s,par);
    return 1;
}
static void commit(struct opus_page *pg, const short *par)
{
    for (int i=0;i<8;++i) pg->par[i]=par[i];
    struct instance *s=pg->inst ? ((struct opus_fx *)pg->inst)->state : 0;
    if (s && s->magic==INSTANCE_MAGIC) {
        s->held_mask=0;
        for (int i=0;i<8;++i) s->heard[i]=par[i];
    }
}
static int page_key(struct opus_page *pg,int code,int pressed)
{
    short par[8];
    if (code!=0x3e || opus_fn_held() || fw_shift_held() || !page_values(pg,par)) return 0;
    if (pg->where==2) return 1;
    if (pressed) {
        unsigned bank=current_bank(par);
        par[5+bank]=(short)decode(0,par[0],bank);
        for (unsigned i=0;i<ROM_BANKS;++i) {
            bank=bank ? bank-1 : ROM_BANKS-1;
            if (rom_ready & (1u<<bank)) break;
        }
        if (rom_ready) {
            par[4]=(short)bank;
            unsigned p=(unsigned)par[5+bank];
            if (p>=banks[bank].programs) p=0;
            par[0]=encode(0,p,bank);
        }
        commit(pg,par);
    }
    return 1;
}
static struct opl_self *encoder_plugin;

static int page_turn(struct opus_page *pg,int encoder,int detents)
{
    short par[8];
    if (encoder<0 || encoder>3 || opus_fn_held() || !page_values(pg,par)) return 0;
    if (pg->where==2) return 1;
    unsigned bank=current_bank(par), word=control_word[encoder];
    int max=word ? 100 : (int)banks[bank].programs-1;
    if (detents<-100) detents=-100;
    if (detents>100) detents=100;
    if (word) detents=encoder_speed_turn(&reverb_speed, encoder_plugin, reverb_expire, pg->par, (int)word+4*(int)bank, detents);
    else reverb_expire();
    int value=(int)decode((int)word,par[word],bank)+detents;
    if (value<0) value=0;
    if (value>max) value=max;
    par[word]=encode((int)word,(unsigned)value,bank);
    par[4]=(short)bank;
    if (!word) par[5+bank]=(short)value;
    commit(pg,par);
    return 1;
}
static void setting_changed(const char *name,int value)
{
    if (same_name(name,"reconstruction")) reconstruction=value!=0;
    else if (same_name(name,"engine")) low_cpu=value!=0;
}
struct screen_text {
    unsigned object[41];
    char shown[32];
};
static struct screen_text text[7];
static const int identity[9] = {1024, 0, 0, 0, 1024, 0, 0, 0, 1024};
static const unsigned char colours[3][4] = {
    {77, 158, 255, 255}, {0, 237, 149, 255}, {255, 255, 255, 255}
};
static unsigned char *segments[14];
static unsigned char *logo_fills[sizeof logo_banks];
static unsigned last_logo = ROM_BANKS;
static int text_ready;

static void screen_init(void *scr)
{
    if (!scr) return;
    void *document = *(void **)scr;
    for (int i = 0; i < 14; ++i) {
        char id[] = "seg00";
        id[3] += i / 10;
        id[4] += i % 10;
        segments[i] = document && fw_svg_node_find(document, id, 0) ? fw_screen_fill(scr, id) : 0;
    }
    for (unsigned i = 0; i < sizeof logo_banks; ++i) {
        char id[] = "logo00";
        id[4] += i / 10;
        id[5] += i % 10;
        logo_fills[i] = document && fw_svg_node_find(document, id, 0) ? fw_screen_fill(scr, id) : 0;
    }
    last_logo = ROM_BANKS;
    if (text_ready) return;
    for (int i = 0; i < 3; ++i) {
        fw_text_new(text[i * 2].object, 174 * 1024, 48 + i * 28, 11, colours[i], 0);
        fw_text_new(text[i * 2 + 1].object, 307 * 1024, 48 + i * 28, 11, colours[2], 2);
    }
    fw_text_new(text[6].object, 13 * 1024, 131, 11, colours[2], 0);
    text_ready = 1;
}

static void update_logo(unsigned bank)
{
    if (bank == last_logo) return;
    for (unsigned i = 0; i < sizeof logo_banks; ++i) {
        unsigned char *fill = logo_fills[i];
        if (fill) {
            fill[2] = 255;
            fill[3] = (logo_banks[i] & (1u << bank)) ? 255 : 0;
        }
    }
    last_logo = bank;
}

static void update_digits(unsigned number, int visible)
{
    static const unsigned char masks[10] = {63, 6, 91, 79, 102, 109, 125, 7, 127, 111};
    unsigned mask = visible ? masks[number / 10] | ((unsigned)masks[number % 10] << 7) : 0;
    for (int i = 0; i < 14; ++i) {
        unsigned char *colour = segments[i];
        if (colour) {
            int on = (mask >> i) & 1;
            colour[0] = on ? 255 : 43;
            colour[1] = on ? 41 : 13;
            colour[2] = on ? 71 : 20;
        }
    }
}

static void draw_text(int slot, const char *value)
{
    struct screen_text *t = text + slot;
    int n = 0;
    while (n < (int)sizeof t->shown - 1 && value[n] && value[n] == t->shown[n]) ++n;
    if (value[n] != t->shown[n]) {
        for (; n < (int)sizeof t->shown - 1 && value[n]; ++n) t->shown[n] = value[n];
        t->shown[n] = 0;
        fw_set_text(t->object, t->shown);
    }
    fw_text_draw(t->object, fw_screen_raster(), identity);
}

static void screen_draw(void *scr, int dt)
{
    (void)dt;
    if (!scr) return;
    static const char *const labels[] = {"feedback", "cutoff", "mix"};
    static const char *const *const names[] = {program_names, midifex_names, midiverb2_names};
    struct opus_page pg;
    if (!opus_page_now(&pg) || pg.module != &effect || !pg.par || pg.n < 8) return;
    short par[8];
    for (int i = 0; i < 8; ++i) par[i] = pg.par[i];
    struct instance *s = pg.inst ? ((struct opus_fx *)pg.inst)->state : 0;
    if (s && s->magic == INSTANCE_MAGIC) effective(s, par);
    int pos = pg.where;
    unsigned bank = current_bank(par), packed = pack_controls(par, bank);
    unsigned program = decode(0, par[0], bank);
    if (program >= banks[bank].programs) program = banks[bank].programs - 1;
    update_logo(bank);
    update_digits(program + banks[bank].first, pos != 2 && (rom_ready & (1u << bank)));
    fw_screen_draw(scr);
    if (!text_ready) return;
    if (pos == 2) draw_text(6, "synth / drum only");
    else if (!rom_ready) draw_text(6, "add rom in mods/mv1rom");
    else draw_text(6, names[bank][program]);
    for (int i = 0; i < 3; ++i) {
        char value[20];
        unsigned v = (packed >> (control_word[i] * 7)) & 127;
        if (i == 1) fw_snprintf(value, sizeof value, "%i hz", (int)(1000 + v * 100));
        else fw_snprintf(value, sizeof value, "%i%%", (int)v);
        draw_text(i * 2, labels[i]);
        draw_text(i * 2 + 1, value);
    }
}


static void view(const short *par,struct opus_fx_view *v)
{
    static char values[4][20];
    static const char *const labels[]={"feedback","cutoff","mix","program"};
    unsigned bank=current_bank(par);
    for(int i=0;i<4;++i) {
        unsigned word=control_word[i], n=decode((int)word,par[word],bank);
        if(i==1) fw_snprintf(values[i],sizeof values[i],"%u hz",1000+n*100);
        else if(i==3) fw_snprintf(values[i],sizeof values[i],"%u",n+banks[bank].first);
        else fw_snprintf(values[i],sizeof values[i],"%u%%",n);
        v->label[i]=labels[i];v->value[i]=values[i];
    }
    v->caption="midireverb";
}
static struct opus_effect effect={
    .size=sizeof effect,.name="midireverb",.need=need,.init=initialize,
    .mono=mono,.stereo=master,.view=view,
    .svg=screen_path,.screen_init=screen_init,.screen_draw=screen_draw,
    .page_key=page_key,.page_turn=page_turn
};
int opl_init(struct opl_self *self)
{
    encoder_plugin=self;
    unsigned n=0;
    while(self->folder[n] && n<sizeof screen_path-sizeof "/screen.svg") {screen_path[n]=self->folder[n];++n;}
    if(self->folder[n]) return -1;
    const char *suffix="/screen.svg";
    while(*suffix) screen_path[n++]=*suffix++;
    screen_path[n]=0;
    for(int i=0;i<4;++i) {
        effect.defaults[i]=encode(i,defaults[i],0);
        effect.limits[i][0]=low[i];effect.limits[i][1]=high[i];effect.limits[i][2]=1;
    }
    effect.limits[4][1]=2;
    for(int i=5;i<8;++i) effect.limits[i][1]=(short)(banks[i-5].programs-1);
    if(!native_screen_file(self,screen_path,screen_svg,sizeof screen_svg-1)) return -3;
    if(!opus_on_folder(self,&rom_folder)) return -4;
    rom_changed();
    reconstruction=opus_setting(self,"reconstruction")!=0;
    low_cpu=opus_setting(self,"engine")!=0;
    opus_on_setting(self,setting_changed);
    return opus_effect_register(self,&effect)<0 ? -5 : 0;
}
