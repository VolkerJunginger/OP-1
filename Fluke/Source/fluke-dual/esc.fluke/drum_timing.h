/* Deterministic style/role timing in 96-PPQN ticks. No tempo drift or jitter. */
#ifndef FLUKE_DRUM_TIMING_H
#define FLUKE_DRUM_TIMING_H
static int drum_timing_offset(const DrumLane *b,int phase,int step,int global_swing){
 static const signed char feel[14][4]={
  {0,0,0,0},{0,4,0,2},{0,-8,-6,-4},{0,6,-4,10},
  {0,-6,4,-2},{0,10,-6,4},{0,-8,2,-10},{0,4,-4,6},
  {0,0,0,0},{0,6,-4,4},{0,0,0,0},{0,8,2,6},{0,-4,4,-2},{0,0,0,0}
 };
 int amount=bounded(b->p[DS_SWING]+global_swing,0,100);
 if(!amount||step==0)return 0;
 int role=drum_role(b->seed),period=rate_ticks[b->p[DS_RATE]];
 /* Foundation quarter-note anchors stay exact; the backbeat can sit behind it. */
 if(role==0&&step%4==0)return 0;
 int percent=feel[b->p[DS_STYLE]][step&3];
 if(role==1&&step%8==4)percent+=12;
 if(role==2&&b->p[DS_STYLE]==2&&step%4==2)percent-=10;
 percent=bounded(percent,-25,25);
 int fixed=period*amount*percent;
 int micro=fixed<0?-((-fixed+5000)/10000):(fixed+5000)/10000;
 micro=bounded(micro,-3,3);
 int shuffle=b->p[DS_STYLE]==13;
 int swing=shuffle?(step%4==2?(period*amount+50)/100:0):(phase&1)?period*amount/200:0;
 return bounded(swing+micro,-period/4,shuffle?period*2/3:period/2);
}
#endif
