/* Fluke global-key variation. Original OP-1 firmware #246 / Opus 2.8+. */
#include "opus_plugin.h"
#include "op1fw.h"
#include "core.h"
#include "../../grip-release/Hits/encoder_speed.h"
static struct encoder_speed fluke_speed;
static void fluke_expire(void) { fluke_speed.burst = 0; }

static struct opl_self *encoder_plugin;
#include "drum.h"
#include "art.h"
#include "../../grip-release/Hits/fluke_link.h"
static Pattern banks[2];
static Pattern *current_pattern=&banks[0],*previous_pattern=&banks[1];
#define pattern (*current_pattern)
static int turn_acc[8];
static Player run;
static unsigned key_gestures;
static char art_path[256];
static unsigned texts[34][41];
static int text_ready;
static int draw_mono,draw_global_key=-1;
static int drum_keyboard_transpose=1;
static volatile unsigned hits_overlay_on;
static volatile unsigned drum_fx_on;
static struct opus_sequencer seq;
/* Opus choices: Play & Rec, Rec (default), Off. */
static int autostart=1,tape_playing;
static unsigned char mono_colors[16][4];
static void *color_art,*mono_art,*popup;
static void *header_dividers[2];
static void *keyboard_beds[2],*keyboard_dots[2][12];
static void snapshot_tick(void);
static void drum_snapshot_tick(void);
static int player_owner(void *p);
static uint32_t entropy(void);
static void connect_visible_hits(void);
static const int identity[9]={1024,0,0,0,1024,0,0,0,1024};
static void *note_nodes[16],*active_nodes[16],*rest_nodes[16];
static void *mono_notes[16],*mono_active[16],*mono_rests[16],*melody_lines[2][16];
static unsigned char rgba[5][4]={{105,142,255,255},{0,237,149,255},{205,205,255,255},{255,66,63,255},{150,150,150,255}};
static const unsigned char hold_dim[4]={102,26,25,255};
static const unsigned char page_color[4]={111,113,115,255};
static const unsigned char root_color[4]={255,240,207,255};
static const unsigned char soft_blue[4]={155,174,224,255};
static const unsigned char key_selected_green[4]={8,204,131,255};
static const unsigned char soft_green[4]={138,203,177,255};
static const unsigned char caption_white[4]={255,255,255,255};
static void note_on(int pitch,int vel){int ev[4]={pitch,vel,1,0};fw_vc_note_on(ev,opus_seq_mode()==1?drum_run.transpose:0);}
static void note_off(int pitch){int ev[4]={pitch,0,1,0};fw_vc_note_off(ev);}
static int mode_ok(int mode){return mode==0||mode==1;}
static int active_mode(void){int m=opus_seq_mode();return m<0?opus_instrument_now():m;}
static int noop(void *p){(void)p;return 0;}
#include "host_sync.h"
static void halt(void *p){int owner=player_owner(p);host_sync.pending[owner]=host_sync.auto_hold[owner]=0;if(owner==1){drum_stop();drum_run.latch=0;}else {stop(&run);run.latch=0;}}
static int hits_cut_held;
static int drum_edit_exit_key_held;
static int companion_edit_pad = -1;
static volatile int hits_linked;

static void leave_drum_edit(void)
{
    companion_edit_pad = -1;
    if (drum_run.edit >= 0 && drum_run.edit < drums.count)
        drum_pick(drums.keys[drum_run.edit], 0);
}

static void fluke_hits_select_pad(int pad)
{
    if ((unsigned)pad >= 24) {
        /* Keep lane Edit until the normal FLUKE key path handles the press. */
        companion_edit_pad = -1;
        return;
    }
    if (hits_cut_held || drum_run.cut_held)
        return;
    opus_drum_select_key(pad);
    if (host_sync.selected[1] && (drum_run.edit < 0 || drums.keys[drum_run.edit] != pad))
        drum_pick(pad, entropy());
    if (host_sync.selected[1] && drum_run.edit >= 0 && drums.keys[drum_run.edit] == pad) {
        companion_edit_pad = -1;
    } else {
        leave_drum_edit();
        companion_edit_pad = pad;
    }
}

static void drum_cut(int *held, int pressed)
{
    *held = !!pressed;
    if (pressed && !fw_shift_held() && drum_run.edit < 0)
        drum_run.jump_pending = 1;
}

static int fluke_hits_cut(int pressed)
{
    if (!host_sync.selected[1]) {
        hits_cut_held = 0;
        return 0;
    }
    drum_cut(&hits_cut_held, pressed);
    return 1;
}

static int named_module(const char *name, const char *wanted);

enum companion_page { COMPANION_NONE, COMPANION_HITS, COMPANION_FLUX };

static enum companion_page drum_companion(const struct opus_page *page)
{
    if (page->where != 1 || !page->module)
        return COMPANION_NONE;
    if (page->kind == OPUS_PAGE_DRUM && page->t == 1) {
        const struct opus_engine *engine = page->module;
        if (named_module(engine->name, "hits"))
            return COMPANION_HITS;
    }
    if (page->kind == OPUS_PAGE_LFO && page->t == 4) {
        const struct opus_lfo_type *lfo = page->module;
        if (named_module(lfo->name, "flux"))
            return COMPANION_FLUX;
    }
    return COMPANION_NONE;
}

static int down(void *p,int key,int shift){
 int owner=player_owner(p);if(!mode_ok(owner)||key<0||key>23)return 0;
 if(owner==1){
  unsigned bit=1u<<key;if((drum_run.gestures|(unsigned)drum_run.held)&bit)return 1;
  int shifted = shift || fw_shift_held();
  int edited_key = drum_run.edit >= 0 && drum_run.edit < drums.count &&
                   drums.keys[drum_run.edit] == key;
  if (hits_cut_held || drum_run.cut_held) {
   drum_run.gestures |= bit;
   if (edited_key) drum_remove(key);
   return 1;
  }
  struct opus_page page;
  enum companion_page companion = opus_page_now(&page) ? drum_companion(&page) : COMPANION_NONE;
  if (shifted) {
   /* The companion key callback already selected the pad. The stock drum
    * handler still receives it, while the FLUKE pattern keeps its lanes. */
   if (hits_linked && companion != COMPANION_NONE)
    return 0;
   if (!edited_key)
    drum_pick(key, entropy());
   if (drum_run.edit >= 0 && drums.keys[drum_run.edit] == key) {
    companion_edit_pad = -1;
    opus_drum_select_key(key);
   }
   if (drum_run.latch && companion == COMPANION_NONE) {
    drum_run.gestures |= bit;
    return 1;
   }
   return 0;
  }
  int leaving_edit = drum_run.edit >= 0 && drum_run.edit < drums.count;
  companion_edit_pad = -1;
  if (leaving_edit)
   leave_drum_edit();
  if(!leaving_edit&&drum_run.edit<0&&!drum_keyboard_transpose&&companion!=COMPANION_HITS)return 0;
  if(drum_run.edit<0){int ev[4];fw_make_midinote(ev,key,100);drum_run.transpose=ev[0]-60;}
  drum_run.held|=(int)bit;if(!drum_run.running)host_sync_start(1);return 1;
 }
 unsigned bit=1u<<key;
 if((key_gestures|(unsigned)run.held)&bit)return 1;
 int ev[4];fw_make_midinote(ev,key,100);
 int root=bounded(ev[0],0,127)%12;
 if(global_key_enabled&&(shift||fw_shift_held())){
  key_gestures|=bit;
  global_key=global_key==root?-1:root;
  return 1;
 }
 /* Out-of-key notes sustain playback but do not select a new root. */
 if(global_key_active()&&!global_note_allowed(&pattern,root)){
  run.held|=(int)bit;
  if(!run.running)start(&run);
  return 1;
 }
 int octave=bounded(ev[0]/12-1,lo[REGISTER],hi[REGISTER]);
 pattern.p[ROOT]=root;pattern.p[REGISTER]=octave;
 run.transpose=0;run.held|=1<<key;
 if(!run.running)start(&run);
 return 1;
}
static void up(void *p,int key){
 if(key<0||key>23)return;
 if(player_owner(p)==1){unsigned bit=1u<<key;if(drum_run.gestures&bit){drum_run.gestures&=~bit;return;}if(!((unsigned)drum_run.held&bit))return;drum_run.held&=~(int)bit;if(!drum_run.held&&!drum_run.latch)drum_stop();return;}
 unsigned bit=1u<<key;
 if(key_gestures&bit){key_gestures&=~bit;return;}
 if(!((unsigned)run.held&bit))return;
 run.held&=~(int)bit;if(!run.held&&!run.latch)stop(&run);
}
static void tick(void *p,int pos){
 int owner=player_owner(p);if(!mode_ok(owner))return;
 host_sync_clock(pos);
 int running=owner==0?run.running:drum_run.running;
 if(!running){host_sync.pending[owner]=0;if(owner==0)snapshot_tick();else drum_snapshot_tick();return;}
 /* Also cover automatically started held Drum lanes/New. */
 if(owner==0&&run.latch&&run.current<0)host_sync.pending[0]=1;
 if(owner==1&&drum_run.latch&&drum_run.ticks==0)host_sync.pending[1]=1;
 if(!host_sync_ready(owner))return;
 if(owner==0){snapshot_tick();advance(&run,&pattern);}else {drum_snapshot_tick();drum_tick();}
}
static const struct opus_os_vfn vtable[8]={{0,0,0},{0,0,noop},{0,0,down},{0,0,up},{0,0,halt},{0,0,tick},{0,0,noop},{0,0,halt}};
static struct {const struct opus_os_vfn *vt;unsigned pad[7];} players[2]={{vtable,{0}},{vtable,{0}}};
#ifndef FLUKE_HOST
_Static_assert(sizeof(struct opus_os_vfn)==8,"OS function entry");
_Static_assert(sizeof players[0]==32,"OS player object");
#endif
static int player_owner(void *p){if(p==&players[0])return 0;if(p==&players[1])return 1;return active_mode();}
static void *get_player(int m){return m>=0&&m<2?&players[m]:0;}
static uint32_t entropy(void){
 unsigned cycles;
#ifdef FLUKE_HOST
 static unsigned counter;cycles=++counter*7919u;
#else
 __asm__ volatile ("%0 = CYCLES;" : "=d"(cycles));
#endif
 struct opus_clock c;opus_clock(&c);
 return cycles^(unsigned)c.year*3661u^(unsigned)c.month*2678400u^
  (unsigned)c.day*86400u^(unsigned)c.hour*3600u^(unsigned)c.minute*60u^c.second;
}
static void fresh_pattern(void){
 *previous_pattern=pattern;
 uint32_t s=pattern.seed^entropy();previous_pattern->seed=random_word(&s);
 if(previous_pattern->seed==pattern.seed)previous_pattern->seed++;
 rotate_visible(previous_pattern,-previous_pattern->p[OFFSET]);
 generate(previous_pattern);
 Pattern *old=current_pattern;current_pattern=previous_pattern;previous_pattern=old;
}
#include "snapshot.h"
#include "drum_snapshot.h"
static void tape_autostart(void){
 struct opus_tempo t={.size=sizeof t};opus_tempo(&t);
 int previous=tape_playing;tape_playing=t.playing;
 if(previous!=tape_playing&&!tape_playing){
  for(int m=0;m<2;m++)if(host_sync.auto_hold[m]){
   host_sync.auto_hold[m]=host_sync.pending[m]=0;
   if(m==0){stop(&run);run.latch=0;}else {drum_stop();drum_run.latch=0;}
  }
 }
 if(previous==tape_playing||autostart==2||!tape_playing)return;
 if(autostart==1&&tape_playing!=1)return;
 int m=active_mode();if(!mode_ok(m)||!host_sync.selected[m])return;
 if(m==0){if(!run.latch)host_sync.auto_hold[0]=1;run.latch=1;if(!run.running)host_sync_start(0);}
 else {if(!drum_run.latch)host_sync.auto_hold[1]=1;drum_run.latch=1;if(!drum_run.running&&drums.count)host_sync_start(1);}
}
static void fluke_main(void){connect_visible_hits();tape_autostart();snapshot_main();drum_snapshot_main();}
static void chosen(int m,int on){
 if(mode_ok(m)){host_sync.selected[m]=!!on;host_sync.pending[m]=host_sync.auto_hold[m]=0;}
 if(m==1){
  int pad=companion_edit_pad;
  if(hits_linked&&pad<0&&drum_run.edit>=0&&drum_run.edit<drums.count)
   pad=drums.keys[drum_run.edit];
  if(!on)leave_drum_edit();
  companion_edit_pad=hits_linked?pad:-1;
  drum_fx_on=!!on;
  hits_cut_held=0;
  drum_stop();drum_run.latch=0;drum_recall_pending=0;
  if(on){
   drum_init(entropy());drum_notice=0;drum_notice_frames=0;
   if(hits_linked&&pad>=0)fluke_hits_select_pad(pad);
  }
  return;
 }
 if(m!=0)return;
 stop(&run);run.latch=0;recall_pending=0;
 if(on){
  run.latch=0;run.transpose=0;global_key=-1;key_gestures=0;
  for(int i=0;i<8;i++)turn_acc[i]=0;
  initialise(&pattern,entropy());snapshot_notice=0;snapshot_notice_frames=0;
 }
}
static void midi_start(int m){
 if(!mode_ok(m))return;
 host_sync.reset=1;
 if((m==0&&(run.running||run.latch))||(m==1&&(drum_run.running||drum_run.latch)))host_sync_start(m);
}
/* Observe transport without consuming its stock action, including off-page Play.
 * Remember Hold before the OS can call its stop callback during transport work. */
static int transport_before(int code,int pressed){
 if(pressed)connect_visible_hits();
 if(code==0x15&&!pressed&&drum_edit_exit_key_held){
  drum_edit_exit_key_held=0;
  return 1;
 }
 if(code==0x15&&pressed&&opus_instrument_now()==1&&(host_sync.selected[1]||companion_edit_pad>=0)){
  if(drum_edit_exit_key_held)return 1;
  if(fw_shift_held()){
   if(drum_run.edit<0&&companion_edit_pad<0)return 0;
   leave_drum_edit();
   drum_edit_exit_key_held=1;
   return 1;
  }
  struct opus_page page;
  int visible=opus_page_now(&page)&&page.kind==OPUS_PAGE_SEQ&&page.where==1&&page.module==&seq;
  drum_fx_on=visible?!drum_fx_on:1;
  if(!drum_fx_on)leave_drum_edit();
 }
 if(code==0x0c&&!pressed)hits_cut_held=0;
 if(pressed&&code==0x22){int m=active_mode();host_sync.play_mode=mode_ok(m)&&host_sync.selected[m]?m:-1;host_sync.play_hold=host_sync.play_mode>=0&&(m==0?run.latch:drum_run.latch)?m:-1;host_sync.play_auto=host_sync.play_hold>=0?host_sync.auto_hold[m]:0;}
 return 0;
}
/* The OS handles the arrows before notifying Fluke. Shifted arrows
 * belong to other controls and do not transpose the running sequence. */
static void transpose_running_octave(int direction)
{
 int mode=opus_instrument_now();
 if(!mode_ok(mode)||!host_sync.selected[mode])return;
 if(mode==1&&drum_run.edit>=0)leave_drum_edit();
 if(mode==0&&run.running){
  int old_register=pattern.p[REGISTER];
  int new_register=old_register+direction;
  int root=12*(new_register+1)+pattern.p[ROOT];
  if(new_register<lo[REGISTER]||new_register>hi[REGISTER]||root>127)return;
  pattern.p[REGISTER]=new_register;
  if(run.delay>0)run.pending.pitch+=12*direction;
 }else if(mode==1&&drum_run.running){
  int transpose=drum_run.transpose+12*direction;
  if(transpose>=-60&&transpose<=67)drum_run.transpose=transpose;
 }
}

static void transport_after(int code,int pressed){
 tape_autostart();
 if(pressed&&(code==0x24||code==0x25)&&!fw_shift_held())
  transpose_running_octave(code==0x24?-1:1);
 if(!pressed||code!=0x22)return;
 int m=host_sync.play_mode,held=host_sync.play_hold,automatic=host_sync.play_auto;host_sync.play_mode=host_sync.play_hold=-1;host_sync.play_auto=0;
 if(!mode_ok(m)||!host_sync.selected[m])return;
 host_sync.reset=1;
 if(held!=m)return;
 if(automatic&&!tape_playing)return;
 host_sync.auto_hold[m]=automatic;
 if(m==0)run.latch=1;else drum_run.latch=1;
 host_sync_start(m);
}
static void toggle_drum_hold(void)
{
    host_sync.auto_hold[1] = 0;
    drum_run.latch = !drum_run.latch;
    if (drum_run.latch)
        host_sync_start(1);
    else if (!drum_run.held) {
        drum_stop();
        host_sync.pending[1] = 0;
    }
}

static int fluke_hits_hold(int pressed)
{
    if (!host_sync.selected[1])
        return 0;
    if (pressed)
        toggle_drum_hold();
    return 1;
}

static int key(int m,int code,int pressed){
 if(!mode_ok(m))return 0;
 if(m==1){
  if(code==0x0c){drum_cut(&drum_run.cut_held,pressed);return 1;}
  if(code==0x0a||code==0x0b){if(pressed&&!fw_shift_held()){if(code==0x0a)drum_snapshot_save();else drum_snapshot_restore();}return 1;}
  if(code<0x3b||code>0x3e)return 0;
  if((fw_shift_held()&&drum_run.edit>=0)||!pressed)return 1;
  if(code==0x3b)drum_new(entropy());
  else if(code==0x3c)drum_pulses();
  else if(code==0x3e)toggle_drum_hold();
  return 1;
 }
 if(code==0x0c){if(pressed&&run.running)run.jump_pending=1;return 1;}
 if(code==0x0a||code==0x0b){
  if(pressed&&!fw_shift_held()){if(code==0x0a)snapshot_save();else snapshot_restore();}
  return 1;
 }
 if(code<0x3b||code>0x3e)return 0;
 /* Encoder pushes are Main-page actions. Shifted pushes have no action. */
 if(fw_shift_held())return 1;
 if(!pressed)return 1;
 if(code==0x3c){pattern.p[RHYTHM]^=1;pattern.p[PULSES]=bounded(pattern.p[PULSES],0,pattern.p[LENGTH]);}
 else if(code==0x3d){
  *previous_pattern=pattern;previous_pattern->p[SCALE]=(pattern.p[SCALE]+1)%6;
  generate(previous_pattern);
  Pattern *old=current_pattern;current_pattern=previous_pattern;previous_pattern=old;
 }
 else if(code==0x3b)fresh_pattern();
 else {host_sync.auto_hold[0]=0;run.latch=!run.latch;if(run.latch)host_sync_start(0);else if(!run.held){stop(&run);host_sync.pending[0]=0;}}
 return 1;
}
static const int map[2][4]={{LENGTH,DENSITY,OFFSET,RATE},{OCTAVE,TYPE,DIRECTION,SWING}};
static int turn(int m,int enc,int delta){
 if(!mode_ok(m)||enc<0||enc>7)return 0;
 if(m==1){drum_turn(enc,delta,fw_shift_held());return 1;}
 int sh=enc>=4||fw_shift_held(),lane=enc&3,pg=sh?1:0;
 int *a=&turn_acc[sh*4+lane];if((*a>0&&delta<0)||(*a<0&&delta>0))*a=0;
 /* OS native turns use Q10; four units per detent, as existing sequencer adapters. */
 *a+=delta;int n=*a/4096;*a-=n*4096;if(!n)return 1;
 int k=map[pg][lane];if(!pg&&lane==1&&pattern.p[RHYTHM])k=PULSES;
 if(k==RATE||k==TYPE||k==DIRECTION)fluke_expire();
 else n=encoder_speed_turn(&fluke_speed, encoder_plugin, fluke_expire, current_pattern, k, n);
 if(k==RATE)n=-n;
 int maximum=k==PULSES?pattern.p[LENGTH]:hi[k];
 int v=bounded(pattern.p[k]+n,lo[k],maximum);
 if(k==OFFSET){int target=(pattern.p[OFFSET]+n)%(pattern.p[LENGTH]+1);if(target<0)target+=pattern.p[LENGTH]+1;rotate_visible(&pattern,target-pattern.p[OFFSET]);pattern.p[OFFSET]=target;return 1;}
 if(k==LENGTH){set_length(&pattern,v);return 1;}
 pattern.p[k]=v;
 if(k==OCTAVE)shape_phrases(&pattern);
 return 1;
}
#include "layout.h"
static void show(void *n,int on){if(n)((unsigned char *)n)[0x50]=(unsigned char)!!on;}
static void text(int slot,const char *s){
 int n=slot<16?slot+draw_mono*17:slot;
 fw_set_text(texts[n],s);
 if(slot==0){fw_snprintf(layout_root,sizeof layout_root,"%s",s);return;}
 if(slot==1){fw_snprintf(layout_scale,sizeof layout_scale,"%s",s);return;}
 int m[9]={1024,0,0,0,1024,0,0,0,1024};
 if(slot>=2&&slot<=14){
  int x=slot<6?8+(slot-2)*78:slot<10?8+(slot-6)*78:slot<14?8+(slot-10)*78:242;
  m[0]=m[4]=FLUKE_LOWER_SCALE;m[6]=x*(1024-m[0]);m[7]=146*(1024-m[4]);
 }
 if(slot==15){m[4]=1044;m[7]=-40;}
 fw_text_draw(texts[n],fw_screen_raster(),m);
}
static int fluke_drum_root(void)
{
    return bounded(60 + drum_run.transpose, 0, 127);
}

/* Read-only drum selection for companion screens, called on the display thread. */
static unsigned fluke_drum_keys(int *edited)
{
    unsigned keys = 0;
    int count = bounded(drums.count, 0, DRUM_LANES);
    int edit = drum_run.edit;
    *edited = companion_edit_pad;
    for (int lane = 0; lane < count; ++lane) {
        unsigned key = (unsigned)drums.keys[lane];
        if (key < 24) {
            keys |= 1u << key;
            if (*edited < 0 && host_sync.selected[1] && lane == edit)
                *edited = (int)key;
        }
    }
    return keys | (host_sync.selected[1] ? 1u << 31 : 0) | (drum_run.latch ? 1u << 30 : 0) |
           (hits_overlay_on && drum_fx_on ? 1u << 29 : 0) |
           (drum_fx_on ? 1u << 28 : 0);
}

#include "drum_screen.h"
static void screen_init(void *scr){
 void *doc=*(void **)scr;char id[20];
 drum_screen_init(scr);
 for(int i=0;i<16;i++){
  for(int v=0;v<2;v++){fw_snprintf(id,sizeof id,"%sline%i",v?"m":"",i);melody_lines[v][i]=fw_svg_node_find(doc,id,0);}
  fw_snprintf(id,sizeof id,"note%i",i);note_nodes[i]=fw_svg_node_find(doc,id,0);
  fw_snprintf(id,sizeof id,"play%i",i);active_nodes[i]=fw_svg_node_find(doc,id,0);
  fw_snprintf(id,sizeof id,"rest%i",i);rest_nodes[i]=fw_svg_node_find(doc,id,0);
  fw_snprintf(id,sizeof id,"mnote%i",i);mono_notes[i]=fw_svg_node_find(doc,id,0);
  fw_snprintf(id,sizeof id,"mplay%i",i);mono_active[i]=fw_svg_node_find(doc,id,0);
  fw_snprintf(id,sizeof id,"mrest%i",i);mono_rests[i]=fw_svg_node_find(doc,id,0);
 }
 color_art=fw_svg_node_find(doc,"color",0);mono_art=fw_svg_node_find(doc,"mono",0);popup=fw_svg_node_find(doc,"popup",0);
 keyboard_beds[0]=fw_svg_node_find(doc,"keyboard",0);keyboard_beds[1]=fw_svg_node_find(doc,"mkeyboard",0);
 for(int i=0;i<12;i++){
  fw_snprintf(id,sizeof id,"keydot%i",i);keyboard_dots[0][i]=fw_svg_node_find(doc,id,0);
  fw_snprintf(id,sizeof id,"mkeydot%i",i);keyboard_dots[1][i]=fw_svg_node_find(doc,id,0);
 }
 header_dividers[0]=fw_svg_node_find(doc,"keydivider",0);
 header_dividers[1]=fw_svg_node_find(doc,"mkeydivider",0);
 if(text_ready)return;
 text_ready=1;
 fw_text_new(texts[0],230*1024,9,11,root_color,0);
 fw_text_new(texts[1],266*1024,9,11,root_color,0);
 for(int k=0;k<4;k++){
  fw_text_new(texts[2+k],(8+k*78)*1024,93,11,rgba[k],0);
  fw_text_new(texts[6+k],(8+k*78)*1024,107,18,rgba[k],0);
  fw_text_new(texts[10+k],(8+k*78)*1024,128,9,k==0?soft_blue:k==1?soft_green:k==3?hold_dim:caption_white,0);
 }
 fw_text_new(texts[14],242*1024,128,9,rgba[3],0);
 fw_text_new(texts[15],160*1024,9,11,page_color,1);
 fw_text_new(texts[16],160*1024,48,16,caption_white,1);
 /* Native text objects keep independent monochrome colors. */
 for(int i=0;i<16;i++){
  int x=i==0?230:i==1?266:i==15?160:8+((i<6?i-2:i<10?i-6:i<14?i-10:3)*78);
  int y=i<2||i==15?9:i<6?93:i<10?107:128;
  int size=i<6||i==15?11:i<10?18:9;
  const unsigned char *c=i==13?hold_dim:i==15?page_color:i<2?root_color:i<10?rgba[(i-2)%4]:i==10?soft_blue:i==11?soft_green:caption_white;
  unsigned char *grey=mono_colors[i];grey[0]=grey[1]=grey[2]=(c[0]*77+c[1]*150+c[2]*29)/256;grey[3]=255;
  fw_text_new(texts[17+i],x*1024,y,size,grey,i==15);
 }
}
static void screen_draw(void *scr,int dt){
 layout_header_mode=active_mode();layout_header_ready=1;
 if(active_mode()==1){drum_screen_draw(scr,dt);return;}
 show(drum_keyboard_node,0);show(drum_dot_node,0);show(drum_play_node,0);show(drum_lit_node,0);
 for(int i=0;i<2;i++)show(header_dividers[i],0);
 draw_global_key=global_key_tonic();
 (void)dt;if(active_mode()!=0){
  show(keyboard_beds[0],0);show(keyboard_beds[1],0);
  for(int i=0;i<16;i++){show(note_nodes[i],0);show(active_nodes[i],0);show(rest_nodes[i],0);}
  fw_screen_draw(scr);text(0,"synth only");return;
 }
 int pg=!!fw_shift_held();
 draw_mono=snapshot_notice_frames>0;layout_header_mono=draw_mono;
 for(int variant=0;variant<2;variant++){
  show(keyboard_beds[variant],draw_global_key>=0);
  for(int pc=0;pc<12;pc++)show(keyboard_dots[variant][pc],draw_global_key>=0&&key_scale_allows(draw_global_key,pattern.p[SCALE],pc));
 }
 show(color_art,!draw_mono);show(mono_art,draw_mono);show(popup,0);
 int header_matrix[9]={1024,0,0,0,1024,0,draw_global_key>=0?-27*1024:0,0,1024};
 for(int i=0;i<2;i++)if(header_dividers[i])fw_node_set_matrix(header_dividers[i],header_matrix);
 for(int i=0;i<16;i++){
  Step st=get_step(&pattern,i,0);int active=i<pattern.p[LENGTH],v=st.velocity;
  int base=12*(pattern.p[REGISTER]+1)+pattern.p[ROOT];
  int range=pattern.p[OCTAVE],span=12*((range<0?-range:range)+1)-1;
  int bottom=base+(range<0?12*range:0);
  int note_y=bounded(79-(st.pitch-bottom)*39/span,40,79);
  int m[9];layout_note_matrix(i,note_y,m);
  if(note_nodes[i])fw_node_set_matrix(note_nodes[i],m);
  if(mono_notes[i])fw_node_set_matrix(mono_notes[i],m);
  if(active_nodes[i])fw_node_set_matrix(active_nodes[i],m);
  if(mono_active[i])fw_node_set_matrix(mono_active[i],m);
  int playing=active&&v&&run.running&&run.current==i;
  for(int variant=0;variant<2;variant++)show(melody_lines[variant][i],active&&run.running&&run.current==i);
  show(mono_notes[i],active&&v&&!playing);show(mono_rests[i],active&&!v);show(mono_active[i],playing);
  show(note_nodes[i],active&&v&&!playing);
  show(rest_nodes[i],active&&!v);show(active_nodes[i],playing);
 }
 fw_screen_draw(scr);
 static const char *const roots[13]={"c","c#","d","d#","e","f","f#","g","g#","a","a#","b","c+12"};
 int root_note=12*(pattern.p[REGISTER]+1)+pattern.p[ROOT];
 char title[24];fw_snprintf(title,sizeof title,"%s%i",roots[root_note%12],root_note/12-1);text(0,title);
 if(draw_global_key>=0){
  char key_name[24];fw_snprintf(key_name,sizeof key_name,"%s-%s",roots[draw_global_key],scale_names[pattern.p[SCALE]]);text(1,key_name);
 }else text(1,scale_names[pattern.p[SCALE]]);
 text(15,draw_global_key>=0?"":pg?"2/2":"1/2");
 static const char *const labels[PARAMS]={"style","density","length","root","rate","swing","rotate","range","dir","scale","rhythm","pulses","register"};
 char value[24];
 for(int k=0;k<4;k++){
  int p=map[pg][k];if(!pg&&k==1&&pattern.p[RHYTHM])p=PULSES;text(2+k,labels[p]);
  if(p==DIRECTION){static const char *const dirs[5]={"fwd","rev","f+r","jump","rjump"};fw_snprintf(value,sizeof value,"%s",dirs[pattern.p[p]]);}
  else if(p==RATE)fw_snprintf(value,sizeof value,"%s",rate_names[pattern.p[p]]);
  else if(p==ROOT)fw_snprintf(value,sizeof value,"%s",roots[pattern.p[p]]);
  else if(p==TYPE)fw_snprintf(value,sizeof value,"%s",type_names[pattern.p[p]]);
  else if(p==OCTAVE&&pattern.p[p]>0)fw_snprintf(value,sizeof value,"+%i",pattern.p[p]);
  else fw_snprintf(value,sizeof value,"%i",pattern.p[p]);
  text(6+k,value);
 }
 text(10,pg?"":"new");text(11,pg?"":pattern.p[RHYTHM]?"density":"pulses");
 text(12,pg?"":"scale");
 if(!pg)text(run.latch?14:13,"hold");
 if(draw_mono){
  static const char *const messages[6]={"","saved","recalled","no save","save fail","saving"};
  show(popup,1);fw_group_draw(popup,*(void **)scr,identity,fw_screen_raster());show(popup,0);
  /* Draw the popup subtree last without clearing the background. */
  text(16,messages[snapshot_notice]);snapshot_notice_frames--;
 }
}
static struct opus_sequencer seq={.size=sizeof seq,.name="fluke",.player=get_player,.key=key,.turn=turn,
 .midi_start=midi_start,.chosen=chosen,.screen_init=screen_init,.screen_draw=screen_draw};
static void global_key_setting(const char *name,int value){
 static const char overlay[]="hitsoverlay";int h=0;while(overlay[h]&&name[h]==overlay[h])h++;
 if(!overlay[h]&&!name[h]){hits_overlay_on=!!value;return;}
 static const char automatic[]="autostart";int a=0;while(automatic[a]&&name[a]==automatic[a])a++;
 if(!automatic[a]&&!name[a]){autostart=bounded(value,0,2);return;}
 static const char keyboard[]="drumkeys";int j=0;while(keyboard[j]&&name[j]==keyboard[j])j++;
 if(!keyboard[j]&&!name[j]){drum_keyboard_transpose=!!value;drum_stop();drum_run.latch=0;drum_run.transpose=0;host_sync.pending[1]=host_sync.auto_hold[1]=0;return;}
 static const char expected[]="globalkey";int i=0;
 while(expected[i]&&name[i]==expected[i])i++;
 if(expected[i]||name[i])return;
 global_key_enabled=!!value;
 if(!global_key_enabled)global_key=-1;
}
static int named_module(const char *name, const char *wanted)
{
    if (!name)
        return 0;
    while (*wanted && *name == *wanted) {
        ++name;
        ++wanted;
    }
    return !*name && !*wanted;
}

static void connect_visible_hits(void)
{
    if (hits_linked)
        return;
    struct opus_page page;
    if (!opus_page_now(&page) || !page.module)
        return;

    int (*connect)(struct opus_page *, int, int) = 0;
    enum companion_page companion = drum_companion(&page);
    if (companion == COMPANION_HITS) {
        const struct opus_engine *engine = page.module;
        if (engine->size >= sizeof(*engine))
            connect = engine->page_key;
    } else if (companion == COMPANION_FLUX) {
        const struct opus_lfo_type *lfo = page.module;
        if (lfo->size >= sizeof(*lfo))
            connect = lfo->page_key;
    } else if (page.kind == OPUS_PAGE_EFFECT) {
        const struct opus_effect *effect = page.module;
        if (effect->size >= sizeof(*effect) && named_module(effect->name, "grip"))
            connect = effect->page_key;
    }
    if (!connect)
        return;

    static struct hits_fluke_link link = {
        .keys = fluke_drum_keys,
        .edit = fluke_hits_select_pad,
        .root = fluke_drum_root,
        .hold = fluke_hits_hold,
        .cut = fluke_hits_cut,
    };
    page.inst = &link;
    if (connect(&page, HITS_FLUKE_CONNECT_KEY, 0))
        hits_linked = 1;
}

int opl_init(struct opl_self *self){
 encoder_plugin=self;
 host_sync_init();
 layout_owner=self;layout_header_ready=0;opus_on_frame(self,layout_header_frame);
 opus_on_key(self,transport_before);opus_on_key_after(self,transport_after);
 emit_on=note_on;emit_off=note_off;
 hits_overlay_on=!!opus_setting(self,"hitsoverlay");
 drum_keyboard_transpose=!!opus_setting(self,"drumkeys");
 autostart=bounded(opus_setting(self,"autostart"),0,2);
 {struct opus_tempo t={.size=sizeof t};opus_tempo(&t);tape_playing=t.playing;}
 global_key_enabled=!!opus_setting(self,"globalkey");global_key=-1;key_gestures=0;
 opus_on_setting(self,global_key_setting);
 initialise(&pattern,entropy());stop(&run);drum_init(entropy());
 fw_snprintf(art_path,sizeof art_path,"%s/screen.svg",self->folder);
 /* opus_art_text writes only missing/changed art with headroom checks. */
 if(opus_art_text(self,"screen.svg",fluke_svg,sizeof fluke_svg-1)<0)return -1;
 if(!snapshot_init(self)||!drum_snapshot_init(self))return -3;
 opus_on_main_pass(self,fluke_main);
 seq.svg=art_path;
 return opus_seq_register(self,&seq)<0?-2:0;
}
void *memcpy(void *d,const void *s,unsigned n){unsigned char *a=d;const unsigned char *b=s;for(unsigned i=0;i<n;i++)a[i]=b[i];return d;}
void *memset(void *d,int c,unsigned n){unsigned char *a=d;for(unsigned i=0;i<n;i++)a[i]=(unsigned char)c;return d;}
