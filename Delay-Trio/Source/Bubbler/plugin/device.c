/* SPDX-License-Identifier: GPL-3.0-or-later
 * Opus adapter based on the GPL-3.0 opus.bitcrush example.
 */
#include "opus_plugin.h"
#include "native_screen.h"
#include "op1fw.h"
#include "bubbler_dsp.h"
#include "bubbler_art.h"
#include "instrument_only.h"
struct native_ui { int extra_page[1],res_edit[1],push_taken,transpose; };
struct native_bulk { const void *owner; short ring[BB_STORAGE]; };
struct native_instance { struct bb_state dsp; struct native_ui ui; unsigned magic; };
static struct opus_effect native_effect;
static struct native_ui fallback_ui;
static struct native_ui *page_ui(void) {
 struct opus_page pg;
 if(opus_page_now(&pg) && pg.module==&native_effect && pg.inst) {
  struct opus_fx *fx=pg.inst;
  if(!fx->stereo && fx->state)return &((struct native_instance *)fx->state)->ui;
 }
 return &fallback_ui;
}
#define extra_page (page_ui()->extra_page)
#define res_edit (page_ui()->res_edit)
#define push_taken (page_ui()->push_taken)
#define transpose (page_ui()->transpose)
static int clamp(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}
static void need(int stereo,struct opus_need *n) {
 n->state=stereo?0:sizeof(struct native_instance);
 n->bulk=stereo?0:sizeof(struct native_bulk); n->fast=stereo?0:64u*sizeof(int); n->fast_moves=0;
}
static void init(struct opus_fx *fx) {
 if(fx->stereo || !fx->state)return;
 struct native_instance *i=fx->state;
 if(i->magic!=0x45534332u || opus_fx_list_shown()) {
  unsigned char *b=(unsigned char *)&i->ui;
  for(unsigned j=0;j<sizeof i->ui;++j)b[j]=0;
  
 }
 i->magic=0x45534332u;
 bb_init(&i->dsp);
}
static void mono(struct opus_fx *fx,short *buf,const short *input) {
 if(fx->stereo || !fx->state || !fx->bulk)return;
 struct native_instance *i=fx->state;
 short p[8];for(int j=0;j<8;++j)p[j]=input[j];
 struct native_bulk *b=fx->bulk;
 if(b->owner!=&i->dsp){bb_init(&i->dsp);b->owner=&i->dsp;}
 i->dsp.pitch_mode=(unsigned short)p[6]>>15 | (((unsigned short)p[4]>>15)<<1) | (((unsigned short)p[5]>>15)<<2) | (((unsigned short)p[7]>>14 & 1u)<<3);
 if(i->dsp.pitch_mode>9)i->dsp.pitch_mode=0;
 bb_render_work(&i->dsp,b->ring,buf,0,64,p,fx->fast);
}
static void stereo(struct opus_fx *fx, short *l, short *r, const short *par)
{
    (void)fx;(void)l;(void)r;(void)par; /* Master is dry, with no allocated state. */
}
/* Page preference is UI state, independently remembered for each sound owner.
 * 0 = extra page 2, 1 = extra page 3. Releasing Shift never changes it. */

static volatile unsigned frame_number, viewed_frame;
static volatile int viewed_owner = -1;


static struct opl_self *art_self;
static short art_par[4];
static char filter_text[16],res_text[24];
static int owner_now(void){return 0;}
static void frame_begin(void) {
    ++frame_number;
    bb_art_begin_frame();
}
static int key(int code,int pressed)
{
    if(code==0x3d && !pressed && push_taken) {
        push_taken=0;
        return 1;
    }
    if(owner_now()!=16 && code==0x3d && pressed && !opus_fn_held() && fw_shift_held() &&
    extra_page[owner_now()]==1 && 1 &&
    viewed_owner==owner_now() && frame_number-viewed_frame<=1u) {
        res_edit[owner_now()]=!res_edit[owner_now()];
        push_taken=1;
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
    if(owner_now()==16 || encoder<0 || encoder>3 || opus_fn_held()) return 0;
    int d=clamp(detents,-64,64);
    struct bb_params p;
    bb_decode(par,&p);
    if(!extra_page[owner_now()]) {
        if(encoder==0) p.scatter=clamp(p.scatter+d*1024,0,32767);
        if(encoder==1) p.jitter=clamp(p.jitter+d*1024,0,32767);
        if(encoder==2) p.size=clamp(p.size+d*1024,0,32767);
        if(encoder==3) p.size_var=clamp(p.size_var+d*1024,0,32767);
    } else {
        if(encoder==0 && d) {
            static const int position[10]={0,1,-4,-3,4,5,-2,-1,2,3};
            static const int mode[10]={2,3,6,7,0,1,8,9,4,5};
            transpose=mode[clamp(position[transpose]+d,-4,5)+4];
            
        }
        if(encoder==1) p.pitch_var=clamp(p.pitch_var+d*5,0,100);
        if(encoder==2) {
            if(res_edit[owner_now()])p.resonance=clamp(p.resonance+d*2,0,63);
            else p.tone=clamp(p.tone+d*4,0,127);
        }
        if(encoder==3) p.reverse=clamp(p.reverse+d*1024,0,32767);
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
    !1 || opus_fx_list_shown()) return;
    unsigned page=fw_shift_held()?(unsigned)extra_page[owner]+1u:0u;
    bb_art_draw(art_self,art_par,page);
    if(page==2) {
        native_page_text_right(art_self,2,311,29,16,OPUS_COLOR_TEXT,filter_text);
        native_page_text_right(art_self,1,311,48,10,OPUS_COLOR_E3,res_text);

    }
}
/* After native drawing, repair only a verified stock plot line. The saved
 * band is invalidated each frame; popups/other changes cause no pixel writes. */
static void view(const short *par, struct opus_fx_view *v)
{
    static const char *const normal[4] = {
        "time", "feedback", "density", "mix"
    } ;
    static const char *const shifted[4] = {
        "time var", "start var", "size", "size var"
    } ;
    static const char *const grains[4] = {
        "pitch", "pitch var", "filter freq", "reverse"
    } ;
    static char value[4][16];
    int owner=owner_now();
    viewed_frame=frame_number;
    viewed_owner=owner;
    int page=fw_shift_held()?extra_page[owner]+1:0;
    struct bb_params p;
    bb_decode(par,&p);

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
        static const char *const modes[10]={"off","key","-12 st","-7 st","+7 st","+12 st","-5 st","-3 st","+3 st","+5 st"};
        fw_snprintf(value[0],16,"%s",modes[transpose]);
        fw_snprintf(value[1],16,"%i%%",p.pitch_var);
        int hz=bb_cutoff(p.tone<<8);
        if(hz>=1000)fw_snprintf(filter_text,16,"%i.%i khz",hz/1000,(hz%1000)/100);
        else fw_snprintf(filter_text,16,"%i hz",hz);
        fw_snprintf(res_text,24,res_edit[owner]?"> reso %i%%":"reso %i%%",p.resonance*100/63);
        value[2][0]=0;
        fw_snprintf(value[3],16,"%i%%",p.reverse*100/32767);
    }
    for (int i = 0; i < 4; ++i) {
        v->label[i] = page==0?normal[i]:page==1?shifted[i]:grains[i];
        v->value[i] = page==2 && i==2?"":value[i];
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

#undef extra_page
#undef res_edit
#undef push_taken
#undef transpose

#include "native_adapter.h"
