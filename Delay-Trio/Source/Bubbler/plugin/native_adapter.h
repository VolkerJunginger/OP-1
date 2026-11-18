/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native Opus 2.31 page interface, following MidiReverb 0.3.0.
 * All sound controls live in the eight parameter words; UI flags live in fx->state.
 */
#include "native_svg.h"
#include "screen_file.h"
static char screen_path[256];
static struct opl_self *native_self;
#include "encoder_speed.h"
static struct encoder_speed bubbler_speed;
static void bubbler_expire(void) { bubbler_speed.burst = 0; }

static void unpack_ui(const short p[8]) {
 struct native_ui *u=page_ui(); u->transpose=((unsigned short)p[6]>>15)|(((unsigned short)p[4]>>15)<<1)|(((unsigned short)p[5]>>15)<<2)|((((unsigned short)p[7]>>14)&1u)<<3);if(u->transpose>9)u->transpose=0;
}
static void pack_ui(short p[8]) {
 struct native_ui *u=page_ui(); unsigned t=(unsigned)u->transpose;p[6]=(short)(((unsigned short)p[6]&0x7fffu)|((t&1u)<<15));p[4]=(short)(((unsigned short)p[4]&0x7fffu)|((t&2u)<<14));p[5]=(short)(((unsigned short)p[5]&0x7fffu)|((t&4u)<<13));p[7]=(short)(((unsigned short)p[7]&0xbfffu)|((t&8u)<<11));
}
static int page_values(struct opus_page *pg,short p[8]) {
 if(!pg || pg->kind!=OPUS_PAGE_EFFECT || pg->module!=&native_effect || !pg->par || pg->n<8)return 0;
 for(int j=0;j<8;++j)p[j]=pg->par[j];
 unpack_ui(p);return 1;
}
static void commit(struct opus_page *pg,short p[8]) {
 pack_ui(p);for(int j=0;j<8;++j)pg->par[j]=p[j];
}
/* Core/OS arrow handling may precede the module's page_key callback.
 * Inspect the native page in the documented pre-core key filter instead.
 * Only physical key-up pairing is global; the remembered page is instance UI.
 */
static unsigned navigation_taken[2];
static int navigation_filter(int code, int pressed)
{
 int arrow = code == 0x24 ? 0 : code == 0x25 ? 1 : -1;
 if (arrow < 0) return 0;
 if (!pressed) {
  if (!navigation_taken[arrow]) return 0;
  navigation_taken[arrow] = 0;
  return 1;
 }
 if (navigation_taken[arrow]) return 1;
 if (!fw_shift_held() || opus_fn_held()) return 0;
 struct opus_page pg;
 if (!opus_page_now(&pg) || pg.module != &native_effect ||
     pg.kind != OPUS_PAGE_EFFECT || pg.where == 2 || !pg.on ||
     !pg.inst || !pg.par || pg.n < 8) return 0;
 struct opus_fx *fx = pg.inst;
 if (fx->stereo || !fx->state) return 0;
 struct native_instance *instance = fx->state;
 if (instance->magic != 0x45534332u) return 0;
 instance->ui.extra_page[0] = arrow;
 bubbler_expire();
 navigation_taken[arrow] = 1;
 return 1;
}
static int page_key(struct opus_page *pg,int code,int pressed) {
 short p[8];if(!page_values(pg,p) || opus_fn_held())return 0;
 if(pg->where==2)return 1;
 viewed_owner=0;viewed_frame=frame_number;
 if(pressed)bubbler_expire();
 int handled=key(code,pressed);if(handled)commit(pg,p);return handled;
}
static int esc_density_turn(int value,int detents)
{
 static const unsigned char stops[]={
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,
  22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,
  53,56,59,62,65,68,71,74,77,80,84,88,92,96,100
 };
 int count=(int)(sizeof stops/sizeof stops[0]);
 while(detents>0){for(int i=0;i<count;++i)if(stops[i]>value){value=stops[i];break;}--detents;}
 while(detents<0){for(int i=count-1;i>=0;--i)if(stops[i]<value){value=stops[i];break;}++detents;}
 return value;
}

static int page_turn(struct opus_page *pg,int encoder,int detents) {
 short p[8];if(encoder<0 || encoder>3 || opus_fn_held() || !page_values(pg,p))return 0;
 if(pg->where==2)return 1;
 viewed_owner=0;viewed_frame=frame_number;
 int d=clamp(detents,-64,64);
 int shift=fw_shift_held();
 int extra=page_ui()->extra_page[0];
 int tag=encoder+4*shift+8*(shift&&extra)+16*(shift&&extra&&encoder==2&&page_ui()->res_edit[0]);
 if(shift&&extra&&encoder==0)bubbler_expire();
 else d=clamp(encoder_speed_turn(&bubbler_speed, native_self, bubbler_expire, pg->par, tag, d),-64,64);

 
 if(!(fw_shift_held() && BUBBLER.shift_knob && BUBBLER.shift_knob(p,encoder,d))) {
  if(encoder==2 && !fw_shift_held()){struct bb_params q;bb_decode(p,&q);q.density=esc_density_turn(q.density,d);bb_encode(p,&q);}else{int step=BUBBLER.limits[encoder][2]/8;
  if(fw_shift_held())step=step>8?step/8:1;
  p[encoder]=(short)clamp(p[encoder]+d*step,BUBBLER.limits[encoder][0],BUBBLER.limits[encoder][1]);}
 }
 commit(pg,p);return 1;
}
static void screen_draw(void *scr,int dt) {
 (void)dt;struct opus_page pg;short p[8];
 if(!opus_page_now(&pg) || !page_values(&pg,p))return;
 for(int j=0;j<4;++j)p[j]=pg.live[j];
 frame_begin();
 struct opus_fx_view v={0};view(p,&v);
 native_screen_prepare(p[3]);
 fw_screen_draw(scr);underlay(0);
 for(int j=0;j<4;++j) {
  int y=j&1?98:9;
  if(j<2){native_page_text(native_self,16+j*2,9,y,10,OPUS_COLOR_E(j),v.label[j]);native_page_text(native_self,17+j*2,9,y+20,24,OPUS_COLOR_TEXT,v.value[j]);}
  else{native_page_text_right(native_self,16+j*2,311,y,10,OPUS_COLOR_E(j),v.label[j]);native_page_text_right(native_self,17+j*2,311,y+20,24,OPUS_COLOR_TEXT,v.value[j]);}
 }
 if(pg.where==2)instrument_only_badge(native_self);
 /* Opus draws its FX OFF overlay after returning from this page. */
}
int opl_init(struct opl_self *s) {
 native_self=s;art_self=s;
 native_effect=BUBBLER;
 native_effect.shift_knob=0; /* Shift goes through page_turn with its instance. */
 for(int j=4;j<8;++j){native_effect.limits[j][0]=-32768;native_effect.limits[j][1]=32767;native_effect.limits[j][2]=0;}
 native_effect.defaults[7]=(short)((unsigned short)native_effect.defaults[7]&0xbfffu);
 bb_fast_tables(opus_fast(s,BB_FAST_TABLE_BYTES));bb_fast_clip(opus_fast(s,BB_FAST_CLIP_BYTES));
 unsigned n=0;while(s->folder[n] && n<sizeof screen_path-12){screen_path[n]=s->folder[n];++n;}
 if(s->folder[n])return -1;
 const char *suffix="/screen.svg";while(*suffix)screen_path[n++]=*suffix++;screen_path[n]=0;
 if(!native_screen_start(s) || !native_screen_file(s,screen_path,ESC_SVG,sizeof ESC_SVG-1))return -2;
 native_effect.svg=screen_path;native_effect.screen_init=native_screen_init;native_effect.screen_draw=screen_draw;
 native_effect.page_key=page_key;native_effect.page_turn=page_turn;
 int result=opus_effect_register(s,&native_effect);
 if(result<0)return result;
 opus_on_key_filter(s,navigation_filter);
 return 0;
}
