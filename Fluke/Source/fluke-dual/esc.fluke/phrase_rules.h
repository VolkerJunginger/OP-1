/* Light, seed-driven musical preferences. Original Marbles-derived proposals
 * remain dominant; there is no fixed arch, obligatory opening or fixed cadence. */
#ifndef FLUKE_PHRASE_RULES_H
#define FLUKE_PHRASE_RULES_H
static int positive_pc(int n){int p=n%12;return p<0?p+12:p;}
static int phrase_allowed(int note,int type,int scale){
 int pc=positive_pc(note);if(!(scale_masks[scale]&(1u<<pc)))return 0;
 if(type<4)return scale_weights[scale][pc]>=type_thresholds[type];
 if(type==4){int third=(scale_masks[scale]&(1u<<4))?4:3;return pc==0||pc==third||pc==7;}
 return 1;
}
static int phrase_chord(int note,int scale){int pc=positive_pc(note),third=(scale_masks[scale]&(1u<<4))?4:3;return pc==0||pc==third||pc==7;}
static int phrase_neighbor(int note,int direction,int type,int scale,int low,int high){
 for(int n=note+direction;n>=low&&n<=high;n+=direction)if(phrase_allowed(n,type,scale))return n;
 return note;
}
static int interval_distance(int a,int b){int d=a-b;return d<0?-d:d;}
static void shape_phrases(Pattern *b){
 int range=b->p[OCTAVE],octaves=(range<0?-range:range)+1;
 int low=range<0?12*range:0,high=low+12*octaves-1,scale=b->p[SCALE];
 int ids[16],times[16],count=0;
 for(int t=0;t<b->p[LENGTH];t++){
  int pos=traversal_step(t,b->p[LENGTH],b->p[DIRECTION]==2?0:b->p[DIRECTION]);
  if(note_on_grid(b,pos)){ids[count]=visible_note(b,pos);times[count++]=t;}
 }
 for(int type=0;type<10;type++){
  uint32_t rng=b->seed^((unsigned)(type+1)*0x9e3779b9u);
  for(int i=0;i<16;i++)b->phrase[type][i]=(signed char)raw_interval(b->pitch[type][i],range);
  if(type==3){
   int tonics[5],n=0;
   for(int note=low;note<=high;note++)if(!positive_pc(note))tonics[n++]=note;
   for(int i=0;i<16;i++)b->phrase[type][i]=(signed char)tonics[rnd(&rng,n)];
   for(int event=0;event<count;event++){
    int choice=rnd(&rng,n);
    if(n>1&&event==1){while(tonics[choice]==b->phrase[type][ids[0]])choice=(choice+1)%n;}
    if(n>1&&event>1&&b->phrase[type][ids[event-1]]==b->phrase[type][ids[event-2]]&&tonics[choice]==b->phrase[type][ids[event-1]])choice=(choice+1)%n;
    b->phrase[type][ids[event]]=(signed char)tonics[choice];
   }
   continue;
  }
  /* Keep motif, reply and anchor identities intact. */
  if(type>=7)continue;
  int previous=0,delta=0;
  for(int event=0;event<count;event++){
   int note=b->phrase[type][ids[event]],changed=0;
   if(event&&positive_pc(previous)==11&&previous+1<=high&&rnd(&rng,100)<60){note=previous+1;changed=1;}
   if(!changed&&event>1&&interval_distance(delta,0)>7&&rnd(&rng,100)<(type==6?20:35)){
    note=phrase_neighbor(previous,delta>0?-1:1,type,scale,low,high);changed=1;
   }
   if(!changed&&(times[event]*rate_ticks[b->p[RATE]])%192==0&&!phrase_chord(note,scale)&&rnd(&rng,100)<25){
    int best=note,distance=1000;
    for(int n=low;n<=high;n++)if(phrase_allowed(n,type,scale)&&phrase_chord(n,scale)){
     int d=interval_distance(n,note);if(d<distance){distance=d;best=n;}
    }
    note=best;
   }
   b->phrase[type][ids[event]]=(signed char)note;
   if(event)delta=note-previous;
   previous=note;
  }
 }
}
#endif
