/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native Opus 2.31 page interface, following MidiReverb 0.3.0.
 * All sound controls live in the eight parameter words; UI flags live in fx->state.
 */
#include "native_svg.h"
#include "screen_file.h"
static char screen_path[256];
static struct opl_self *native_self;
#include "encoder_speed.h"
static struct encoder_speed delayed_speed;
static void delayed_expire(void) { delayed_speed.burst = 0; }

static void unpack_ui(const short p[8]) {
 struct native_ui *u=page_ui(); u->resonance[0]=((unsigned short)p[6]>>3)&63u;u->frozen[0]=((unsigned short)p[6]>>9)&1u;
}
static void pack_ui(short p[8]) {
 struct native_ui *u=page_ui(); p[6]=(short)(((unsigned short)p[6]&7u)|((unsigned)u->resonance[0]<<3)|((unsigned)u->frozen[0]<<9));
}
static int page_values(struct opus_page *pg,short p[8]) {
 if(!pg || pg->kind!=OPUS_PAGE_EFFECT || pg->module!=&native_effect || !pg->par || pg->n<8)return 0;
 for(int j=0;j<8;++j)p[j]=pg->par[j];
 unpack_ui(p);p[6]=(short)((unsigned short)p[6]&7u);return 1;
}
static void commit(struct opus_page *pg,short p[8]) {
 pack_ui(p);for(int j=0;j<8;++j)pg->par[j]=p[j];
}
static int page_key(struct opus_page *pg,int code,int pressed) {
 short p[8];if(!page_values(pg,p) || opus_fn_held())return 0;
 if(pg->where==2)return 1;
 view_owner=0;view_frame=frame;
 if(pressed)delayed_expire();
 int handled=key(code,pressed);if(handled)commit(pg,p);return handled;
}

static int page_turn(struct opus_page *pg,int encoder,int detents) {
 short p[8];if(encoder<0 || encoder>3 || opus_fn_held() || !page_values(pg,p))return 0;
 if(pg->where==2)return 1;
 view_owner=0;view_frame=frame;
 int d=clamp(detents,-64,64);
 int shift=fw_shift_held();
 int extra=!shift && encoder==2 && page_ui()->res_edit[0];
 int tag=encoder+4*shift+8*extra;
 if(shift && encoder==2)delayed_expire();
 else d=clamp(encoder_speed_turn(&delayed_speed, native_self, delayed_expire, pg->par, tag, d),-64,64);

 if(knob(encoder,d)){commit(pg,p);return 1;}
 if(!(fw_shift_held() && DELAY.shift_knob && DELAY.shift_knob(p,encoder,d))) {
  int step=DELAY.limits[encoder][2]/8;
  if(fw_shift_held())step=step>8?step/8:1;
  p[encoder]=(short)clamp(p[encoder]+d*step,DELAY.limits[encoder][0],DELAY.limits[encoder][1]);
 }
 commit(pg,p);return 1;
}
static void screen_draw(void *scr,int dt) {
 (void)dt;struct opus_page pg;short p[8];
 if(!opus_page_now(&pg) || !page_values(&pg,p))return;
 for(int j=0;j<4;++j)p[j]=pg.live[j];
 begin();
 struct opus_fx_view v={0};view(p,&v);
 
 fw_screen_draw(scr);under(0);
 for(int j=0;j<4;++j) {
  int y=j&1?98:9;
  if(j<2){native_page_text(native_self,16+j*2,9,y,10,OPUS_COLOR_E(j),v.label[j]);native_page_text(native_self,17+j*2,9,y+20,24,OPUS_COLOR_TEXT,v.value[j]);}
  else{native_page_text_right(native_self,16+j*2,311,y,10,OPUS_COLOR_E(j),v.label[j]);native_page_text_right(native_self,17+j*2,311,y+20,24,OPUS_COLOR_TEXT,v.value[j]);}
 }
 if(pg.where==2)instrument_only_badge(native_self);
 /* Opus draws its FX OFF overlay after returning from this page. */
}
int opl_init(struct opl_self *s) {
 native_self=s;self=s;
 native_effect=DELAY;
 native_effect.shift_knob=0; /* Shift goes through page_turn with its instance. */
 for(int j=4;j<8;++j){native_effect.limits[j][0]=-32768;native_effect.limits[j][1]=32767;native_effect.limits[j][2]=0;}
 
 settings("smooth",opus_setting(s,"smooth"));opus_on_setting(s,settings);
 unsigned n=0;while(s->folder[n] && n<sizeof screen_path-12){screen_path[n]=s->folder[n];++n;}
 if(s->folder[n])return -1;
 const char *suffix="/screen.svg";while(*suffix)screen_path[n++]=*suffix++;screen_path[n]=0;
 if(!native_screen_start(s) || !native_screen_file(s,screen_path,ESC_SVG,sizeof ESC_SVG-1))return -2;
 native_effect.svg=screen_path;native_effect.screen_init=native_screen_init;native_effect.screen_draw=screen_draw;
 native_effect.page_key=page_key;native_effect.page_turn=page_turn;
 int result=opus_effect_register(s,&native_effect);return result<0?result:0;
}
