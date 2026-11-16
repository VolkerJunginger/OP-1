/* Copyright 2015 Emilie Gillet. Integer adaptation Copyright 2026 Volker.
 * MIT license in LICENSE-Marbles.txt. Based on Marbles locked RandomSequence,
 * weighted nearest-note Quantizer and major/minor weights in settings.cc.
 * Added equal-tempered scale masks and chord/fifth/root reduction profiles.
 * This is a focused melody adaptation, not the full hardware module. */
#ifndef FLUKE_MARBLES_MELODY_H
#define FLUKE_MARBLES_MELODY_H
static const char *const scale_names[6]={"chrom","major","minor","pent","dorian","mixo"};
static const char *const type_names[10]={"scale","chord","fifth","root","triad","walk","leap","motif","reply","anchor"};
/* One bit per equal-tempered pitch class; all scale tones are available in Scale. */
static const unsigned short scale_masks[6]={0xfff,0xab5,0x5ad,0x295,0x6ad,0x6b5};
/* Original major/minor degree priorities; other modes use matching chord tones. */
static const unsigned char scale_weights[6][12]={
 {255,16,96,24,128,64,8,192,16,96,24,128},
 {255,16,96,24,128,64,8,192,16,96,24,128},
 {255,16,96,128,8,64,4,192,96,16,128,16},
 {255,0,96,0,128,0,0,192,0,96,0,0},
 {255,0,96,128,0,64,0,192,0,96,128,0},
 {255,0,96,0,128,64,0,192,0,96,128,0}
};
static const unsigned char type_thresholds[4]={0,128,192,255};
/* Q16 semitones, nearest enabled note, ties upward. Range: root to +24 st. */
static int marbles_quantize(int value,int type,int scale){
 int lower=0,upper=24;
 for(int note=0;note<=24;note++){
  int degree=note%12;
  if(!(scale_masks[scale]&(1u<<degree))||scale_weights[scale][degree]<type_thresholds[type])continue;
  if((note<<16)<value)lower=note;
  else {upper=note;break;}
 }
 return value<((lower+upper)<<15)?lower:upper;
}
/* Locked Deja Vu branch: prepare 16 random values, then replay deterministically. */
static void marbles_fill(unsigned char pitch[10][16],uint32_t seed,int scale){
 uint32_t state=seed;unsigned short loop[16];
 for(int i=0;i<16;i++)loop[i]=(unsigned short)(random_word(&state)>>16);
 int step=15;
 for(int i=0;i<16;i++){
  step=(step+1)%16;
  int value=(int)loop[step]*24;
  for(int type=0;type<4;type++)pitch[type][i]=(unsigned char)marbles_quantize(value,type,scale);
 }
 /* Additional discrete melody profiles, built on the scale-quantized values. */
 int notes[25],count=0;
 for(int note=0;note<=24;note++)if(scale_masks[scale]&(1u<<(note%12)))notes[count++]=note;
 int third=(scale_masks[scale]&(1u<<4))?4:3;
 int walk=rnd(&state,count),previous=rnd(&state,count);
 for(int i=0;i<16;i++){
  /* Distances are Q16 semitones: initialize above the full two-octave range. */
  int best=0,distance=25*65536,value=(int)loop[i]*24;
  for(int n=0;n<=24;n++)if(n%12==0||n%12==third||n%12==7){
   int d=(n<<16)-value;if(d<0)d=-d;
   if(d<=distance){distance=d;best=n;}
  }
  pitch[4][i]=(unsigned char)best;
  if(i){int step=(rnd(&state,8)?1:3)*(rnd(&state,2)?1:-1);walk=bounded(walk+step,0,count-1);}
  pitch[5][i]=(unsigned char)notes[walk];
  int candidates[25],n=0;
  for(int j=0;j<count;j++)if(j-previous>=3||previous-j>=3)candidates[n++]=j;
  previous=n?candidates[rnd(&state,n)]:rnd(&state,count);
  pitch[6][i]=(unsigned char)notes[previous];
  int motif=pitch[0][i%4],degree=0;
  while(degree<count-1&&notes[degree]<motif)degree++;
  if((i/4)&1 && i%4==3)degree=bounded(degree+1,0,count-1);
  pitch[7][i]=(unsigned char)notes[degree];
  int answer=pitch[0][i%8];degree=0;
  while(degree<count-1&&notes[degree]<answer)degree++;
  if(i>=8)degree=bounded(degree+1,0,count-1);
  pitch[8][i]=(unsigned char)(i==15?0:notes[degree]);
  int other[24],others=0;
  for(int j=0;j<count;j++)if(notes[j]%12)other[others++]=notes[j];
  pitch[9][i]=(unsigned char)((i&1)?other[rnd(&state,others)]:0);
 }

}
#endif
