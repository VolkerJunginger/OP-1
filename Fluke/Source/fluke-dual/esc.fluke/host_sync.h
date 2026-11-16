/* The SDK supplies 96-PPQN callbacks with a repeating 0..47 phase. Track its
 * continuous phase, and join/restart held sequences only on clock boundaries.
 * 5760 is a common multiple of every supported Rate period. */
#ifndef FLUKE_HOST_SYNC_H
#define FLUKE_HOST_SYNC_H
static struct {int seen,last_pos,phase,reset;int pending[2],selected[2],auto_hold[2],play_hold,play_mode,play_auto;} host_sync;
static void host_sync_init(void){host_sync.seen=0;host_sync.last_pos=host_sync.phase=host_sync.reset=0;host_sync.play_hold=host_sync.play_mode=-1;host_sync.play_auto=0;for(int i=0;i<2;i++)host_sync.pending[i]=host_sync.selected[i]=host_sync.auto_hold[i]=0;}
static void host_sync_clock(int pos){
 pos%=48;if(pos<0)pos+=48;
 if(!host_sync.seen||host_sync.reset){host_sync.phase=pos;host_sync.seen=1;host_sync.reset=0;}
 else {
  int delta=(pos-host_sync.last_pos+48)%48;
  /* A repeated position cannot establish a new clock phase. Keep it on the
   * reported position rather than introducing an invented time advance. */
  host_sync.phase=delta?(host_sync.phase+delta)%5760:pos;
 }
 host_sync.last_pos=pos;
}
static void host_sync_start(int mode){
 if(mode==0)start(&run);else drum_start();
 host_sync.pending[mode]=1;
}
static int host_sync_ready(int mode){
 if(!host_sync.pending[mode])return 1;
 int period=mode==0?rate_ticks[pattern.p[RATE]]:drums.count?rate_ticks[drums.lanes[0].p[DS_RATE]]:24;
 if(host_sync.phase%period)return 0;
 if(mode==1){
  for(int i=0;i<drums.count;i++)drum_run.voices[i].elapsed=host_sync.phase%rate_ticks[drums.lanes[i].p[DS_RATE]];
  drum_run.ticks=(unsigned)host_sync.phase;
 }
 host_sync.pending[mode]=0;return 1;
}
#endif
