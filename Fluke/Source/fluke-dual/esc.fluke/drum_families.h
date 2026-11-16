/* Original role-aware style grammars. General musical relationships informed by
 * studying Bardet's 260 Drum Machine Patterns; no numbered patterns are copied. */
#ifndef FLUKE_DRUM_FAMILIES_H
#define FLUKE_DRUM_FAMILIES_H
static int drum_family_weight(int style,int role,int i,uint32_t seed){
 int beat=i/4,sub=i%4,back=beat==1||beat==3,detail=(int)(seed&3u),w=0;
 if(role==5)return i>=12?1000+(i-12)*100:i>=8?280:20;
 if(style==8){ /* Rock: stable backbeat, varied kick, eighth-note pulse. */
  if(role==0)w=sub==0&&(beat==0||beat==2)?1500:sub==2&&beat==detail?700:60;
  else if(role==1)w=sub==0&&back?1700:sub==3?220:20;
  else if(role==2)w=sub%2==0?1200:80;
  else if(role==3)w=sub==2&&back?1200:sub==2?600:40;
  else w=sub==3?650:sub==2?300:30;
 }else if(style==9){ /* Funk: syncopated kick and secondary snare details. */
  if(role==0)w=i==0?1600:sub==2||((i+detail)%5==0)?1150:sub==0?500:100;
  else if(role==1)w=sub==0&&back?1700:sub==3?720:sub==1?420:30;
  else if(role==2)w=sub%2==0?1000:((i+detail)%3==0)?700:200;
  else if(role==3)w=sub==2?1200:sub==3?380:50;
  else w=(i+detail)%3==0?1050:sub==1?500:60;
 }else if(style==10){ /* Disco: four-on-the-floor and offbeat cymbals. */
  if(role==0)w=sub==0?1800:20;
  else if(role==1)w=sub==0&&back?1800:sub==3?160:20;
  else if(role==2)w=sub==0?1800:sub==2?80:180;
  else if(role==3)w=sub==2?1800:30;
  else w=sub==2&&back?1100:sub==1?500:40;
 }else if(style==11){ /* Reggae: third-beat foundation, space and offbeats. */
  if(role==0)w=beat==2&&sub==0?2000:sub==0&&beat==3?430:sub==2?180:10;
  else if(role==1)w=beat==2&&sub==0?2000:sub==3?180:20;
  else if(role==2)w=sub==2?1300:sub==0?650:40;
  else if(role==3)w=sub==2?1700:30;
  else w=sub==1||sub==3?750:20;
 }else if(style==12){ /* Latin: stable pulse with anticipations and counter-rhythm. */
  if(role==0)w=i==0||i==8?1500:sub==3&&(beat==1||beat==3)?1200:sub==2?300:30;
  else if(role==1)w=(i+(detail&1)*2)%3==0?1150:sub==2?650:40;
  else if(role==2)w=sub%2==0?1050:140;
  else if(role==3)w=(i+1)%3==0?1300:sub==3?650:40;
  else w=(i+2)%3==0?1500:sub==0?800:40;
 }else if(style==13){ /* Shuffle: paired eighth-note pulse; timing supplies lilt. */
  if(role==0)w=sub==0&&(beat==0||beat==2)?1500:sub==0?800:sub==2?150:20;
  else if(role==1)w=sub==0&&back?1750:sub==3?300:20;
  else if(role==2)w=sub%2==0?1400:30;
  else if(role==3)w=sub==2&&back?1300:sub==2?500:20;
  else w=sub==3?800:sub==1?380:20;
 }
 return w;
}
#endif
