/* Bounded candidate selection: original Fluke heuristics informed by Vogl's
 * seed-relative variation and role-weighted rhythm comparison. No model/code
 * or training data from the thesis is included. Runs only for New. */
#ifndef FLUKE_DRUM_CANDIDATES_H
#define FLUKE_DRUM_CANDIDATES_H
#define DRUM_CANDIDATES 8
static struct {DrumPattern original,candidate,best;} drum_search;
static int drum_bits(unsigned x){int n=0;while(x){x&=x-1;n++;}return n;}
static int drum_abs(int x){return x<0?-x:x;}
typedef struct {unsigned w[12];} DrumOnsets;
static DrumOnsets drum_onsets(const DrumLane *b,int global_swing){
 DrumOnsets mask={{0}};int period=rate_ticks[b->p[DS_RATE]],span=traversal_length(b->p[DS_LENGTH],b->p[DS_DIRECTION]);
 /* Exact host ticks; use the same signed timing helper as playback. */
 for(int boundary=0;boundary<=384;boundary+=period){
  int phase=(boundary/period)%span,step=traversal_step(phase,b->p[DS_LENGTH],b->p[DS_DIRECTION]);
  int t=boundary+drum_timing_offset(b,phase,step,global_swing);if(!boundary&&t<0)t=0;
  if(t>=0&&t<384&&drum_base_hit(b,step))mask.w[t/32]|=1u<<(t%32);
 }
 return mask;
}
#include "drum_groove_score.h"
static int drum_candidate_score(const DrumPattern *before,const DrumPattern *next,int edit,int target){
 static const int weight[6]={6,8,1,2,3,2};
 DrumOnsets masks[6];int score=0,difference=0,total=0;
 static int active_delta[385];for(int t=0;t<=384;t++)active_delta[t]=0;
 for(int i=0;i<next->count;i++){
  const DrumLane *b=&next->lanes[i];int role=drum_role(b->seed);DrumOnsets mask=drum_onsets(b,next->master.p[DS_SWING]);masks[i]=mask;
  score+=drum_motion_score(b,next->master.p[DS_SWING]);
  int last_end=-1;
  for(int t=0;t<384;t++)if((mask.w[t/32]>>(t%32))&1u){if(t<last_end){active_delta[t]--;if(last_end<384)active_delta[last_end]++;}active_delta[t]++;last_end=t+rate_ticks[b->p[DS_RATE]]/2;if(last_end<384)active_delta[last_end]--;}
  int hits=0;for(int w=0;w<12;w++)hits+=drum_bits(mask.w[w]);if(!hits)score-=90;
  /* Fills may depart more freely; sustained backbones should leave space. */
  int ceiling=role==0?12:role==1?10:role==5?28:24;
  if(hits>ceiling)score-=(hits-ceiling)*(role<2?7:2);
  if(edit<0||i==edit){DrumOnsets old=drum_onsets(&before->lanes[i],before->master.p[DS_SWING]);for(int w=0;w<12;w++)difference+=drum_bits(mask.w[w]^old.w[w])*weight[role];total+=32*weight[role];}
  for(int j=0;j<i;j++){
   int union_count=0,changes=0;for(int w=0;w<12;w++){union_count+=drum_bits(mask.w[w]|masks[j].w[w]);changes+=drum_bits(mask.w[w]^masks[j].w[w]);}
   if(union_count&&changes*5<union_count)score-=45;
   int other_role=drum_role(next->lanes[j].seed);
   if((b->p[DS_STYLE]>=8||next->lanes[j].p[DS_STYLE]>=8)&&((role==2&&other_role==3)||(role==3&&other_role==2))){
    for(int w=0;w<12;w++)score-=3*drum_bits(mask.w[w]&masks[j].w[w]);
   }
  }
 }
 int active=0;
 for(int slot=0;slot<384;slot++){
  active+=active_delta[slot];if(active>4)score-=(active-4)*4;
  int hits=0;for(int i=0;i<next->count;i++)hits+=(masks[i].w[slot/32]>>(slot%32))&1u;
  if(hits>4)score-=(hits-4)*100;
 }
 score+=drum_relationship_score(next,masks);
 /* Density is evaluated above; this independent term measures rhythmic change. */
 if(total)score-=2*drum_abs(difference*100/total-target);
 return score;
}
static void drum_prepare_candidate(DrumPattern *out,const DrumPattern *before,uint32_t seed,int edit){
 *out=*before;if(edit<0)out->master.seed=random_word(&seed);
 int first=edit<0?0:edit,last=edit<0?out->count:edit+1;unsigned roles=0;
 for(int i=first;i<last;i++){
  if(edit<0){int role=drum_role(out->lanes[i].seed);if(roles&(1u<<role)){role=0;while(roles&(1u<<role))role++;}roles|=1u<<role;out->lanes[i].seed=drum_groove_seed(out->lanes[i].seed,role,out->lanes[i].seed);}
  drum_randomize(&out->lanes[i],random_word(&seed),out->master.seed,edit>=0);
 }
}
static const DrumPattern *drum_choose_candidate(uint32_t seed,int edit){
 drum_search.original=drums;int best=-0x7fffffff;
 uint32_t variation=seed^0x9db1c573u;
 int fill=edit>=0&&drum_role(drums.lanes[edit].seed)==5;
 int target=(fill?40:edit>=0?12:22)+rnd(&variation,fill?26:19);
 for(int n=0;n<DRUM_CANDIDATES;n++){
  drum_prepare_candidate(&drum_search.candidate,&drum_search.original,random_word(&seed),edit);
  int score=drum_candidate_score(&drum_search.original,&drum_search.candidate,edit,target);
  if(n==0||score>best){best=score;drum_search.best=drum_search.candidate;}
 }
 return &drum_search.best;
}
#endif
