/* Six fixed drum voices, independent rhythm/rate state. No pitch quantizer. */
#ifndef FLUKE_DRUM_H
#define FLUKE_DRUM_H
#define DRUM_LANES 6
#define DRUM_STYLES 14
#include "drum_groove.h"
enum {DS_STYLE,DS_DENSITY,DS_LENGTH,DS_RATE,DS_SWING,DS_ROTATE,DS_RANDOM,DS_DIRECTION,DS_RHYTHM,DS_PULSES,DS_PARAMS};
static const int drum_lo[DS_PARAMS]={0,1,1,0,0,0,0,0,0,0};
static const int drum_hi[DS_PARAMS]={13,16,16,13,100,16,100,4,1,16};
static const int drum_defaults[DS_PARAMS]={0,8,16,2,0,0,0,0,0,8};
static const char *const drum_styles[DRUM_STYLES]={"strght","backbt","offbt","tres","clave","break","fill","random","rock","funk","disco","reggae","latin","shufl"};
typedef struct {
 uint32_t seed;
 int p[DS_PARAMS];
 unsigned visible_mask;
 int first_note;
 unsigned char rank[DRUM_STYLES][16];
} DrumLane;
typedef struct {
 DrumLane lanes[DRUM_LANES],master;
 int keys[DRUM_LANES],count;
} DrumPattern;
typedef struct {
 int current,next,elapsed,delay,left,last_length,last_direction,pitch;
 uint32_t random;
 unsigned cycle;
 int pending_hit,last_hit,perturbation,early,early_phase;
} DrumVoice;
typedef struct {
 int running,latch,held,edit,cut_held,transpose;
 unsigned gestures,ticks,priority;
 int jump_pending;
 int weight_target[DRUM_LANES+1][DS_PARAMS],weight_last[DRUM_LANES+1][DS_PARAMS];
 int rotate_fraction[DRUM_LANES+1];
 DrumVoice voices[DRUM_LANES];
} DrumPlayer;
static DrumPattern drums;
static DrumPlayer drum_run;
static int drum_turn_acc[8];
static int drum_count(const DrumLane *b){int n=0;for(int i=0;i<16;i++)n+=!!(b->visible_mask&(1u<<i));return n;}
static int drum_visible(const DrumLane *b,int position){
 int count=drum_count(b);if(!count)return 0;
 position%=count;if(position<0)position+=count;
 for(int i=0;i<16;i++){int note=(b->first_note+i)&15;if(b->visible_mask&(1u<<note)){if(!position)return note;position--;}}
 return b->first_note;
}
static void drum_rotation(DrumLane *b){int rank=0,count=drum_count(b);for(int i=0;i<b->first_note;i++)rank+=!!(b->visible_mask&(1u<<i));b->p[DS_ROTATE]=count?(count-rank)%count:0;}
static void drum_length(DrumLane *b,int length){
 length=bounded(length,1,16);int count=drum_count(b);
 while(count>length){b->visible_mask&=~(1u<<drum_visible(b,count-1));count--;}
 while(count<length){for(int i=0;i<16;i++){int note=(b->first_note+i)&15;if(!(b->visible_mask&(1u<<note))){b->visible_mask|=1u<<note;count++;break;}}}
 b->p[DS_LENGTH]=length;b->p[DS_PULSES]=bounded(b->p[DS_PULSES],0,length);drum_rotation(b);
}
static void drum_rotate(DrumLane *b,int amount){drum_length(b,b->p[DS_LENGTH]);int length=b->p[DS_LENGTH],delta=amount%length;b->first_note=drum_visible(b,(length-delta)%length);drum_rotation(b);}
static int drum_weight(int style,int i){
 if(style==0)return i==0?900:i%4==0?700:i%2==0?400:100;
 if(style==1)return i==4||i==12?900:i==7||i==15?600:i%2==0?300:100;
 if(style==2)return i%4==2?900:i%2?300:100;
 if(style==3)return i==0||i==6||i==12?900:i==3||i==9||i==15?500:100;
 if(style==4)return i==0||i==3||i==6||i==10||i==12?900:i%2?300:100;
 if(style==5)return i==0||i==4||i==10||i==12?900:i==6||i==7||i==14?600:100;
 if(style==6)return (i/4)*200+(i==15?80:0);
 return 0;
}
#include "drum_families.h"
static void drum_generate(DrumLane *b){
 for(int style=0;style<DRUM_STYLES;style++){
  uint32_t state=b->seed^((unsigned)(style+1)*0x9e3779b9u);int score[16],order[16];
  static const int role_phase[6]={0,0,2,2,3,1};int role=drum_role(b->seed);
  for(int i=0;i<16;i++){order[i]=i;score[i]=style>=8?drum_map_level(b->seed,i)*2+drum_family_weight(style,role,i,b->seed)+rnd(&state,96):style==7?drum_map_level(b->seed,i)*2+rnd(&state,1024):drum_map_level(b->seed,i)*4+drum_weight(style,(i+role_phase[role])&15)+rnd(&state,128);}
  for(int i=0;i<15;i++)for(int j=i+1;j<16;j++)if(score[order[j]]>score[order[i]]){int t=order[i];order[i]=order[j];order[j]=t;}
  for(int i=0;i<16;i++)b->rank[style][order[i]]=(unsigned char)i;
 }
}
static void drum_lane_init(DrumLane *b,uint32_t seed){b->seed=seed;b->visible_mask=0xffffu;b->first_note=0;for(int p=0;p<DS_PARAMS;p++)b->p[p]=drum_defaults[p];drum_generate(b);}
static int drum_base_hit(const DrumLane *b,int step){
 if(step<0||step>=b->p[DS_LENGTH])return 0;
 if(b->p[DS_RHYTHM]){static const int phase[14]={0,1,2,3,5,7,11,13,0,3,2,4,5,6};int length=b->p[DS_LENGTH],i=(step-b->p[DS_ROTATE]%length-(int)(b->seed%(unsigned)length)-phase[b->p[DS_STYLE]]%length+3*length)%length,pulses=b->p[DS_PULSES];return (i*pulses)%length<pulses;}
 return b->rank[b->p[DS_STYLE]][drum_visible(b,step)]<b->p[DS_DENSITY];
}
#include "drum_timing.h"
static void drum_release(int lane){DrumVoice *v=&drum_run.voices[lane];if(v->left>0)emit_off(v->pitch);v->left=0;v->delay=-1;}
static void drum_stop(void){
 for(int i=0;i<drums.count;i++)drum_release(i);
 drum_run.running=0;drum_run.held=0;drum_run.jump_pending=0;
}
static void drum_weight_reset(void){for(int i=0;i<=DRUM_LANES;i++){drum_run.rotate_fraction[i]=0;for(int p=0;p<DS_PARAMS;p++)drum_run.weight_last[i][p]=-999;}}
static void drum_reset_voice(int lane){
 DrumVoice *v=&drum_run.voices[lane];drum_release(lane);
 v->current=-1;v->next=0;v->elapsed=0;v->last_length=0;v->last_direction=-1;v->cycle=0;v->random=drums.lanes[lane].seed^0x8b5ad4ceu;v->last_hit=0;v->perturbation=0;v->early=0;v->early_phase=-1;
}
static void drum_start(void){for(int i=0;i<drums.count;i++)drum_reset_voice(i);drum_run.running=1;drum_run.ticks=0;drum_run.priority=0;drum_run.jump_pending=0;}
static void drum_init(uint32_t seed){
 drum_stop();drums.count=0;drum_lane_init(&drums.master,seed);
 for(int i=0;i<DRUM_LANES;i++){drums.keys[i]=-1;drum_lane_init(&drums.lanes[i],drum_groove_seed(drums.master.seed,i,random_word(&seed)));drum_run.voices[i].left=0;drum_run.voices[i].delay=-1;drum_run.voices[i].current=-1;}
 drum_run.transpose=0;drum_run.latch=0;drum_run.held=0;drum_run.edit=-1;drum_run.cut_held=0;drum_run.gestures=0;drum_run.ticks=0;drum_weight_reset();
 for(int i=0;i<8;i++)drum_turn_acc[i]=0;
 /* Fresh kit: F3, G3, C#4; initialized but waits for a playback gesture. */
 drums.count=3;drums.keys[0]=0;drums.keys[1]=2;drums.keys[2]=8;
 for(int i=0;i<3;i++)drum_reset_voice(i);
}
static int drum_find(int key){for(int i=0;i<drums.count;i++)if(drums.keys[i]==key)return i;return -1;}
static void drum_pick(int key,uint32_t seed){
 int i=drum_find(key);
 if(i<0){
  if(drums.count==DRUM_LANES)return;
  i=drums.count;drums.lanes[i]=drums.master;drums.lanes[i].p[DS_SWING]=0;int role=0;unsigned roles=0;for(int j=0;j<drums.count;j++)roles|=1u<<drum_role(drums.lanes[j].seed);while(roles&(1u<<role))role++;
  drums.lanes[i].seed=drum_groove_seed(drums.master.seed,role,seed);drum_generate(&drums.lanes[i]);drums.keys[i]=key;drums.count++;drum_reset_voice(i);
  /* A newly added lane joins the running clock, rather than firing off-grid. */
  if(drum_run.running)drum_run.voices[i].elapsed=(int)(drum_run.ticks%(unsigned)rate_ticks[drums.lanes[i].p[DS_RATE]]);
 }
 drum_run.edit=drum_run.edit==i?-1:i;drum_weight_reset();
 for(int n=0;n<8;n++)drum_turn_acc[n]=0;
}
static void drum_remove(int key){
 int i=drum_find(key);if(i<0)return;
 drum_release(i);
 for(int j=i;j<drums.count-1;j++){drums.lanes[j]=drums.lanes[j+1];drums.keys[j]=drums.keys[j+1];drum_run.voices[j]=drum_run.voices[j+1];}
 drums.count--;drum_weight_reset();drums.keys[drums.count]=-1;drum_run.voices[drums.count].left=0;drum_run.voices[drums.count].delay=-1;
 if(drum_run.edit==i)drum_run.edit=-1;else if(drum_run.edit>i)drum_run.edit--;
 if(!drums.count)drum_stop();
 for(int n=0;n<8;n++)drum_turn_acc[n]=0;
}
/* New changes the complete lane, without changing its assigned drum note. */
static void drum_randomize(DrumLane *b,uint32_t seed,uint32_t group,int local){
 uint32_t old=b->seed;int role=drum_role(old);
 uint32_t shape=group^0x671ad487u;int common_length[3]={8,12,16},length=common_length[rnd(&shape,3)];
 int rate_group=1+rnd(&shape,2),style=rnd(&shape,DRUM_STYLES);
 int direction=rnd(&shape,5)?0:1+rnd(&shape,4);
 if(local){style=(style+DRUM_STYLES-1+rnd(&seed,3))%DRUM_STYLES;if(!rnd(&seed,4))direction=rnd(&seed,5);}
 b->seed=drum_groove_seed(group,role,random_word(&seed));if(b->seed==old)b->seed^=1;
 b->visible_mask=0xffffu;b->first_note=0;
 if(style>=8){length=16;direction=0;}
 b->p[DS_STYLE]=style;
 static const int density_min[6]={3,2,7,4,3,2},density_span[6]={6,6,8,7,7,7};
 b->p[DS_DENSITY]=density_min[role]+rnd(&seed,density_span[role]);
 if(style==10){if(role==0)b->p[DS_DENSITY]=4+rnd(&seed,3);if(role==2)b->p[DS_DENSITY]=4;if(role==3)b->p[DS_DENSITY]=4+rnd(&seed,3);}
 if(style==11&&role<2)b->p[DS_DENSITY]=1+rnd(&seed,3);
 static const int rates[4]={0,2,4,7};int r=rate_group;
 if(role==2||role==3){if(!rnd(&seed,3))r--;}else if(!rnd(&seed,4))r++;
 b->p[DS_RATE]=style>=8?2:rates[bounded(r,0,3)];
 b->p[DS_SWING]=style==13?50+rnd(&seed,21):rnd(&seed,26);
 b->p[DS_RANDOM]=rnd(&seed,36);
 b->p[DS_DIRECTION]=direction;
 b->p[DS_RHYTHM]=style>=8?0:rnd(&seed,5)==0;
 if(style<8&&!rnd(&seed,5))length/=2;
 drum_length(b,length);
 b->p[DS_PULSES]=1+rnd(&seed,b->p[DS_LENGTH]);
 drum_rotate(b,-b->p[DS_ROTATE]);drum_generate(b);
}
#include "drum_candidates.h"
static void drum_new(uint32_t seed){
 int edit=drum_run.edit;
 const DrumPattern *selected=drum_choose_candidate(seed,edit);
 if(edit<0)drums=*selected;else drums.lanes[edit]=selected->lanes[edit];
 int first=edit<0?0:edit,last=edit<0?drums.count:edit+1;
 for(int i=first;i<last;i++){drum_run.voices[i].random=drums.lanes[i].seed^0x8b5ad4ceu;drum_run.voices[i].early=0;drum_run.voices[i].delay=-1;}
 drum_weight_reset();for(int i=0;i<8;i++)drum_turn_acc[i]=0;
 /* New changes the material, never the player's gate or Hold state. */
}
static void drum_fire(int lane){
 DrumVoice *v=&drum_run.voices[lane];if(!v->pending_hit)return;
 if(v->left>0){emit_off(v->pitch);v->left=0;}
 int active=0;for(int i=0;i<drums.count;i++)active+=drum_run.voices[i].left>0;
 if(active>=4){v->last_hit=0;return;}
 int pitch=53+drums.keys[lane];if(pitch<0||pitch>127){v->last_hit=0;return;}v->pitch=pitch;
 emit_on(pitch,100);v->left=rate_ticks[drums.lanes[lane].p[DS_RATE]]/2;if(v->left<1)v->left=1;
}
static void drum_jump_now(void){
 for(int i=0;i<drums.count;i++){
  drum_reset_voice(i);DrumLane *b=&drums.lanes[i];DrumVoice *v=&drum_run.voices[i];int phase=0;
  while(traversal_step(phase,b->p[DS_LENGTH],b->p[DS_DIRECTION])!=0)phase++;
  v->next=phase;v->last_length=b->p[DS_LENGTH];v->last_direction=b->p[DS_DIRECTION];
 }
}
static int drum_prepare_hit(int lane,int phase,int step){
 DrumLane *b=&drums.lanes[lane];DrumVoice *v=&drum_run.voices[lane];
 if(phase==0)v->perturbation=b->p[DS_RANDOM]>=100?16:rnd(&v->random,b->p[DS_RANDOM]*16/100+1);
 int hit=drum_base_hit(b,step);
 if(!hit&&b->p[DS_RANDOM]){int base=b->p[DS_RHYTHM]?b->p[DS_PULSES]:b->p[DS_DENSITY];hit=b->rank[b->p[DS_STYLE]][drum_visible(b,step)]<base+v->perturbation;}
 return hit;
}
static void drum_tick(void){
 if(!drum_run.running)return;
 if(drum_run.jump_pending){
  /* Group jumps use the next old ALL reference-lane boundary. */
  if(!drums.count||drum_run.voices[0].elapsed==0){
   drum_jump_now();drum_run.jump_pending=0;
  }
 }
 /* Release all elapsed notes before allocating the four available voices. */
 for(int lane=0;lane<drums.count;lane++){
  DrumVoice *v=&drum_run.voices[lane];
  if(v->left>0&&!--v->left)emit_off(v->pitch);
  if(v->delay>0)--v->delay;
 }
 for(int lane=0;lane<drums.count;lane++){
  DrumLane *b=&drums.lanes[lane];DrumVoice *v=&drum_run.voices[lane];
  int period=rate_ticks[b->p[DS_RATE]];if(v->elapsed>=period)v->elapsed%=period;
  int length=b->p[DS_LENGTH],dir=b->p[DS_DIRECTION];
  if(v->last_length!=length||v->last_direction!=dir){v->next=0;v->early=0;v->delay=-1;v->last_length=length;v->last_direction=dir;}
  int phase=v->next%traversal_length(length,dir),step=traversal_step(phase,length,dir);
  if(!v->elapsed){
   v->current=step;v->next=(phase+1)%traversal_length(length,dir);if(!v->next)v->cycle++;
   if(!(v->early&&v->early_phase==phase)){
    v->pending_hit=v->last_hit=drum_prepare_hit(lane,phase,step);
    int offset=drum_timing_offset(b,phase,step,drums.master.p[DS_SWING]);
    /* Startup or a just-edited pattern has no past in which to play an early hit. */
    v->delay=offset>0?offset:0;
   }
   v->early=0;
  }else if(!v->early){
   int offset=drum_timing_offset(b,phase,step,drums.master.p[DS_SWING]);
   if(offset<0&&v->elapsed==period+offset){
    v->pending_hit=v->last_hit=drum_prepare_hit(lane,phase,step);v->current=step;
    v->early=1;v->early_phase=phase;v->delay=0;
   }
  }
  v->elapsed=(v->elapsed+1)%period;
 }
 /* Rotate contention priority so late-selected lanes are not starved. */
 int due=0;for(int i=0;i<drums.count;i++)due|=drum_run.voices[i].delay==0&&drum_run.voices[i].pending_hit;
 for(int n=0;n<drums.count;n++){
  int lane=(n+(int)(drum_run.priority%(unsigned)drums.count))%drums.count;
  DrumVoice *v=&drum_run.voices[lane];if(v->delay==0){drum_fire(lane);v->delay=-1;}
 }
 if(due)drum_run.priority++;
 drum_run.ticks++;
}
static const int drum_map[2][4]={{DS_LENGTH,DS_DENSITY,DS_ROTATE,DS_RATE},{DS_RANDOM,DS_STYLE,DS_DIRECTION,DS_SWING}};
static int drum_parameter(const DrumLane *b,int pg,int lane){int p=drum_map[pg][lane];return !pg&&lane==1&&b->p[DS_RHYTHM]?DS_PULSES:p;}
static void drum_rotate_selector(DrumLane *b,int delta){int target=(b->p[DS_ROTATE]+delta)%(b->p[DS_LENGTH]+1);if(target<0)target+=b->p[DS_LENGTH]+1;drum_rotate(b,target-b->p[DS_ROTATE]);b->p[DS_ROTATE]=target;}
static void drum_adjust(DrumLane *b,int pg,int lane,int delta){
 int p=drum_parameter(b,pg,lane);if(p==DS_RATE)delta=-delta;
 if(p==DS_ROTATE){drum_rotate_selector(b,delta);return;}
 int max=p==DS_PULSES?b->p[DS_LENGTH]:drum_hi[p],v=bounded(b->p[p]+delta,drum_lo[p],max);
 if(p==DS_LENGTH)drum_length(b,v);else b->p[p]=v;
}
/* ALL uses proportional fixed-point targets, retaining sub-step motion.
 * Categorical selectors step locally; rotations scale to lane Length. */
static void drum_weighted(DrumLane *b,int slot,int pg,int lane,int detents){
 int p=drum_parameter(b,pg,lane);
 if(p==DS_STYLE||p==DS_DIRECTION){drum_adjust(b,pg,lane,detents);return;}
 if(p==DS_ROTATE){
  drum_run.rotate_fraction[slot]+=detents*b->p[DS_LENGTH];
  int steps=drum_run.rotate_fraction[slot]/16;drum_run.rotate_fraction[slot]-=steps*16;
  if(steps){drum_rotate_selector(b,steps);}
  return;
 }
 int value=p==DS_RATE?rate_ticks[b->p[p]]:b->p[p];
 int *target=&drum_run.weight_target[slot][p],*last=&drum_run.weight_last[slot][p];
 if(*last!=b->p[p])*target=value*65536;
 int sign=detents<0?-1:1,n=detents<0?-detents:detents;if(p==DS_RATE)sign=-sign;
 int minimum=p==DS_RATE?rate_ticks[0]:drum_lo[p];
 int maximum=p==DS_RATE?rate_ticks[13]:p==DS_PULSES?b->p[DS_LENGTH]:drum_hi[p];
 for(int i=0;i<n;i++){
  int amount=*target/16;if(amount<16384)amount=16384;
  *target=bounded(*target+sign*amount,minimum*65536,maximum*65536);
 }
 if(p==DS_RATE){
  int best=0,distance=0x7fffffff;
  for(int i=0;i<14;i++){int d=rate_ticks[i]*65536-*target;if(d<0)d=-d;if(d<distance){best=i;distance=d;}}
  b->p[p]=best;
 }else{
  int result=(*target+32768)/65536;
  if(p==DS_LENGTH)drum_length(b,result);else b->p[p]=result;
 }
 *last=b->p[p];
}
static void drum_turn(int enc,int delta,int shifted){
 int lane=enc&3,pg=drum_run.edit>=0&&(enc>=4||shifted);int *a=&drum_turn_acc[pg*4+lane];
 if((*a>0&&delta<0)||(*a<0&&delta>0))*a=0;
 *a+=delta;int n=*a/4096;*a-=n*4096;if(!n)return;
 if(drum_run.edit>=0){drum_adjust(&drums.lanes[drum_run.edit],pg,lane,n);drum_weight_reset();}
 else {
  if(lane==3){drums.master.p[DS_SWING]=bounded(drums.master.p[DS_SWING]+n,0,100);return;}
  if(pg)return;
  if(lane==1){drums.master.p[DS_RHYTHM]=0;for(int i=0;i<drums.count;i++)drums.lanes[i].p[DS_RHYTHM]=0;}
  int previous_rotation=drums.master.p[DS_ROTATE];
  drum_weighted(&drums.master,DRUM_LANES,pg,lane,n);for(int i=0;i<drums.count;i++)drum_weighted(&drums.lanes[i],i,pg,lane,n);
  if(lane==2&&previous_rotation!=0&&drums.master.p[DS_ROTATE]==0){for(int i=0;i<drums.count;i++)drum_rotate_selector(&drums.lanes[i],-drums.lanes[i].p[DS_ROTATE]);drum_weight_reset();}
 }
}
static void drum_toggle_rhythm(DrumLane *b){b->p[DS_RHYTHM]^=1;b->p[DS_PULSES]=bounded(b->p[DS_PULSES],0,b->p[DS_LENGTH]);}
static void drum_pulses(void){drum_weight_reset();if(drum_run.edit>=0)drum_toggle_rhythm(&drums.lanes[drum_run.edit]);}
#endif
