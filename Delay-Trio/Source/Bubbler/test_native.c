#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "opus_plugin.h"
#include "op1fw.h"
static int (*registered_filter)(int,int);
void opus_on_key_filter(struct opl_self *self,int (*fn)(int,int))
{ (void)self;registered_filter=fn; }
static int t_list,t_shift,t_fn,t_frames,t_paths,t_text;
static void (*t_setting)(const char *,int);
static void *t_allocated[32];static unsigned t_count;
void *opus_alloc(struct opl_self *s,unsigned n){(void)s;assert(t_count<32);void *p=calloc(1,n);assert(p);t_allocated[t_count++]=p;return p;}
void *opus_fast(struct opl_self *s,unsigned n){return opus_alloc(s,n);}
void opus_on_setting(struct opl_self *s,void (*f)(const char *,int)){(void)s;t_setting=f;}
int opus_fx_list_shown(void){return t_list;}int opus_fn_held(void){return t_fn;}int fw_shift_held(void){return t_shift;}
int opus_setting(struct opl_self *s,const char *n){(void)s;(void)n;return 0;}
int fw_snprintf(char *s,unsigned n,const char *f,...){va_list a;va_start(a,f);int result=vsnprintf(s,n,f,a);va_end(a);return result;}

int opus_flash_headroom(void){return 1000000;}
int fw_yaffs_stat(const char *p,unsigned *s){(void)p;(void)s;return -1;}
int fw_yaffs_mkdir(const char *p,int m){(void)p;(void)m;return 0;}
int fw_yaffs_open(const char *p,int f,int m){(void)f;(void)m;assert(strstr(p,"/screen.svg"));return 1;}
int fw_yaffs_read(int f,void *p,unsigned n){(void)f;(void)p;(void)n;return -1;}
int fw_yaffs_write(int f,const void *p,unsigned n){assert(f==1&&p&&n);return (int)n;}
int fw_yaffs_close(int f){assert(f==1);return 0;}
/* These checks model native object boundaries and draw ordering; they are
 * not an emulation of TE's parser/rasterizer. Page callbacks are mocked; hardware timing is not emulated. */
static int t_document,t_raster,t_draw_active,t_constructed,t_sets;
static void *t_screen[4]={&t_document,0,0,0};
static unsigned char t_nodes[100][96],t_styles[100][20];
static void *t_text_mem[32];static int t_text_n;
static void t_native_drawing(void){assert(t_draw_active);}
void fw_screen_draw(void *scr){assert(scr==t_screen&&*(void **)scr==&t_document);++t_frames;t_draw_active=1;}
void *fw_svg_node_find(void *doc,const char *id,int warn){assert(doc==&t_document&&!warn);int i=-1;assert(sscanf(id,"boy%d",&i)==1&&i>=0&&i<100);return t_nodes[i];}
void *fw_screen_fill(void *scr,const char *id){assert(scr==t_screen);int i=-1;assert(sscanf(id,"boy%d",&i)==1&&i>=0&&i<100);return t_styles[i];}
void *fw_screen_line_style(void *scr,const char *id){return fw_screen_fill(scr,id);}
void *fw_screen_raster(void){t_native_drawing();return &t_raster;}
void fw_path_init(void *p){assert(p);memset(p,0,52);}
void fw_path_clear(void *p){assert(p);++t_paths;}
void fw_path_move_to(void *p,const int *xy,int absolute){assert(p&&xy&&absolute==1);}
void fw_path_line_to(void *p,const int *xy,int absolute){assert(p&&xy&&absolute==1);}
void fw_path_cubic(void *p,int a,int b,int c,int d,int e,int f,int absolute){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;assert(p&&absolute==1);}
void fw_path_close(void *p){assert(p);}
void fw_path_finish(void *p,int f,int l){assert(p&&((f==0&&l==1)||(f==1&&l==0)));}
void *fw_line_style_make(void *p,const unsigned char *rgba,int width,int cap,int join,int miter){assert(p&&rgba&&width>0&&cap==1&&join==1&&miter==4096);memcpy(p,rgba,4);return p;}
void fw_stroke_path(void *r,int unused,const void *s,void *p,const int *m,int extra){t_native_drawing();assert(r==&t_raster&&!unused&&s&&p&&m[0]==1024&&m[4]==1024&&m[8]==1024&&!extra);}
void fw_draw_path(void *r,int cache,const void *f,const void *l,void *p,const int *m,int override,int extra){t_native_drawing();assert(r==&t_raster&&!cache&&f&&l&&p&&m&&!override&&!extra);}
void *fw_text_new(void *mem,int x,int y,int size,const unsigned char *rgba,int align){assert(!t_draw_active&&t_text_n<32&&mem&&x>=0&&x<=320*1024&&y>=0&&y+size<=146&&rgba&&(align==0||align==2));memset(mem,0,164);t_text_mem[t_text_n++]=mem;++t_constructed;return mem;}
void fw_set_text(void *mem,const char *s){t_native_drawing();int found=0;for(int i=0;i<t_text_n;++i)if(t_text_mem[i]==mem)found=1;assert(found&&s);++t_sets;}
void fw_text_draw(void *mem,void *r,const int *m){t_native_drawing();assert(mem&&r==&t_raster&&m[0]==1024);++t_text;}
void orig_01a85750(void *p,int v){(void)p;(void)v;}

static const struct opus_effect *registered;
static struct opus_page shown;
int opus_page_now(struct opus_page *p){*p=shown;return shown.module!=0;}
int opus_effect_register(struct opl_self *s,const struct opus_effect *e){(void)s;registered=e;return 8;}
#include "device.c"
static struct opus_fx fx[2];
static short params[2][8];
static void select_page(int i){
 shown=(struct opus_page){.kind=OPUS_PAGE_EFFECT,.where=0,.slot=0,.on=1,.n=8,.key=-1,.par=params[i],.inst=&fx[i],.module=registered};
 for(int j=0;j<8;++j)shown.live[j]=params[i][j];
}
static void draw(int i){select_page(i);t_draw_active=0;registered->screen_draw(t_screen,16);t_draw_active=0;}
static void turn(int i,int enc,int d){select_page(i);assert(registered->page_turn(&shown,enc,d));}
static unsigned os_arrows, module_arrows;
static int dispatch_event(int code, int pressed)
{
 if (registered_filter && registered_filter(code, pressed)) return 1;
 /* Model the reported failure path: core/OS takes arrows before page_key. */
 if (code == 0x24 || code == 0x25) { ++os_arrows; return 1; }
 ++module_arrows;
 return registered->page_key(&shown, code, pressed);
}
static void push(int i,int code)
{
 select_page(i);
 assert(dispatch_event(code,1));
 assert(dispatch_event(code,0));
}
static void test_navigation(void)
{
 struct native_instance *a=fx[0].state,*b=fx[1].state;
 short saved[2][8];memcpy(saved,params,sizeof saved);
 select_page(0);t_shift=1;
 unsigned original_os=os_arrows;
 push(0,0x25);assert(a->ui.extra_page[0]==1 && b->ui.extra_page[0]==0);
 push(0,0x24);assert(a->ui.extra_page[0]==0);
 push(0,0x24);assert(a->ui.extra_page[0]==0);
 push(0,0x25);push(0,0x25);assert(a->ui.extra_page[0]==1);
 assert(os_arrows==original_os);
 t_shift=0;draw(0);assert(a->ui.extra_page[0]==1);
 t_shift=1;draw(0);assert(a->ui.extra_page[0]==1);
 push(1,0x24);assert(b->ui.extra_page[0]==0 && a->ui.extra_page[0]==1);
 push(1,0x25);assert(b->ui.extra_page[0]==1);
 push(0,0x24);assert(a->ui.extra_page[0]==0 && b->ui.extra_page[0]==1);
 /* Release remains paired after Shift release and a different page/instance. */
 select_page(0);assert(dispatch_event(0x25,1));t_shift=0;
 shown.module=0;assert(dispatch_event(0x25,0));
 assert(os_arrows==original_os);
 select_page(0);t_shift=1;
 /* No frame freshness requirement and no inherited state on other pages. */
 frame_number+=100;viewed_frame=0;viewed_owner=-1;
 push(0,0x24);assert(a->ui.extra_page[0]==0);
 assert(os_arrows==original_os);
 t_shift=0;push(0,0x25);assert(a->ui.extra_page[0]==0 && os_arrows==original_os+2);
 t_shift=1;t_fn=1;push(0,0x25);assert(a->ui.extra_page[0]==0);t_fn=0;
 select_page(0);shown.on=0;dispatch_event(0x25,1);dispatch_event(0x25,0);assert(a->ui.extra_page[0]==0);
 select_page(0);shown.module=0;dispatch_event(0x25,1);dispatch_event(0x25,0);assert(a->ui.extra_page[0]==0);
 select_page(0);shown.where=2;dispatch_event(0x25,1);dispatch_event(0x25,0);assert(a->ui.extra_page[0]==0);
 select_page(0);shown.inst=0;dispatch_event(0x25,1);dispatch_event(0x25,0);assert(a->ui.extra_page[0]==0);
 assert(!memcmp(saved,params,sizeof saved));
 t_shift=0;a->ui.extra_page[0]=b->ui.extra_page[0]=0;select_page(0);
 puts("Shift arrows passed through pre-core routing, page/instance guards and paired releases");
}
static void process(int i,short buf[64]){registered->mono(&fx[i],buf,params[i]);}
static void (*expire_turn)(void);
void opus_key_timer(struct opl_self *self, unsigned ms, void (*fn)(void))
{ (void)self; assert(ms == 120); expire_turn = fn; }

int main(void){
 struct opl_self self={sizeof self,"esc.test","/yaffs2/mod/esc.test",0};
 assert(opl_init(&self)==0&&registered&&registered->page_key&&registered->page_turn&&registered->screen_draw);
 struct opus_need n={0};registered->need(0,&n);
 for(int i=0;i<2;++i){fx[i].state=calloc(1,n.state);fx[i].bulk=calloc(1,n.bulk);fx[i].fast=n.fast?calloc(1,n.fast):0;assert(fx[i].state&&fx[i].bulk);memcpy(params[i],registered->defaults,sizeof params[i]);registered->init(&fx[i]);}
 registered->screen_init(t_screen);draw(0);assert(t_frames==1&&t_paths>0&&t_text>=8);
 test_navigation();draw(0);
 /* Normal page turns arrive in detents, with one replaceable timer. */
 short original_time=params[0][0];
 int step=registered->limits[0][2]/8;
 params[0][0]=10*step;
 turn(0,0,1);turn(0,0,1);turn(0,0,1);
 assert(params[0][0]==14*step && expire_turn);
 expire_turn();
 turn(0,0,1);assert(params[0][0]==15*step);
 turn(0,0,-1);assert(params[0][0]==14*step);
 expire_turn();
 const int factors[]={1,1,2,2,2,4,4,4,4,8};
 for(unsigned i=0;i<sizeof factors/sizeof factors[0];++i) {
  params[0][0]=step;
  turn(0,0,1);
  assert(params[0][0]==(1+factors[i])*step);
 }
 short second_time=params[1][0];
 params[1][0]=step;
 turn(1,0,1);assert(params[1][0]==2*step);
 params[1][0]=second_time; /* Restore the second instance for recall checks. */
 expire_turn();
 params[0][0]=original_time;
 int objects=t_constructed,sets=t_sets;draw(0);assert(t_constructed==objects&&t_sets==sets);
 short original[8];memcpy(original,params[1],sizeof original);
 turn(0,0,2);assert(params[0][0]!=params[1][0]);assert(!memcmp(original,params[1],sizeof original));
#ifdef TEST_BUBBLER
 t_shift=1;push(0,0x25);turn(0,0,2); /* Off -> key -> +3 */
 short saved[8];memcpy(saved,params[0],sizeof saved);
 struct native_instance *a=fx[0].state,*b=fx[1].state;
 assert(a->ui.transpose==8&&b->ui.transpose==0);
 select_page(1);assert(page_ui()==&b->ui);push(1,0x25);turn(1,0,-1);assert(b->ui.transpose==7&&a->ui.transpose==8);
 t_shift=0;turn(0,2,-2);t_shift=1;draw(0);assert(a->ui.transpose==8);
 memcpy(params[0],saved,sizeof saved);t_shift=0;
 for(int mode=0;mode<10;++mode){
  select_page(0);a->ui.transpose=mode;short q[8];memcpy(q,params[0],sizeof q);pack_ui(q);memcpy(params[0],q,sizeof q);a->ui.transpose=0;unpack_ui(q);assert(a->ui.transpose==mode);
  short audio[64]={0};process(0,audio);assert(a->dsp.pitch_mode==mode);
 }
 assert(esc_density_turn(0,1)==1&&esc_density_turn(20,1)==22&&esc_density_turn(50,1)==53&&esc_density_turn(80,1)==84&&esc_density_turn(100,1)==100);
 memcpy(params[0],saved,sizeof saved);

#endif
#ifdef TEST_DELAY
 push(0,0x3d);turn(0,2,6);assert(((unsigned short)params[0][6]>>3 &63u)==12);assert(params[1][6]==0);
 push(0,0x3e);assert((unsigned short)params[0][6]&512u);assert(!((unsigned short)params[1][6]&512u));push(0,0x3e);
 t_shift=1;turn(0,2,1);assert((params[0][6]&7)==1);draw(0);struct opus_fx_view v={0};short plain[8];select_page(0);page_values(&shown,plain);view(plain,&v);assert(!strcmp(v.value[2],"tri"));t_shift=0;
 assert(t_setting);t_setting("smooth",1);t_setting("smooth",0);
#endif
#ifdef TEST_PITCH
 push(0,0x3d);turn(0,2,1);assert((params[0][4]&7)==4&&(params[1][4]&7)==3);
 push(0,0x3e);turn(0,3,5);assert(params[0][4]!=params[1][4]);
#endif
 /* Audio and history are independent even when the same type runs twice. */
 short buf[64]={0},zero[64]={0};buf[0]=12000;
 process(0,buf);struct native_instance snapshot=*(struct native_instance *)fx[0].state;
 void *history=malloc(n.bulk);memcpy(history,fx[0].bulk,n.bulk);
 for(int k=0;k<800;++k){memset(buf,0,sizeof buf);process(1,buf);assert(!memcmp(buf,zero,sizeof buf));}
 assert(!memcmp(&snapshot,fx[0].state,sizeof snapshot)&&!memcmp(history,fx[0].bulk,n.bulk));free(history);
 for(int k=0;k<2500;++k){for(int j=0;j<64;++j)buf[j]=(short)(j*201-k);process(k&1,buf);}
 /* UI state follows the instance, and all saved controls round-trip in params. */
 draw(0);draw(1);draw(0);t_fn=1;select_page(0);assert(!registered->page_key(&shown,0x3d,1));assert(!registered->page_turn(&shown,0,1));t_fn=0;
 t_list=1;registered->init(&fx[0]);t_list=0;memcpy(params[0],registered->defaults,sizeof params[0]);draw(0);
 struct native_instance *reset=fx[0].state;
#ifdef TEST_BUBBLER
 assert(!reset->ui.transpose&&!reset->ui.extra_page[0]&&!reset->ui.res_edit[0]);
#else
#ifdef TEST_DELAY
 assert(!reset->ui.resonance[0]&&!reset->ui.frozen[0]&&!reset->ui.res_edit[0]);
#else
 assert(reset->ui.saved_extras[0]==123&&!reset->ui.edit[0][0]&&!reset->ui.edit[0][1]);
#endif
#endif
 struct opus_need master={1,1,1,1};registered->need(1,&master);assert(!master.state&&!master.bulk&&!master.fast);
 short left[64],right[64],copy[64];for(int j=0;j<64;++j)left[j]=right[j]=copy[j]=(short)j;
 struct opus_fx m={.stereo=1};registered->stereo(&m,left,right,params[0]);assert(!memcmp(left,copy,sizeof copy)&&!memcmp(right,copy,sizeof copy));
 for(int i=0;i<2;++i){free(fx[i].state);free(fx[i].bulk);free(fx[i].fast);}
 for(unsigned j=0;j<t_count;++j)free(t_allocated[j]);
 puts("Native two-instance controls, independent audio/history, drawing, defaults and dry master passed");
 return 0;
}
