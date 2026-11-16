/* Original Fluke scoring heuristics informed by Malcolm Braff's discussion of
 * groove balance, potential energy and interacting rhythmic layers. These tick-
 * based measures are not a reproduction or validation of his complete theory.
 * Prepared only for New; playback and host-clock phase are unchanged. */
#ifndef FLUKE_DRUM_GROOVE_SCORE_H
#define FLUKE_DRUM_GROOVE_SCORE_H
#define DRUM_SCORE_BAR 384
typedef struct {int signed_sum,absolute_sum,lean_sum,hits,period;} DrumMotion;
static DrumMotion drum_motion(const DrumLane *b,int global_swing){
 DrumMotion m={0,0,0,0,rate_ticks[b->p[DS_RATE]]};
 int span=traversal_length(b->p[DS_LENGTH],b->p[DS_DIRECTION]);
 int amount=bounded(b->p[DS_SWING]+global_swing,0,100);
 for(int boundary=0;boundary<=DRUM_SCORE_BAR;boundary+=m.period){
  int phase=(boundary/m.period)%span,step=traversal_step(phase,b->p[DS_LENGTH],b->p[DS_DIRECTION]);
  int offset=drum_timing_offset(b,phase,step,global_swing);if(!boundary&&offset<0)offset=0;
  int t=boundary+offset;if(t<0||t>=DRUM_SCORE_BAR||!drum_base_hit(b,step))continue;
  /* Separate Style push/pull from the user's ordinary long/short Swing. */
  int neutral=b->p[DS_STYLE]==13?(step%4==2?(m.period*amount+50)/100:0):(phase&1)?m.period*amount/200:0;
  if(step==0||(drum_role(b->seed)==0&&step%4==0))neutral=0;
  if(b->p[DS_STYLE]==13)neutral=bounded(neutral,0,m.period*2/3);
  m.signed_sum+=offset;m.absolute_sum+=drum_abs(offset);m.lean_sum+=offset-neutral;m.hits++;
 }
 return m;
}
static int drum_motion_score(const DrumLane *b,int global_swing){
 static const signed char bias[14]={0,1,-1,0,-1,1,-1,0,0,0,0,1,0,0};
 static const unsigned char upper[6]={12,24,42,42,45,65};
 DrumMotion m=drum_motion(b,global_swing);if(!m.hits)return 0;
 int amount=bounded(b->p[DS_SWING]+global_swing,0,100),role=drum_role(b->seed),style=b->p[DS_STYLE];
 int target=bias[style]*128;if(role==0)target=0;else if(role==1)target+=512;else if(role==2&&style==2)target-=384;
 target=target*amount/100;
 int penalty=drum_abs(m.lean_sum*256/m.hits-target)/128;if(role==0)penalty*=2;
 int energy=m.absolute_sum*10000/(m.period*m.hits),high=upper[role]*amount;
 int low=role>1?((style==13?6:style==9||style==11||style==12?2:0)*amount):0;
 if(energy>high)penalty+=bounded((energy-high)/100,0,18);
 if(energy<low)penalty+=bounded((low-energy)/100,0,6);
 return -penalty;
}
static int drum_onset_count(const DrumOnsets *m){int n=0;for(int w=0;w<12;w++)n+=drum_bits(m->w[w]);return n;}
static int drum_near_onset(const DrumOnsets *m,int tick){
 /* Allow the existing three-tick microtiming to retain a musical connection. */
 for(int d=-3;d<=3;d++){int t=(tick+d+DRUM_SCORE_BAR)%DRUM_SCORE_BAR;if((m->w[t/32]>>(t%32))&1u)return 1;}return 0;
}
static int drum_relationship_score(const DrumPattern *kit,const DrumOnsets *masks){
 int score=0;
 for(int i=0;i<kit->count;i++)for(int j=0;j<i;j++){
  int a=drum_role(kit->lanes[i].seed),b=drum_role(kit->lanes[j].seed);
  /* Foundation/backbeat unisons can be structural (for example a one-drop). */
  int counters=(a==3&&b==4)||(a==4&&b==3);
  int support=((a==3||a==4)&&b==2)||((b==3||b==4)&&a==2);
  if(!counters&&!support)continue;
  int na=drum_onset_count(&masks[i]),nb=drum_onset_count(&masks[j]);if(!na||!nb)continue;
  int shared_a=0,shared_b=0,first_a=0,first_b=0;
  for(int t=0;t<DRUM_SCORE_BAR;t++){
   int on=(masks[i].w[t/32]>>(t%32))&1u;
   if(on){shared_a+=drum_near_onset(&masks[j],t);if(t<192)first_a++;}
   if((masks[j].w[t/32]>>(t%32))&1u){shared_b+=drum_near_onset(&masks[i],t);if(t<192)first_b++;}
  }
  int ratio=(shared_a+shared_b)*100/(na+nb),unique=(na+nb-shared_a-shared_b+1)/2;
  /* Add independent detail while retaining a few common points of reference. */
  if(ratio>75)score-=(ratio-75)/2;
  if(counters){
   int latin=kit->lanes[i].p[DS_STYLE]==12||kit->lanes[j].p[DS_STYLE]==12;
   score+=bounded(unique*2,0,latin?14:8);
   if(shared_a||shared_b)score+=latin?4:2;
   int answer=drum_abs(first_a*nb-first_b*na);
   score+=bounded(answer*8/(na*nb),0,latin?6:4);
  }else score+=bounded(unique*2,0,6);
 }
 return score;
}
#endif
