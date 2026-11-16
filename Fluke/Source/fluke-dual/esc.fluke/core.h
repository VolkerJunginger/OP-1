/* Fluke: native melody sequencer. Marbles quantization in marbles_melody.h. */
#ifndef FLUKE_CORE_H
#define FLUKE_CORE_H
#include <stdint.h>
enum { TYPE, DENSITY, LENGTH, ROOT, RATE, SWING, OFFSET, OCTAVE, DIRECTION, SCALE, RHYTHM, PULSES, REGISTER, PARAMS };
static const int lo[PARAMS]={0,1,1,0,0,0,0,-3,0,0,0,0,-1};
static const int hi[PARAMS]={9,16,16,11,13,100,16,3,4,5,1,16,9};
static const int defaults[PARAMS]={1,14,16,0,2,0,0,1,0,1,0,14,2};
/* Musical divisions at 96 ticks per quarter. */
static const int rate_ticks[14]={12,16,24,32,48,64,72,96,120,128,144,192,288,384};
static const char *const rate_names[14]={"1/32","1/24","1/16","1/12","1/8","1/6","3/16","1/4","5/16","1/3","3/8","1/2","3/4","1"};
typedef struct {
 uint32_t seed;
 int p[PARAMS];
 unsigned char pitch[10][16],vel[16][16];
 signed char phrase[10][16];
 unsigned visible_mask;
 int first_note;
} Pattern;
typedef struct {int pitch,velocity;} Step;
static int bounded(int x,int a,int b){return x<a?a:x>b?b:x;}
static uint32_t random_word(uint32_t *s){uint32_t x=*s?*s:1;x^=x<<13;x^=x>>17;x^=x<<5;*s=x;return x;}
static int rnd(uint32_t *s,int n){return (int)(random_word(s)%(unsigned)n);}
static void shuffle(uint32_t *s,int *v,int n){for(int i=n-1;i>0;i--){int j=rnd(s,i+1),t=v[i];v[i]=v[j];v[j]=t;}}
#include "marbles_melody.h"
static void shape_phrases(Pattern *b);
static int traversal_step(int phase,int length,int direction);
static void generate(Pattern *b){
 marbles_fill(b->pitch,b->seed,b->p[SCALE]);
 /* A shuffled ordering gives nested density masks without altering pitches. */
 uint32_t state=b->seed^0xa19f5b23u;int order[16];
 for(int i=0;i<16;i++)order[i]=i;
 shuffle(&state,order,16);
 for(int d=0;d<16;d++){
  for(int i=0;i<16;i++)b->vel[d][i]=0;
  for(int i=0;i<=d;i++)b->vel[d][order[i]]=1;
 }
 shape_phrases(b);
}
/* Visible membership is separate from the circular order of all 16 notes.
 * Shortening hides the tail; rotations skip hidden notes. Extending inserts
 * hidden notes back at their original positions in the rotated circle. */
static int visible_count(const Pattern *b){int n=0;for(int i=0;i<16;i++)n+=!!(b->visible_mask&(1u<<i));return n;}
static int visible_note(const Pattern *b,int position){
 int count=visible_count(b);if(!count)return 0;
 position%=count;if(position<0)position+=count;
 for(int i=0;i<16;i++){int note=(b->first_note+i)&15;if(b->visible_mask&(1u<<note)){if(!position)return note;position--;}}
 return b->first_note;
}
static void update_rotation_value(Pattern *b){
 int count=visible_count(b),rank=0;
 for(int i=0;i<b->first_note;i++)rank+=!!(b->visible_mask&(1u<<i));
 b->p[OFFSET]=count?(count-rank)%count:0;
}
static void set_length(Pattern *b,int length){
 length=bounded(length,1,16);int count=visible_count(b);
 while(count>length){int tail=visible_note(b,count-1);b->visible_mask&=~(1u<<tail);count--;}
 while(count<length){
  for(int i=0;i<16;i++){int note=(b->first_note+i)&15;if(!(b->visible_mask&(1u<<note))){b->visible_mask|=1u<<note;count++;break;}}
 }
 b->p[LENGTH]=length;update_rotation_value(b);
 b->p[PULSES]=bounded(b->p[PULSES],0,length);
}
static void rotate_visible(Pattern *b,int amount){
 set_length(b,b->p[LENGTH]);int length=b->p[LENGTH],delta=amount%length;
 b->first_note=visible_note(b,(length-delta)%length);
 update_rotation_value(b);
}
static int note_on_grid(const Pattern *b,int index){
 int length=b->p[LENGTH],i=visible_note(b,index);
 int rhythm_index=(index-b->p[OFFSET]%length+length)%length;
 int pulses=bounded(b->p[PULSES],0,length);
 return b->p[RHYTHM]?((rhythm_index*pulses)%length<pulses):b->vel[b->p[DENSITY]-1][i];
}
static int raw_interval(int raw,int range){
 int octaves=(range<0?-range:range)+1;
 return raw%12+12*(raw*octaves/25)+(range<0?12*range:0);
}
#include "global_key.h"
static Step get_step(const Pattern *b,int index,int transpose){
 int i=visible_note(b,index),base=12*(b->p[REGISTER]+1)+b->p[ROOT];
 Step result={global_quantize(b,base+b->phrase[b->p[TYPE]][i]+transpose),note_on_grid(b,index)?100:0};
 return result;
}
#include "phrase_rules.h"
static void initialise(Pattern *b,uint32_t seed){b->seed=seed;b->visible_mask=0xffffu;b->first_note=0;for(int i=0;i<PARAMS;i++)b->p[i]=defaults[i];generate(b);}
/* One tick engine: pending swing and note-offs advance in musical time.
 * Repeated pitches are explicitly released before retrigger. Fixed half-step note duration; no gate or accent parameter. */
typedef struct {int note,left;} HeldNote;
typedef struct {
 int running,latch,held,transpose,next_step,current,elapsed,delay,last_direction,last_length;
 int jump_pending;
 Step pending;
 HeldNote notes[4];
} Player;
static void (*emit_on)(int,int);
static void (*emit_off)(int);
static void all_off(Player *r){for(int i=0;i<4;i++){if(r->notes[i].left>0)emit_off(r->notes[i].note);r->notes[i].left=0;}r->delay=-1;}
static void stop(Player *r){all_off(r);r->running=0;r->held=0;r->next_step=0;r->current=-1;r->elapsed=0;r->last_direction=-1;r->last_length=0;r->jump_pending=0;}
static void start(Player *r){all_off(r);r->running=1;r->next_step=0;r->current=-1;r->elapsed=0;r->last_direction=-1;r->last_length=0;r->jump_pending=0;}
static void sound_note(Player *r,Step v,int duration){
 if(!v.velocity)return;
 int free_slot=-1;
 for(int i=0;i<4;i++){
  if(r->notes[i].left>0&&r->notes[i].note==v.pitch){emit_off(v.pitch);r->notes[i].left=0;}
  if(r->notes[i].left==0)free_slot=i;
 }
 if(free_slot<0){free_slot=0;emit_off(r->notes[0].note);}
 if(duration<1)duration=1;
 emit_on(v.pitch,v.velocity);r->notes[free_slot].note=v.pitch;r->notes[free_slot].left=duration;
}
static void fire(Player *r,const Pattern *b){
 Step note=r->pending;
 /* A live Shift+note or scale change can arrive during a swing delay. */
 note.pitch=global_quantize(b,note.pitch);
 sound_note(r,note,rate_ticks[b->p[RATE]]/2);
}
/* phase maps to a physical step; ping-pong does not repeat its endpoints. */
static int traversal_length(int length,int direction){return direction==2&&length>1?2*length-2:length;}
static int traversal_step(int phase,int length,int direction){
 if(direction==1)return length-1-phase;
 if(direction==2)return phase<length?phase:2*length-2-phase;
 if(direction==4)phase=length-1-phase;
 if(direction>=3){int odds=(length+1)/2;return phase<odds?2*phase:2*(phase-odds)+1;}
 return phase;
}
static void advance(Player *r,const Pattern *b){
 if(!r->running)return;
 for(int i=0;i<4;i++)if(r->notes[i].left>0&&!--r->notes[i].left)emit_off(r->notes[i].note);
 if(r->delay>0&&!--r->delay){fire(r,b);r->delay=-1;}
 int period=rate_ticks[b->p[RATE]];
 if(r->elapsed==0){
  int length=b->p[LENGTH],direction=b->p[DIRECTION];
  if(r->last_direction!=direction||r->last_length!=length){r->next_step=0;r->last_direction=direction;r->last_length=length;}
  int phase=r->next_step%traversal_length(length,direction);
  if(r->jump_pending){
   all_off(r);phase=0;
   while(traversal_step(phase,length,direction)!=0)phase++;
   r->jump_pending=0;
  }
  int step=traversal_step(phase,length,direction);r->current=step;
  r->next_step=(phase+1)%traversal_length(length,direction);
  r->pending=get_step(b,step,r->transpose);
  /* Delay alternate playback positions by up to half a step. */
  r->delay=(phase&1)?period*b->p[SWING]/200:0;
  if(!r->delay){fire(r,b);r->delay=-1;}
 }
 r->elapsed=(r->elapsed+1)%period;
}
#endif
