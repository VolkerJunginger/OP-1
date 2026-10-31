#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opus_plugin.h"
#include "bubbler_dsp.h"
#include "bubbler_params.h"
#include "bubbler_art.h"
static const struct opus_effect *effect;
static int shifted,owner,fxpage=1,fxlist,fn;
static int (*key_handler)(int,int);
static void (*frame_handler)(void);
static void (*under_handler)(unsigned short *);
static void (*post_handler)(unsigned short *);
static unsigned short framebuffer[400*256];
void opus_on_frame(struct opl_self *self,void (*f)(unsigned short *)){(void)self;post_handler=f;}
static char counter[20],mode_text[16],orange_text[16];
static int mode_setting=5;
static void (*setting_handler)(const char *,int);
int opus_setting(struct opl_self *s,const char *n){(void)s;(void)n;return mode_setting;}
void opus_on_setting(struct opl_self *s,void (*f)(const char *,int)){(void)s;setting_handler=f;}
int opus_set_setting(struct opl_self *s,const char *n,int v){(void)s;(void)n;mode_setting=v;return 1;}
static int forwarded_notes;
void orig_01a85750(void *e,int p){(void)e;(void)p;++forwarded_notes;}
extern void bubbler_note_on(void *,int);

static int capture,cache_enabled,draw_calls,put_calls,keep_calls,fill_calls;
static unsigned colour_key;
static int panel_rows=320;
struct command {int op,p[6];};
struct object {struct command cmds[64];int count,colour,alpha,width,fill,circle,x,y,r;};
static struct object current,objects[128];static int object_count;
static void cmd(int op,int a,int b,int c,int d,int e,int f) {
 assert(current.count<64);struct command *o=&current.cmds[current.count++];
 o->op=op;o->p[0]=a;o->p[1]=b;o->p[2]=c;o->p[3]=d;o->p[4]=e;o->p[5]=f;
}
static void emit(int colour,int alpha,int width,int fill){
 ++draw_calls;if(!capture)return;assert(object_count<128);
 current.colour=colour;current.alpha=alpha;current.width=width;current.fill=fill;objects[object_count++]=current;
}
void opus_on_frame_under(struct opl_self *self,void (*f)(unsigned short *)){(void)self;under_handler=f;}
void opus_page_fill(int x0,int y0,int x1,int y1,int colour){
 ++fill_calls;assert(colour==OPUS_COLOR_BACK);
 for(int x=x0*panel_rows/320;x<=(x1*panel_rows/320)&&x<panel_rows;++x)
  for(int y=y0*panel_rows/320;y<=(y1*panel_rows/320)&&y<panel_rows/2;++y)
   framebuffer[256*(panel_rows-1-x)+y]=0;
}
unsigned opus_art_colours(void){return colour_key;}
unsigned short *opus_page_pixels(int *rows){*rows=panel_rows;return cache_enabled?framebuffer:0;}
int opus_page_keep(unsigned short *out,unsigned cap,int x0,int x1,int y0,int y1){(void)out;assert(cap==8192);assert(x0>=0&&x1<panel_rows&&y0>=0&&y1<panel_rows/2);++keep_calls;return 1;}
void opus_page_put(const unsigned short *out,int used){(void)out;assert(used==1);++put_calls;}
void opus_page_text_right(struct opl_self *self,int slot,int x,int y,int size,int colour,const char *text){
 (void)self;
 if(slot==2){assert(x==311&&y==104&&size==16&&colour==OPUS_COLOR_TEXT);snprintf(orange_text,sizeof orange_text,"%s",text);return;}
 if(slot==1){assert(x==311&&y==123&&size==10&&colour==OPUS_COLOR_E4);snprintf(mode_text,sizeof mode_text,"%s",text);return;}
 assert(slot==0&&x==172&&y==10&&size==12&&colour==OPUS_COLOR_DIM);
 snprintf(counter,sizeof counter,"%s",text);
}

void opus_page_path(void){memset(&current,0,sizeof current);}
void opus_page_move(int x,int y){cmd('M',x,y,0,0,0,0);}
void opus_page_line_to(int x,int y){cmd('L',x,y,0,0,0,0);}
void opus_page_curve_to(int a,int b,int c,int d,int e,int f){cmd('C',a,b,c,d,e,f);}
void opus_page_close(void){cmd('Z',0,0,0,0,0,0);}
void opus_page_stroke(int width,int colour,int alpha,int cap,int join){assert(cap==OPUS_CAP_ROUND&&join==OPUS_JOIN_ROUND);emit(colour,alpha,width,0);}
void opus_page_fill_path(int colour,int alpha){emit(colour,alpha,0,1);}
void opus_page_dot(int x,int y,int r,int filled,int colour){opus_page_path();current.circle=1;current.x=x;current.y=y;current.r=r;emit(colour,255,256,filled);}
void opus_page_stroke_line(int x0,int y0,int x1,int y1,int width,int colour){opus_page_path();cmd('M',x0,y0,0,0,0,0);cmd('L',x1,y1,0,0,0,0);emit(colour,255,width,0);}

static unsigned char tables[BB_FAST_TABLE_BYTES] __attribute__((aligned(32)));
int opus_effect_register(struct opl_self *self,const struct opus_effect *e){(void)self;effect=e;return 8;}
void *opus_fast(struct opl_self *self,unsigned n){(void)self;static unsigned char clip[BB_FAST_CLIP_BYTES] __attribute__((aligned(32)));if(n==sizeof clip)return clip;assert(n==sizeof tables);return tables;}
void opus_on_key(struct opl_self *self,int (*f)(int,int)){(void)self;key_handler=f;}
void opus_on_frame_begin(struct opl_self *self,void (*f)(void)){(void)self;frame_handler=f;}
int opus_fx_owner(void){return owner;}
int opus_fx_page_shown(void){return fxpage;}
int opus_fx_list_shown(void){return fxlist;}
int opus_fn_held(void){return fn;}
int fw_shift_held(void){return shifted;}
int fw_snprintf(char *dst,unsigned n,const char *fmt,...){va_list a;va_start(a,fmt);int r=vsnprintf(dst,n,fmt,a);va_end(a);return r;}
static struct opus_fx_view view(short p[8]){struct opus_fx_view v={0};effect->view(p,&v);return v;}
static void caption(short *p,const char *prefix){struct opus_fx_view v=view(p);assert(v.waves==0 && v.caption==0);under_handler(0);assert(!strncmp(counter,prefix,3));}
static void arrow(int code){assert(key_handler(code,1)==1);assert(key_handler(code,0)==1);}
static void snapshot(const char *name,short *p){
 mode_text[0]=0;capture=1;cache_enabled=0;object_count=0;
 struct opus_fx_view v=view(p);under_handler(0);capture=0;
 printf("{\"state\":\"%s\",\"labels\":[",name);
 for(int i=0;i<4;++i)printf("%s\"%s\"",i?",":"",v.label[i]);
 printf("],\"values\":[");for(int i=0;i<4;++i)printf("%s\"%s\"",i?",":"",(i==3&&!v.value[i][0])?orange_text:v.value[i]);
 printf("],\"caption\":\"%s\",\"mode\":\"%s\",\"art\":[",counter,mode_text);
 for(int i=0;i<object_count;++i){
  struct object *o=&objects[i];
  printf("%s{\"colour\":%i,\"alpha\":%i,\"width\":%i,\"fill\":%i,\"circle\":%i,\"x\":%i,\"y\":%i,\"r\":%i,\"commands\":[",i?",":"",o->colour,o->alpha,o->width,o->fill,o->circle,o->x,o->y,o->r);
  for(int k=0;k<o->count;++k){struct command *c=&o->cmds[k];int n=c->op=='C'?6:c->op=='Z'?0:2;printf("%s[\"%c\"",k?",":"",c->op);for(int j=0;j<n;++j)printf(",%.4f",c->p[j]/256.0);printf("]");}
  printf("]}");
 }
 printf("]}\n");
}
static void art_test(short *p){
 shifted=0;view(p);cache_enabled=1;int before=draw_calls;
 under_handler(0);assert(draw_calls>before&&keep_calls==0);
 before=draw_calls;int puts=put_calls;under_handler(0);assert(draw_calls>before&&put_calls==puts);
 ++colour_key;under_handler(0);assert(draw_calls>before);
 for(int k=0;k<2;++k){panel_rows=k?376:400;before=draw_calls;under_handler(0);assert(draw_calls>before);}
 panel_rows=320;
 cache_enabled=0;capture=1;
 for(int m=0;m<3;++m){
  p[3]=(short)(m==0?0:m==1?16384:32767);p[1]=(short)(m?32767:0);object_count=0;view(p);under_handler(0);
  int red=0,green=0;
  for(int i=0;i<object_count;++i){if(objects[i].colour==OPUS_COLOR_E4){red++;assert(objects[i].alpha==(m==0?128:m==1?192:255));}if(objects[i].circle&&objects[i].colour==OPUS_COLOR_E2)green++;}
  assert(red>0);assert(green==(m?12:1));
 }
 capture=0;view(p);before=draw_calls;fxpage=0;under_handler(0);assert(draw_calls==before);fxpage=1;
}
static void repair_test(short *p){
 cache_enabled=1;shifted=0;fxpage=1;fxlist=0;
 for(int panel=0;panel<3;++panel){
  panel_rows=panel==0?320:panel==1?376:400;
  for(int variant=0;variant<5;++variant){
   memset(framebuffer,0,sizeof framebuffer);
   /* Native readouts may already exist: artwork must preserve them. */
   framebuffer[256*(panel_rows-1-245*panel_rows/320)+115*panel_rows/320]=0xffff;
   frame_handler();view(p);under_handler(0);
   assert(framebuffer[256*(panel_rows-1-245*panel_rows/320)+115*panel_rows/320]==0xffff);
   assert(fill_calls==0&&keep_calls==0&&put_calls==0);
   int x0=(variant==2?130:104)*panel_rows/320,x1=232*panel_rows/320;
   int y0=76*panel_rows/320,height=variant==1?6:1;
   unsigned short colour=variant==3?0xf800:0xffff;
   for(int x=x0;x<=x1;++x)for(int y=y0;y<y0+height;++y)framebuffer[256*(panel_rows-1-x)+y]=colour;
   if(variant==4)bb_art_begin_frame();
   post_handler(0);
   assert(framebuffer[256*(panel_rows-1-x0)+y0]==(variant==0?0:colour));
  }
 }
 /* Also simulate the actual device ordering: native plot and values exist
  * BEFORE the underlay, rather than being drawn after it. */
 for(int panel=0;panel<3;++panel){
  panel_rows=panel==0?320:panel==1?376:400;
  for(int variant=0;variant<5;++variant){
   memset(framebuffer,0,sizeof framebuffer);
   int x0=(variant==2?130:104)*panel_rows/320,x1=232*panel_rows/320;
   int y0=76*panel_rows/320,height=variant==1?6:variant==4?3:1;
   unsigned short colour=variant==3?0xf800:0xffff;
   for(int x=x0;x<=x1;++x)for(int y=y0;y<y0+height;++y)framebuffer[256*(panel_rows-1-x)+y]=colour;
   /* Simulate the face crossing the line: coloured art must survive. */
   int face=180*panel_rows/320;
   framebuffer[256*(panel_rows-1-face)+y0]=0xf800;
   int digit=256*(panel_rows-1-245*panel_rows/320)+115*panel_rows/320;
   framebuffer[digit]=0xffff;
   frame_handler();view(p);under_handler(0);
   assert(framebuffer[256*(panel_rows-1-x0)+y0]==((variant==0||variant==4)?0:colour));
   assert(framebuffer[256*(panel_rows-1-face)+y0]==0xf800);
   assert(framebuffer[digit]==0xffff);
   assert(fill_calls==0&&keep_calls==0&&put_calls==0);
  }
 }
 panel_rows=320;cache_enabled=0;
}
static void stable_art_test(short *p){
 shifted=0;cache_enabled=0;capture=1;object_count=0;view(p);under_handler(0);
 struct object reference[128];int count=object_count;memcpy(reference,objects,sizeof reference);
 shifted=1;view(p);key_handler(0x24,1);key_handler(0x24,0);
 for(int page=0;page<2;++page){
  if(page){view(p);arrow(0x25);}
  for(int encoder=0;encoder<4;++encoder){
   effect->shift_knob(p,encoder,1);object_count=0;view(p);under_handler(0);
   assert(object_count==count && !memcmp(reference,objects,(size_t)count*sizeof *objects));
  }
 }
 capture=0;
}
static void parameters(void){
 unsigned seed=42;
 for(int n=0;n<100000;++n){
  short p[8]={100,200,300,400};struct bb_params a,b;
  seed=seed*1664525u+1013904223u;
  a.scatter=seed&32767;a.tone=(seed>>15)&127;a.pitch=0;a.reverse=(seed>>10)&32767;
  a.size=seed&32767;a.resonance=seed%64;
  a.density=seed%101;a.size_var=(seed>>5)&32767;a.pitch_var=(seed>>23)%101;a.jitter=(seed>>8)&32767;
  bb_encode(p,&a);assert(p[7]<0);short saved[8];memcpy(saved,p,sizeof saved);bb_decode(saved,&b);
  assert(p[0]==100 && p[1]==200 && p[3]==400);
  assert(abs(a.size-b.size)<=683 && a.resonance==b.resonance);
  assert(a.tone==b.tone && a.pitch==b.pitch && a.density==b.density && a.pitch_var==b.pitch_var);
  assert(abs(a.scatter-b.scatter)<=65 && abs(a.reverse-b.reverse)<=65 && abs(a.size_var-b.size_var)<=65 && abs(a.jitter-b.jitter)<=65);
 }

}
int main(int argc,char **argv){
 struct opl_self self={0};assert(opl_init(&self)==0);assert(effect && key_handler && frame_handler && under_handler && post_handler);
 struct opus_need n={0};effect->need(0,&n);assert(n.bulk==BB_STORAGE*2 && n.fast==256 && n.state==sizeof(struct bb_state));
 short p[8];memcpy(p,effect->defaults,sizeof p);assert(mode_setting==0);parameters();
 caption(p,"1/3");assert(!strcmp(view(p).label[2],"density"));assert(!key_handler(0x3e,1));
 shifted=1;caption(p,"2/3");assert(!strcmp(view(p).label[1],"start var")&&!strcmp(view(p).label[2],"size"));assert(!key_handler(0x3e,1));
 {struct bb_params before,after;bb_decode(p,&before);assert(effect->shift_knob(p,1,1));bb_decode(p,&after);assert(after.jitter>before.jitter&&after.tone==before.tone);}

 arrow(0x25);caption(p,"3/3");assert(!strcmp(view(p).label[2],"reverse")&&!strcmp(view(p).label[0],"filter freq"));
 shifted=0;caption(p,"1/3");shifted=1;caption(p,"3/3");
 arrow(0x25);caption(p,"3/3");arrow(0x24);caption(p,"2/3");arrow(0x24);caption(p,"2/3");
 for(int page=0;page<2;++page){
  if(page)arrow(0x25);
  for(int k=0;k<4;++k){assert(effect->shift_knob(p,k,-32768));assert(effect->shift_knob(p,k,32767));}
 }
 /* The key-up remains consumed after Shift release or leaving this screen. */
 caption(p,"3/3");assert(key_handler(0x24,1));shifted=0;fxpage=0;assert(key_handler(0x24,0));
 assert(!key_handler(0x24,1));fxpage=1;assert(!key_handler(0x24,1));shifted=1;
 fxlist=1;assert(!key_handler(0x24,1));fxlist=0;fn=1;assert(!key_handler(0x24,1));fn=0;
 for(int k=0;k<3;++k)frame_handler();assert(!key_handler(0x24,1));
 caption(p,"2/3");assert(!key_handler(0x06,1));assert(!key_handler(0x25,1));
 caption(p,"2/3");owner=1;assert(!key_handler(0x25,1));caption(p,"2/3");arrow(0x25);caption(p,"3/3");
 owner=0;caption(p,"2/3");
 arrow(0x25);caption(p,"3/3");
 for(int i=1;i<=6;++i){assert(key_handler(0x3e,1));assert(key_handler(0x3e,0));assert(mode_setting==i%6);}
 fn=1;assert(!key_handler(0x3e,1));fn=0;
 int event[4]={67,100,0,0};bubbler_note_on(event,0);assert(forwarded_notes==1);
 assert(setting_handler);setting_handler("transpose",0);
 stable_art_test(p);art_test(p);repair_test(p);
 if(argc>1 && !strcmp(argv[1],"--snapshots")){
  memcpy(p,effect->defaults,sizeof p);shifted=0;snapshot("page-1-default",p);
  shifted=1;arrow(0x24);snapshot("page-2-default",p);arrow(0x25);snapshot("page-3-default",p);
  shifted=0;snapshot("shift-released",p);shifted=1;snapshot("shift-reopened",p);
  shifted=0;p[3]=0;p[1]=0;snapshot("dry-low-feedback",p);
  p[3]=32767;p[1]=32767;snapshot("wet-high-feedback",p);
  for(int max=0;max<2;++max){
   p[0]=p[1]=p[2]=p[3]=max?32767:0;
   struct bb_params v={max?32767:0,max?127:0,max?12:-12,max?32767:0,max?100:0,max?32767:0,max?100:0,max?32767:0,max?32767:0,max?63:0};
   bb_encode(p,&v);shifted=0;snapshot(max?"page-1-max":"page-1-min",p);
   shifted=1;arrow(0x24);snapshot(max?"page-2-max":"page-2-min",p);
   arrow(0x25);snapshot(max?"page-3-max":"page-3-min",p);
   if(max){shifted=0;p[3]=0;snapshot("dry-high-feedback",p);}
  }
  shifted=1;arrow(0x25);setting_handler("transpose",1);snapshot("page-3-key",p);setting_handler("transpose",0);
 }else puts("PASS: native registration/memory; fresh Density layout; 100,000 packed round trips; three pages, counters, remembered Shift page, scoped arrows, key-up pairing, readout-safe uncached art, mix fade and feedback bubble counts");
 return 0;
}
