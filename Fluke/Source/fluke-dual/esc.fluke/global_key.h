/* Session-wide harmonic constraint; deliberately outside the saved Pattern.
 * The enable switch is persisted by Opus, while Shift+note selects a tonic
 * until it is cleared or this sequencer is selected again. */
#ifndef FLUKE_GLOBAL_KEY_H
#define FLUKE_GLOBAL_KEY_H
static volatile int global_key_enabled=1;
static volatile int global_key=-1;
static int global_key_tonic(void){
 int tonic=global_key;
 return global_key_enabled&&tonic>=0&&tonic<12?tonic:-1;
}
static int global_key_active(void){return global_key_tonic()>=0;}
static int key_scale_allows(int tonic,int scale,int pitch){
 int degree=(pitch-tonic)%12;if(degree<0)degree+=12;
 return !!(scale_masks[scale]&(1u<<degree));
}
static int global_note_allowed(const Pattern *b,int pitch){
 int tonic=global_key_tonic();
 return tonic<0?1:key_scale_allows(tonic,b->p[SCALE],pitch);
}
static int global_quantize(const Pattern *b,int pitch){
 pitch=bounded(pitch,0,127);
 int tonic=global_key_tonic(),scale=b->p[SCALE];
 if(tonic<0)return pitch;
 int base=12*(b->p[REGISTER]+1)+b->p[ROOT],range=b->p[OCTAVE];
 int low=bounded(base+(range<0?12*range:0),0,127);
 int high=bounded(base+(range>0?12*range:0)+11,0,127);
 /* Preserve the chosen Range wherever MIDI limits permit it. Nearest
  * allowed note wins; exact ties go upward. No table allocation or floats. */
 for(int pass=0;pass<2;pass++){
  for(int d=0;d<12;d++){
   int up=pitch+d,down=pitch-d;
   if(up<=high&&up>=low&&key_scale_allows(tonic,scale,up))return up;
   if(d&&down>=low&&down<=high&&key_scale_allows(tonic,scale,down))return down;
  }
  /* A clipped MIDI endpoint can leave no allowed note in the Range. */
  low=0;high=127;
 }
 return pitch;
}
#endif
