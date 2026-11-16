/* Read-only conversion of the original eight-Style Drum snapshot. */
#ifndef FLUKE_DRUM_LEGACY_H
#define FLUKE_DRUM_LEGACY_H
struct legacy_drum_lane {uint32_t seed;int p[DS_PARAMS];unsigned visible_mask;int first_note;unsigned char rank[8][16];};
struct legacy_drum_pattern {struct legacy_drum_lane lanes[6],master;int keys[6],count;};
struct legacy_saved_drums {uint32_t magic,format,bytes,generation;struct legacy_drum_pattern data;uint32_t checksum;};
static struct legacy_saved_drums drum_legacy_io;
static void drum_upgrade_lane(DrumLane *out,const struct legacy_drum_lane *in){
 out->seed=in->seed;for(int p=0;p<DS_PARAMS;p++)out->p[p]=in->p[p];out->visible_mask=in->visible_mask;out->first_note=in->first_note;
 drum_generate(out);for(int s=0;s<8;s++)for(int i=0;i<16;i++)out->rank[s][i]=in->rank[s][i];
}
static int drum_snapshot_upgrade(unsigned bytes){
 if(bytes!=sizeof drum_legacy_io)return 0;
 for(unsigned i=0;i<bytes;i++)((unsigned char *)&drum_legacy_io)[i]=((const unsigned char *)&drum_snapshot_io)[i];
 const struct legacy_saved_drums *old=&drum_legacy_io;
 if(old->magic!=0x464c4431u||old->format!=1||old->bytes!=sizeof(old->data))return 0;
 uint32_t hash=2166136261u;for(unsigned i=0;i<sizeof(*old)-4;i++)hash=(hash^((const unsigned char *)old)[i])*16777619u;
 if(hash!=old->checksum||old->data.master.p[DS_STYLE]<0||old->data.master.p[DS_STYLE]>=8)return 0;
 for(int i=0;i<6;i++)if(old->data.lanes[i].p[DS_STYLE]<0||old->data.lanes[i].p[DS_STYLE]>=8)return 0;
 drum_upgrade_lane(&drum_snapshot_io.data.master,&old->data.master);
 for(int i=0;i<6;i++){drum_upgrade_lane(&drum_snapshot_io.data.lanes[i],&old->data.lanes[i]);drum_snapshot_io.data.keys[i]=old->data.keys[i];}
 drum_snapshot_io.data.count=old->data.count;
 drum_snapshot_io.magic=old->magic;drum_snapshot_io.format=2;drum_snapshot_io.bytes=sizeof(DrumPattern);drum_snapshot_io.generation=old->generation;
 drum_snapshot_io.checksum=drum_snapshot_hash(&drum_snapshot_io);
 return drum_snapshot_valid(&drum_snapshot_io);
}
#endif
