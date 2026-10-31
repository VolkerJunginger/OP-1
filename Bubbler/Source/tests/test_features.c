#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bubbler_dsp.h"
static short storage[BB_STORAGE];static struct bb_state s;
static void controls(short p[8],int resonance){
 short defaults[8]={0,0,32767,32767,29984,0,4108,-16352};memcpy(p,defaults,sizeof defaults);
 struct bb_params q;bb_decode(p,&q);q.scatter=q.reverse=q.size_var=q.jitter=q.pitch_var=q.pitch=0;
 q.density=100;q.size=16384;q.tone=55;q.resonance=resonance;bb_encode(p,&q);
}
static double energy(int resonance){
 short p[8];controls(p,resonance);bb_init(&s);bb_set_transpose(0);double sum=0;
 double hz=bb_cutoff(55<<8);
 for(int b=0;b<2500;++b){short x[64];
  for(int n=0;n<64;++n)x[n]=(short)(300*sin(2*3.141592653589793*hz*(b*64+n)/44100));
  bb_render(&s,storage,x,0,64,p);
  if(b>1000)for(int n=0;n<64;++n)sum+=(double)x[n]*x[n];
 }
 return sum;
}
static void octave_variation_test(void){
 short p[8];int total[3]={0};
 for(int probability=0;probability<=100;probability+=50){
  controls(p,0);struct bb_params q;bb_decode(p,&q);q.pitch_var=probability;bb_encode(p,&q);
  int counts[3]={0};
  for(int trial=0;trial<1000;++trial){
   bb_init(&s);s.seed=(unsigned)trial*196314165u+17u;bb_set_transpose(4);
   short x[64]={0};bb_render(&s,storage,x,0,64,p);
   const int intervals[3]={-5,7,19};
   for(int k=0;k<BB_GRAINS;++k)if(s.grain[k].active){
    int found=0;for(int j=0;j<3;++j)if(s.grain[k].step==(int)lround(4096*pow(2,intervals[j]/12.0))){++counts[j];found=1;}
    assert(found);
   }
  }
  int all=counts[0]+counts[1]+counts[2];assert(all>0);
  if(probability==0)assert(counts[1]==all);
  if(probability==100)assert(counts[1]==0 && counts[0]>0 && counts[2]>0);
  if(probability==50)assert(counts[1]>all*4/10 && counts[1]<all*6/10);
  total[probability/50]=all;
 }
 printf("PASS: octave probability 0/50/100%%; base +7 preserved; %d/%d/%d grains\n",total[0],total[1],total[2]);
}
static void midi_range_test(void){
 for(int note=0;note<128;++note)for(int reverse=0;reverse<2;++reverse)for(int random=0;random<2;++random){
  short p[8];controls(p,63);p[0]=32767;
  struct bb_params q;bb_decode(p,&q);q.size=32767;q.size_var=32767;q.reverse=reverse?32767:0;q.pitch_var=random?100:0;bb_encode(p,&q);
  bb_init(&s);bb_set_transpose(1);bb_note(note);
  {short x=0;bb_render(&s,storage,&x,0,1,p);
   for(int k=0;k<BB_GRAINS;++k)if(s.grain[k].active){
    struct bb_grain *g=&s.grain[k];
    unsigned start=(g->pos-(unsigned)(g->step*(int)g->age))&BB_PHASE_MASK;
    long long lag=(0u-start)&BB_PHASE_MASK;
    long long end=lag+(4096LL-g->step)*g->length;
    assert(lag>=3*4096LL && lag<=(BB_SAMPLES-3)*4096LL);
    assert(end>=3*4096LL && end<=(BB_SAMPLES-3)*4096LL);
   }
  }
  for(int b=0;b<32;++b){short x[64]={0};x[0]=20000;bb_render(&s,storage,x,0,64,p);}
  for(int k=0;k<BB_GRAINS;++k)if(s.grain[k].active){
   int step=s.grain[k].step;if(step<0)step=-step;
   int base=note-60;
   int down=(int)lround(4096*pow(2,(base-12)/12.0)),up=(int)lround(4096*pow(2,(base+12)/12.0));
   assert(random?(step==down||step==up):step==(int)lround(4096*pow(2,base/12.0)));
  }
 }
 puts("PASS: all 128 MIDI notes, both directions, octave variation and maximum grain size");
}
int main(void){
 octave_variation_test();midi_range_test();
 short p[8];controls(p,0);
 const int semitones[6]={0,7,-12,-7,7,12};bb_note(67);
 for(int mode=0;mode<6;++mode){bb_init(&s);bb_set_transpose(mode);
  short x[64]={0};bb_render(&s,storage,x,0,64,p);int seen=0;
  int expected=(int)lround(4096*pow(2,semitones[mode]/12.0));
  for(int i=0;i<BB_GRAINS;++i)if(s.grain[i].active){assert(s.grain[i].step==expected);++seen;}
  assert(seen);
 }
 bb_set_transpose(0);double a=energy(0),b=energy(63);assert(a>0 && b>a*4);
 printf("PASS: six transpose modes and MIDI C4 reference; resonance peak energy ratio %.2f\n",b/a);
 /* Exercise every frequency/resonance cell and abrupt transitions. */
 bb_init(&s);unsigned seed=17;
 for(int tone=0;tone<128;++tone)for(int res=0;res<64;++res){
  struct bb_params q;bb_decode(p,&q);q.tone=tone;q.resonance=res;bb_encode(p,&q);
  for(int block=0;block<3;++block){short x[64];for(int n=0;n<64;++n){seed=1664525u*seed+1013904223u;x[n]=(short)(seed>>16);}bb_render(&s,storage,x,0,64,p);}
 }
 puts("PASS: all 8192 resonance/frequency combinations and transitions");return 0;
}
