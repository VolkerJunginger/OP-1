#include <stdlib.h>
#include <time.h>
#include "bubbler_dsp.h"
#ifndef BB_STORAGE
#define BB_STORAGE BB_SAMPLES
#endif
struct handle {struct bb_state s;short ring[BB_STORAGE];};
void *bb_create(void) {struct handle *h=calloc(1,sizeof(*h));bb_init(&h->s);return h;}
void bb_destroy(void *p){free(p);}
void bb_run(void *p,short *l,short *r,const short *par,int frames)
{
 struct handle *h=p;
 for(int i=0;i<frames;i+=64)bb_render(&h->s,h->ring,l+i,r?r+i:0,64,par);
}
double bb_bench(const short *par,int blocks)
{
 struct handle *h=bb_create(); short buf[64];
 struct timespec a,b;
 clock_gettime(CLOCK_MONOTONIC,&a);
 for(int k=0;k<blocks;++k){
  for(int i=0;i<64;++i)buf[i]=(short)(i*127-4000);
  bb_render(&h->s,h->ring,buf,0,64,par);
 }
 clock_gettime(CLOCK_MONOTONIC,&b);bb_destroy(h);
 return b.tv_sec-a.tv_sec+(b.tv_nsec-a.tv_nsec)*1e-9;
}
/* Paired host timing for the optimized paths; not a Blackfin cycle count. */
double bb_bench_mode(const short *par,int blocks,int stereo,int mode,int note)
{
 struct handle *h=bb_create();short l[64],r[64];struct timespec a,b;
 bb_set_transpose(mode);bb_note(note);
 for(int block=0;block<2048;++block){
  for(int i=0;i<64;++i){l[i]=(short)(i*127-4000);r[i]=(short)(3000-i*163);}
  bb_render(&h->s,h->ring,l,stereo?r:0,64,par);
 }
 clock_gettime(CLOCK_MONOTONIC,&a);
 for(int block=0;block<blocks;++block){
  for(int i=0;i<64;++i){l[i]=(short)(i*127-4000);r[i]=(short)(3000-i*163);}
  bb_render(&h->s,h->ring,l,stereo?r:0,64,par);
 }
 clock_gettime(CLOCK_MONOTONIC,&b);bb_destroy(h);
 return b.tv_sec-a.tv_sec+(b.tv_nsec-a.tv_nsec)*1e-9;
}
