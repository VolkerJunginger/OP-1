#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bubbler_dsp.c"
static short storage[BB_STORAGE];
static unsigned seed=123;
static unsigned next(void){seed=seed*1664525u+1013904223u;return seed;}
int main(void){
 short *ring=storage+2;
 for(unsigned i=0;i<BB_SAMPLES;++i)ring[i]=(short)next();
 ring[-1]=ring[BB_MASK];ring[BB_SAMPLES]=ring[0];ring[BB_SAMPLES+1]=ring[1];
 const int strides[]={-64,-8,-4,-2,-1,1,2,4,8,64};unsigned checks=0;
 for(int trial=0;trial<12000;++trial){
  int a[64]={0},b[64]={0};int n=1+(int)(next()%64u);
  unsigned index=trial%3==0?next()%64u:trial%3==1?BB_MASK-next()%64u:next()&BB_MASK;
  unsigned pos=index<<12,env=128u<<16;int delta=(int)(next()%32769u)-16384;
  if(trial&1){int step=(trial&2)?2048:-2048;pos+=(trial&4)?2048u:0u;
   add_half(ring,a,pos,step,env,delta,n);add_cubic(ring,b,pos,step,env,delta,n);
  }else{int step=strides[next()%(sizeof strides/sizeof *strides)]*4096;
   add_direct(ring,a,pos,step,env,delta,n);add_cubic(ring,b,pos,step,env,delta,n);
  }
  assert(!memcmp(a,b,sizeof a));++checks;
 }
 printf("PASS: %u direct-stride/half-speed comparisons with cubic, including wrap boundaries\n",checks);
 return 0;
}
