/* One explicit Lift/Drop snapshot. A/B files leave one valid previous commit
 * if a write is interrupted. Files are touched only at init and OP_Main. */
#ifndef FLUKE_SNAPSHOT_H
#define FLUKE_SNAPSHOT_H
struct saved_pattern {
 uint32_t magic,format,bytes,generation;
 Pattern data;
 uint32_t checksum;
};
static struct saved_pattern snapshot_io __attribute__((aligned(32)));
static unsigned char snapshot_chunk[256] __attribute__((aligned(32)));
static Pattern snapshot_cache,recall_pattern;
static int recall_pending;
static int snapshot_ready,snapshot_slot;
static uint32_t snapshot_generation;
static volatile unsigned snapshot_revision,snapshot_ack;
static volatile int snapshot_notice,snapshot_notice_frames;
static char snapshot_paths[2][256];
static struct opl_self *snapshot_owner;
static void snapshot_feedback(int message){snapshot_notice=message;snapshot_notice_frames=30;}
static uint32_t snapshot_hash(const struct saved_pattern *s){
 const unsigned char *p=(const unsigned char *)s;uint32_t h=2166136261u;
 for(unsigned i=0;i<sizeof(*s)-sizeof(s->checksum);i++)h=(h^p[i])*16777619u;
 return h;
}
static int snapshot_valid(const struct saved_pattern *s){
 if(s->magic!=0x464c4b31u||s->format!=1||s->bytes!=sizeof(Pattern)||s->checksum!=snapshot_hash(s))return 0;
 const Pattern *p=&s->data;
 for(int i=0;i<PARAMS;i++)if(p->p[i]<lo[i]||p->p[i]>hi[i])return 0;
 if(p->visible_mask&~0xffffu||p->first_note<0||p->first_note>15||!(p->visible_mask&(1u<<p->first_note)))return 0;
 if(visible_count(p)!=p->p[LENGTH]||p->p[OFFSET]>p->p[LENGTH]||p->p[PULSES]>p->p[LENGTH])return 0;
 int range=p->p[OCTAVE],lower=range<0?12*range:0,upper=lower+12*((range<0?-range:range)+1)-1;
 for(int type=0;type<10;type++)for(int i=0;i<16;i++){
  if(p->pitch[type][i]>24||p->phrase[type][i]<lower||p->phrase[type][i]>upper)return 0;
 }
 for(int d=0;d<16;d++)for(int i=0;i<16;i++)if(p->vel[d][i]>1)return 0;
 return 1;
}
static int snapshot_read_file(const char *path){
 int fd=fw_yaffs_open(path,0,0);if(fd<0)return 0;
 unsigned offset=0;int ok=1;
 while(offset<sizeof snapshot_io){
  unsigned request=sizeof snapshot_io-offset;if(request>sizeof snapshot_chunk)request=sizeof snapshot_chunk;
  int n=fw_yaffs_read(fd,snapshot_chunk,request);
  if(n<=0||(unsigned)n>request){ok=0;break;}
  for(int i=0;i<n;i++)((unsigned char *)&snapshot_io)[offset+(unsigned)i]=snapshot_chunk[i];
  offset+=(unsigned)n;
 }
 if(fw_yaffs_close(fd)!=0)ok=0;
 return ok&&offset==sizeof snapshot_io&&snapshot_valid(&snapshot_io);
}
static int snapshot_init(struct opl_self *self){
 snapshot_owner=self;recall_pending=0;snapshot_ready=0;snapshot_generation=0;snapshot_slot=0;
 snapshot_revision=snapshot_ack=0;snapshot_notice=snapshot_notice_frames=0;
 for(int slot=0;slot<2;slot++){
  int n=fw_snprintf(snapshot_paths[slot],sizeof snapshot_paths[slot],"%s/snapshot%i.dat",self->folder,slot);
  if(n<0||(unsigned)n>=sizeof snapshot_paths[slot])return 0;
  if(snapshot_read_file(snapshot_paths[slot])&&(!snapshot_ready||(int32_t)(snapshot_io.generation-snapshot_generation)>0)){
   snapshot_cache=snapshot_io.data;snapshot_ready=1;snapshot_generation=snapshot_io.generation;snapshot_slot=slot;
  }
 }
 return 1;
}
/* Key thread: capture now, not after the musician has changed more controls. */
static void snapshot_save(void){
 snapshot_revision++;snapshot_cache=pattern;snapshot_ready=1;snapshot_revision++;
 snapshot_feedback(5);
}
static void snapshot_apply(void){
 all_off(&run);*previous_pattern=recall_pattern;
 Pattern *old=current_pattern;current_pattern=previous_pattern;previous_pattern=old;
 for(int i=0;i<8;i++)turn_acc[i]=0;
 run.transpose=0;run.next_step=0;run.last_direction=-1;run.last_length=0;
 run.jump_pending=run.running;recall_pending=0;snapshot_feedback(2);
}
static void snapshot_restore(void){
 if(!snapshot_ready){snapshot_feedback(3);return;}
 recall_pattern=snapshot_cache;recall_pending=1;
 if(!run.running)snapshot_apply();
}
static void snapshot_tick(void){
 if(recall_pending&&(!run.running||run.elapsed==0))snapshot_apply();
}
/* Main thread: one small explicit save; no work is added to audio/tick. */
static void snapshot_main(void){
 unsigned revision=snapshot_revision;
 if(!snapshot_ready||(revision&1)||revision==snapshot_ack)return;
 snapshot_io.data=snapshot_cache;
 if(revision!=snapshot_revision)return;
 snapshot_io.magic=0x464c4b31u;snapshot_io.format=1;snapshot_io.bytes=sizeof(Pattern);
 snapshot_io.generation=snapshot_generation+1;snapshot_io.checksum=snapshot_hash(&snapshot_io);
 int ok=snapshot_valid(&snapshot_io)&&opus_flash_headroom()>=8192;
 int slot=snapshot_slot^1,fd=-1;
 if(ok){fw_yaffs_mkdir(snapshot_owner->folder,0x4180);fd=fw_yaffs_open(snapshot_paths[slot],2|0x40|0x200,0x180);if(fd<0)ok=0;}
 if(ok){
  unsigned offset=0;
  while(offset<sizeof snapshot_io){
   int n=fw_yaffs_write(fd,(const unsigned char *)&snapshot_io+offset,sizeof snapshot_io-offset);
   if(n<=0||(unsigned)n>sizeof snapshot_io-offset){ok=0;break;}offset+=(unsigned)n;
  }
 }
 if(fd>=0&&fw_yaffs_close(fd)!=0)ok=0;
 if(ok){snapshot_generation=snapshot_io.generation;snapshot_slot=slot;}
 snapshot_ack=revision;
 if(snapshot_revision==revision)snapshot_feedback(ok?1:4);
}
#endif
