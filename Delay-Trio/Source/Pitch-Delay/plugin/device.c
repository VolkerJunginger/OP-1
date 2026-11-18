/* SPDX-License-Identifier: GPL-3.0-or-later
 * Opus adapter derived from opus.bitcrush and ESC_CODE's prior ports.
 */
#include "opus_plugin.h"
#include "native_screen.h"
#include "native_screen.h"
#include "op1fw.h"
#include "pitch_dsp.h"
#include "pitch_params.h"
#include "pitch_art.h"
#include "instrument_only.h"
struct native_ui { unsigned saved_extras[1]; int edit[1][2],acc[1][2],taken[2]; };
struct native_instance { struct pd_state dsp; struct native_ui ui; unsigned magic; };
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
#define extras (page_ui()->saved_extras)
#define edit (page_ui()->edit)
#define acc (page_ui()->acc)
#define taken (page_ui()->taken)
static int clamp(int x,int a,int b){return x<a?a:x>b?b:x;}
static struct opl_self *self;


static unsigned frame,view_frame;
static int view_owner=-1;
static short art[4];static int art_cents;
static char pitch_text[24],mix_text[24],oct_text[24],loop_text[24];
static int owner_now(void){return 0;}
static int octave(unsigned v){return clamp((int)(v&7),0,6)-3;}
static int depth(unsigned v){return 10+clamp((int)(v>>3),0,90);}
static void need(int stereo,struct opus_need *n) {
 n->state=stereo?0:sizeof(struct native_instance);
 n->bulk=stereo?0:sizeof(struct pd_buffer); n->fast=0; n->fast_moves=0;
}
static void init(struct opus_fx *fx) {
 if(fx->stereo || !fx->state)return;
 struct native_instance *i=fx->state;
 if(i->magic!=0x45534332u || opus_fx_list_shown()) {
  unsigned char *b=(unsigned char *)&i->ui;
  for(unsigned j=0;j<sizeof i->ui;++j)b[j]=0;
  i->ui.saved_extras[0]=123;
 }
 i->magic=0x45534332u;
 pd_init(&i->dsp);
}
static void mono(struct opus_fx *fx,short *buf,const short *input) {
 if(fx->stereo || !fx->state || !fx->bulk)return;
 struct native_instance *i=fx->state;
 short p[8];for(int j=0;j<8;++j)p[j]=input[j];
 struct pd_buffer *b=fx->bulk;
 if(b->owner!=&i->dsp){pd_init(&i->dsp);b->owner=&i->dsp;}
 unsigned ex=(unsigned short)p[4];
 pd_render(&i->dsp,b->ring,buf,0,p,octave(ex),depth(ex));
}
static void stereo(struct opus_fx *fx,short *l,short *r,const short *p)
{(void)fx;(void)l;(void)r;(void)p; /* Master is dry, no state or buffers. */}

static int focused(void){return view_owner==owner_now() && frame-view_frame<=1u && 1;}
static void store(int owner,unsigned value)
{extras[owner]=value&1023u;(void)owner;}
static int key(int code,int pressed)
{
 int which=code==0x3d?0:code==0x3e?1:-1;
 if(which>=0 && !pressed && taken[which]){taken[which]=0;return 1;}
 if(which>=0 && pressed && focused() && !opus_fn_held() && !fw_shift_held()) {
  int o=owner_now();edit[o][which]=!edit[o][which];acc[o][which]=0;taken[which]=1;return 1;
 }
 if(pressed && ((code>=1&&code<=9)||code==0x15||code==0x2c||(code>=0x2d&&code<=0x2f)||code==0x10||(code>=0x11&&code<=0x14)))view_owner=-1;
 return 0;
}
static int knob(int index,int detents)
{
 int which=(index&3)-2,o=owner_now();
 if(opus_fn_held() || which<0 || !focused() || !edit[o][which])return 0;
 int d=detents;
 if(d) {
  unsigned v=extras[o];int oct=octave(v),dep=depth(v);
  if(which==0)oct=clamp(oct+d,-3,3);else dep=clamp(dep+d,10,100);
  store(o,(unsigned)(oct+3)|((unsigned)(dep-10)<<3));
 }
 return 1;
}
static void begin(void){++frame;pd_art_begin_frame();}
static void under(unsigned short *buf)
{
 (void)buf;if(!focused())return;
 pd_art_draw(art,art_cents);
 native_page_text_right(self,3,311,29,16,OPUS_COLOR_TEXT,pitch_text);
 native_page_text_right(self,4,311,48,10,OPUS_COLOR_E3,oct_text);
 native_page_text_right(self,5,311,98,10,OPUS_COLOR_E4,"mix");
 native_page_text_right(self,2,311,118,16,OPUS_COLOR_TEXT,mix_text);
 native_page_text_right(self,1,311,136,8,OPUS_COLOR_E4,loop_text);
}
static void view(const short *p,struct opus_fx_view *v)
{
 static const char *const labels[4]={"time","feedback","pitchshift","mix"};
 static char time_text[24],feedback[24];int o=owner_now();view_owner=o;view_frame=frame;
 unsigned ex=extras[o];int oct=octave(ex),dep=depth(ex);
 for(int i=0;i<4;++i)art[i]=p[i];
 art_cents=pd_cents(pd_pitch_index(p[2]))+oct*1200;
 fw_snprintf(time_text,24,"%i ms",(pd_time(p[0])*10+220)/441);
 fw_snprintf(feedback,24,"%i%%",clamp(p[1],0,32767)*200/32767);
 fw_snprintf(pitch_text,24,"%s",pd_pitch_label(pd_pitch_index(p[2])));
 fw_snprintf(mix_text,24,"%i%%",clamp(p[3],0,32767)*100/32767);
 fw_snprintf(oct_text,24,edit[o][0]?"> oct %+i":"oct %+i",oct);
 fw_snprintf(loop_text,24,edit[o][1]?"> loop %i%%":"loop %i%%",dep);
 for(int i=0;i<4;++i){v->label[i]=i==3?"":labels[i];v->value[i]=i==0?time_text:i==1?feedback:"";}
 for(int i=0;i<3;++i)v->wave[i]=0;
 v->waves=0;v->caption=0;
}
static const struct opus_effect PITCH={
 .size=sizeof(struct opus_effect),.name="pitchdelay",.defaults={27008,8192,0,16384,0,0,0,0},
 .limits={{0,32767,8192},{0,32767,8192},{0,PD_PITCH_MAX,PD_PITCH_STEP*8},{0,32767,8192},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},
 .need=need,.init=init,.mono=mono,.stereo=stereo,.view=view
};

#undef extras
#undef edit
#undef acc
#undef taken

#include "native_adapter.h"
