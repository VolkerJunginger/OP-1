/* Separate Drum save files. Melody snapshot0/1.dat remain untouched. */
#ifndef FLUKE_DRUM_SNAPSHOT_H
#define FLUKE_DRUM_SNAPSHOT_H
struct saved_drums {uint32_t magic,format,bytes,generation;DrumPattern data;uint32_t checksum;};
static struct saved_drums drum_snapshot_io __attribute__((aligned(32)));
static DrumPattern drum_snapshot_cache,drum_recall_data;
static int drum_snapshot_ready,drum_snapshot_slot,drum_recall_pending;
static uint32_t drum_snapshot_generation;
static volatile unsigned drum_snapshot_revision,drum_snapshot_ack;
static volatile int drum_notice,drum_notice_frames;
static char drum_snapshot_paths[2][256];
static void drum_feedback(int n){drum_notice=n;drum_notice_frames=30;}
static uint32_t drum_snapshot_hash(const struct saved_drums *s){const unsigned char *p=(const unsigned char *)s;uint32_t h=2166136261u;for(unsigned i=0;i<sizeof(*s)-4;i++)h=(h^p[i])*16777619u;return h;}
static int drum_lane_valid(const DrumLane *b){
 for(int p=0;p<DS_PARAMS;p++)if(b->p[p]<drum_lo[p]||b->p[p]>drum_hi[p])return 0;
 if((b->visible_mask&~0xffffu)||b->first_note<0||b->first_note>15||!(b->visible_mask&(1u<<b->first_note)))return 0;
 if(drum_count(b)!=b->p[DS_LENGTH]||b->p[DS_ROTATE]>b->p[DS_LENGTH]||b->p[DS_PULSES]>b->p[DS_LENGTH])return 0;
 for(int s=0;s<DRUM_STYLES;s++){unsigned mask=0;for(int i=0;i<16;i++){int rank=b->rank[s][i];if(rank>15||(mask&(1u<<rank)))return 0;mask|=1u<<rank;}}
 return 1;
}
static int drum_snapshot_valid(const struct saved_drums *s){
 if(s->magic!=0x464c4431u||s->format!=2||s->bytes!=sizeof(DrumPattern)||s->checksum!=drum_snapshot_hash(s))return 0;
 const DrumPattern *b=&s->data;if(b->count<0||b->count>DRUM_LANES||!drum_lane_valid(&b->master))return 0;
 unsigned keys=0;
 for(int i=0;i<DRUM_LANES;i++){
  if(!drum_lane_valid(&b->lanes[i]))return 0;
  int k=b->keys[i];if(i>=b->count){if(k!=-1)return 0;continue;}
  if(k<0||k>23||(keys&(1u<<k)))return 0;
  keys|=1u<<k;
 }
 return 1;
}
#include "drum_legacy.h"
static int drum_snapshot_read(const char *path){
 int fd=fw_yaffs_open(path,0,0);if(fd<0)return 0;unsigned off=0;int ok=1;
 while(off<sizeof drum_snapshot_io){unsigned request=sizeof drum_snapshot_io-off;if(request>sizeof snapshot_chunk)request=sizeof snapshot_chunk;int n=fw_yaffs_read(fd,snapshot_chunk,request);if(n==0)break;if(n<0||(unsigned)n>request){ok=0;break;}for(int i=0;i<n;i++)((unsigned char *)&drum_snapshot_io)[off+(unsigned)i]=snapshot_chunk[i];off+=(unsigned)n;}
 if(fw_yaffs_close(fd)!=0)ok=0;
 return ok&&((off==sizeof drum_snapshot_io&&drum_snapshot_valid(&drum_snapshot_io))||drum_snapshot_upgrade(off));
}
static int drum_snapshot_init(struct opl_self *self){
 drum_snapshot_ready=0;drum_snapshot_slot=0;drum_snapshot_generation=0;drum_snapshot_revision=drum_snapshot_ack=0;drum_recall_pending=0;drum_notice=drum_notice_frames=0;
 for(int slot=0;slot<2;slot++){
  int n=fw_snprintf(drum_snapshot_paths[slot],sizeof drum_snapshot_paths[slot],"%s/drum%i.dat",self->folder,slot);if(n<0||(unsigned)n>=sizeof drum_snapshot_paths[slot])return 0;
  if(drum_snapshot_read(drum_snapshot_paths[slot])&&(!drum_snapshot_ready||(int32_t)(drum_snapshot_io.generation-drum_snapshot_generation)>0)){drum_snapshot_cache=drum_snapshot_io.data;drum_snapshot_ready=1;drum_snapshot_generation=drum_snapshot_io.generation;drum_snapshot_slot=slot;}
 }
 return 1;
}
static void drum_snapshot_save(void){drum_snapshot_revision++;drum_snapshot_cache=drums;drum_snapshot_ready=1;drum_snapshot_revision++;drum_feedback(5);}
static void drum_snapshot_apply(void){
 int edit_key=drum_run.edit>=0&&drum_run.edit<drums.count?drums.keys[drum_run.edit]:-1;
 int running=drum_run.running,hold=drum_run.latch,held=drum_run.held;unsigned ticks=drum_run.ticks;
 drum_stop();drums=drum_recall_data;drum_run.latch=hold;drum_run.held=held;drum_run.edit=edit_key>=0?drum_find(edit_key):-1;drum_run.running=running;drum_run.ticks=ticks;
 drum_jump_now();drum_weight_reset();for(int i=0;i<8;i++)drum_turn_acc[i]=0;
 drum_recall_pending=0;drum_feedback(2);
}
static void drum_snapshot_restore(void){if(!drum_snapshot_ready){drum_feedback(3);return;}drum_recall_data=drum_snapshot_cache;drum_recall_pending=1;if(!drum_run.running)drum_snapshot_apply();}
static void drum_snapshot_tick(void){if(drum_recall_pending&&(!drum_run.running||!drums.count||drum_run.voices[0].elapsed==0))drum_snapshot_apply();}
static void drum_snapshot_main(void){
 unsigned revision=drum_snapshot_revision;if(!drum_snapshot_ready||(revision&1)||revision==drum_snapshot_ack)return;
 drum_snapshot_io.data=drum_snapshot_cache;if(revision!=drum_snapshot_revision)return;
 drum_snapshot_io.magic=0x464c4431u;drum_snapshot_io.format=2;drum_snapshot_io.bytes=sizeof(DrumPattern);drum_snapshot_io.generation=drum_snapshot_generation+1;drum_snapshot_io.checksum=drum_snapshot_hash(&drum_snapshot_io);
 int ok=drum_snapshot_valid(&drum_snapshot_io)&&opus_flash_headroom()>=8192,slot=drum_snapshot_slot^1,fd=-1;
 if(ok){fw_yaffs_mkdir(snapshot_owner->folder,0x4180);fd=fw_yaffs_open(drum_snapshot_paths[slot],2|0x40|0x200,0x180);if(fd<0)ok=0;}
 if(ok){unsigned off=0;while(off<sizeof drum_snapshot_io){int n=fw_yaffs_write(fd,(const unsigned char *)&drum_snapshot_io+off,sizeof drum_snapshot_io-off);if(n<=0||(unsigned)n>sizeof drum_snapshot_io-off){ok=0;break;}off+=(unsigned)n;}}
 if(fd>=0&&fw_yaffs_close(fd)!=0)ok=0;
 if(ok){drum_snapshot_generation=drum_snapshot_io.generation;drum_snapshot_slot=slot;}
 drum_snapshot_ack=revision;if(revision==drum_snapshot_revision)drum_feedback(ok?1:4);
}
#endif
