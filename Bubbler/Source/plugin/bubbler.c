/* SPDX-License-Identifier: GPL-3.0-or-later
 * Opus adapter based on the GPL-3.0 opus.bitcrush example.
 */
#include "opus_plugin.h"
#include "op1fw.h"
#include "bubbler_dsp.h"
#include "bubbler_art.h"
static int clamp(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}
static void need(int stereo, struct opus_need *n)
{
    (void)stereo;
    n->state = sizeof(struct bb_state);
    n->bulk = BB_STORAGE * sizeof(short);
    n->fast = 64u * sizeof(int);
}
static void init(struct opus_fx *fx) {
    bb_init((struct bb_state *)fx->state);
}
static void mono(struct opus_fx *fx, short *buf, const short *par)
{
    bb_render_work((struct bb_state *)fx->state, (short *)fx->bulk, buf, 0, 64, par, (int *)fx->fast);
}
static void stereo(struct opus_fx *fx, short *l, short *r, const short *par)
{
    bb_render_work((struct bb_state *)fx->state, (short *)fx->bulk, l, r, 64, par, (int *)fx->fast);
}
/* Page preference is UI state, independently remembered for each sound owner.
 * 0 = extra page 2, 1 = extra page 3. Releasing Shift never changes it. */
static int extra_page[OPUS_FX_OWNERS];
static volatile unsigned frame_number, viewed_frame;
static volatile int viewed_owner = -1;
static int arrow_taken[2],push_taken,transpose;
static void setting_changed(const char *name,int value)
{
    if(name && name[0]=='t') {
        transpose=clamp(value,0,5);
        bb_set_transpose(transpose);
    }
}
static struct opl_self *art_self;
static short art_par[4];
static char orange_random[16];
static int owner_now(void) {
    return clamp(opus_fx_owner(),0,OPUS_FX_OWNERS-1);
}
static void frame_begin(void) {
    ++frame_number;
    bb_art_begin_frame();
}
static int key(int code,int pressed)
{
    if(code==0x3e && !pressed && push_taken) {
        push_taken=0;
        return 1;
    }
    if(code==0x3e && pressed && !opus_fn_held() && fw_shift_held() &&
    extra_page[owner_now()]==1 && opus_fx_page_shown() && !opus_fx_list_shown() &&
    viewed_owner==owner_now() && frame_number-viewed_frame<=1u) {
        transpose=(transpose+1)%6;
        bb_set_transpose(transpose);
        opus_set_setting(art_self,"transpose",transpose);
        push_taken=1;
        return 1;
    }
    int arrow=code==0x24?0:code==0x25?1:-1;
    if(arrow>=0 && !pressed && arrow_taken[arrow]) {
        arrow_taken[arrow]=0;
        return 1;
        /* pair even if Shift was released first */
    }
    if(arrow>=0 && pressed && !opus_fn_held() && fw_shift_held() &&
    opus_fx_page_shown() && !opus_fx_list_shown() &&
    viewed_owner==owner_now() && frame_number-viewed_frame<=1u) {
        int owner=owner_now();
        extra_page[owner]=clamp(extra_page[owner]+(arrow?1:-1),0,1);
        arrow_taken[arrow]=1;
        return 1;
    }
    /* Invalidate the last-view token immediately on navigation. Other effects
         * cannot inherit it while the next display frame is still being built. */
    if(pressed && ((code>=1 && code<=9) || code==0x15 || code==0x2c ||
    (code>=0x2d && code<=0x2f) || code==0x10 ||
    (code>=0x11 && code<=0x14))) viewed_owner=-1;
    return 0;
}
static int shift_knob(short *par, int encoder, int detents)
{
    if(encoder<0 || encoder>3 || opus_fn_held()) return 0;
    int d=clamp(detents,-64,64);
    struct bb_params p;
    bb_decode(par,&p);
    if(!extra_page[owner_now()]) {
        if(encoder==0) p.scatter=clamp(p.scatter+d*1024,0,32767);
        if(encoder==1) p.jitter=clamp(p.jitter+d*1024,0,32767);
        if(encoder==2) p.size=clamp(p.size+d*1024,0,32767);
        if(encoder==3) p.size_var=clamp(p.size_var+d*1024,0,32767);
    } else {
        if(encoder==0) p.tone=clamp(p.tone+d*4,0,127);
        if(encoder==1) p.resonance=clamp(p.resonance+d*2,0,63);
        if(encoder==2) p.reverse=clamp(p.reverse+d*1024,0,32767);
        if(encoder==3) p.pitch_var=clamp(p.pitch_var+d*5,0,100);
    }
    if(d) bb_encode(par,&p);
    /* native callback saves all eight words */
    return 1;
}
/* The underlay runs before native values and popups. One-frame freshness
 * ties it to this effect's own view; navigation invalidates the token. */
static void underlay(unsigned short *buf)
{
    (void)buf;
    int owner=owner_now();
    if(viewed_owner!=owner || frame_number-viewed_frame>1u ||
    !opus_fx_page_shown() || opus_fx_list_shown()) return;
    unsigned page=fw_shift_held()?(unsigned)extra_page[owner]+1u:0u;
    bb_art_draw(art_self,art_par,page);
    if(page==2) {
        static const char *const modes[6]= {
            "off","","-12 st","-7 st","+7 st","+12 st"
        } ;
        opus_page_text_right(art_self,2,311,104,16,OPUS_COLOR_TEXT,orange_random);
        opus_page_text_right(art_self,1,311,123,10,OPUS_COLOR_E4,modes[transpose]);
        if(transpose==1) {
            /* Original miniature keyboard, native paths, not a stock glyph. */
            opus_page_path();
            opus_page_move(298*256,125*256);
            opus_page_line_to(310*256,125*256);
            opus_page_line_to(310*256,133*256);
            opus_page_line_to(298*256,133*256);
            opus_page_close();
            opus_page_stroke(256,OPUS_COLOR_E4,255,OPUS_CAP_ROUND,OPUS_JOIN_ROUND);
            for(int x=302;x<310;x+=4)opus_page_stroke_line(x*256,125*256,x*256,133*256,256,OPUS_COLOR_E4);
        }
    }
}
/* After native drawing, repair only a verified stock plot line. The saved
 * band is invalidated each frame; popups/other changes cause no pixel writes. */
static void finish_frame(unsigned short *buf)
{
    (void)buf;
    if(viewed_owner==owner_now() && viewed_frame==frame_number &&
    opus_fx_page_shown() && !opus_fx_list_shown())bb_art_repair_plot();
}
static void view(const short *par, struct opus_fx_view *v)
{
    static const char *const normal[4] = {
        "time", "feedback", "density", "mix"
    } ;
    static const char *const shifted[4] = {
        "time var", "start var", "size", "size var"
    } ;
    static const char *const grains[4] = {
        "filter freq", "reso", "reverse", "pitch var"
    } ;
    static char value[4][16];
    int owner=owner_now();
    viewed_frame=frame_number;
    viewed_owner=owner;
    int page=fw_shift_held()?extra_page[owner]+1:0;
    struct bb_params p;
    bb_decode(par,&p);
    fw_snprintf(orange_random,16,"%i%%",p.pitch_var);
    for(int i=0;i<4;++i)art_par[i]=par[i];
    art_par[2]=(short)(p.density*32767/100);
    if (!page) {
        fw_snprintf(value[0], 16, "%i ms", (bb_time_samples(par[0]) * 10 + 220) / 441);
        fw_snprintf(value[1], 16, "%i%%", (clamp(par[1], 0, 32767) * 95 + 16383) / 32767);
        fw_snprintf(value[2], 16, "%i%%", p.density);
        fw_snprintf(value[3], 16, "%i%%", (clamp(par[3], 0, 32767) * 100 + 16383) / 32767);
    } else if(page==1) {
        fw_snprintf(value[0],16,"%i%%",p.scatter*100/32767);
        fw_snprintf(value[1],16,"%i%%",p.jitter*100/32767);
        fw_snprintf(value[2],16,"%i%%",2+p.size*48/32767);
        fw_snprintf(value[3],16,"%i%%",p.size_var*100/32767);
    } else {
        int hz=bb_cutoff(p.tone<<8);
        if(hz>=1000)fw_snprintf(value[0],16,"%i.%i khz",hz/1000,(hz%1000)/100);
        else fw_snprintf(value[0],16,"%i hz",hz);
        fw_snprintf(value[1],16,"%i%%",p.resonance*100/63);
        fw_snprintf(value[2],16,"%i%%",p.reverse*100/32767);
        fw_snprintf(value[3],16,"%i%%",p.pitch_var);
    }
    for (int i = 0; i < 4; ++i) {
        v->label[i] = page==0?normal[i]:page==1?shifted[i]:grains[i];
        v->value[i] = page==2 && i==3?"":value[i];
    }
    for(int i=0;i<3;++i) v->wave[i]=0;
    v->waves=0;
    v->caption=0;
    /* counter is retained native text in the underlay */
}
static const struct opus_effect BUBBLER = {
    .size = sizeof(struct opus_effect),
    .name = "bubbler",
    .defaults = {
        20480, 12000, 26214, 16384, 29984, 0, 4108, -16352
    } ,
    .limits = {
        {
            0, 32767, 8192
        } , {
            0, 32767, 8192
        } ,
        {
            0, 32767, 8192
        } , {
            0, 32767, 8192
        } ,
        {
            0, 32767, 0
        } , {
            0, 32767, 0
        } ,
        {
            -12, 32767, 0
        } , {
            -32768, 32767, 0
        }
    } ,
    .need = need, .init = init, .mono = mono, .stereo = stereo,
    .view = view, .shift_knob = shift_knob,
} ;
int opl_init(struct opl_self *self)
{
    art_self=self;
    transpose=0;
    bb_set_transpose(0);
    opus_set_setting(self,"transpose",0);
    /* start Off, even after a previous selection */
    opus_on_setting(self,setting_changed);
    bb_fast_tables(opus_fast(self,BB_FAST_TABLE_BYTES));
    int type=opus_effect_register(self,&BUBBLER);
    if(type<0) return -1;
    bb_fast_clip(opus_fast(self,BB_FAST_CLIP_BYTES));
    /* optional, after effect scratch */
    opus_on_key(self,key);
    opus_on_frame_begin(self,frame_begin);
    opus_on_frame_under(self,underlay);
    opus_on_frame(self,finish_frame);
    return 0;
}
