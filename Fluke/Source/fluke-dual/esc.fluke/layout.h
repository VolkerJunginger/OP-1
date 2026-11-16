#ifndef FLUKE_LAYOUT_H
#define FLUKE_LAYOUT_H
/* Approved layout: lower band 96%, plot gains its 2.12 units, header ~108%. */
#define FLUKE_LOWER_SCALE 983
#define FLUKE_PLOT_SCALE 1063
static int layout_upper_y(int y){return (y-2*1024)*102/100+2*1024;}
static int layout_plot_y(int y){return 31*1024+(y-31)*FLUKE_PLOT_SCALE;}
static void layout_note_matrix(int step,int y,int *m){
 int x=15+19*step;m[0]=1065;m[1]=m[2]=m[3]=m[5]=0;m[4]=1106;
 m[6]=x*(1024-m[0]);m[7]=layout_plot_y(y)-58*m[4];m[8]=1024;
}
/* A native frame overlay provides true right-aligned text with the documented
 * page API. Only a sequencer screen draw arms it, so other pages stay untouched. */
static struct opl_self *layout_owner;
static int layout_header_ready,layout_header_mode,layout_header_mono;
static int layout_drum_edit;
static char layout_root[16],layout_scale[24];
static int layout_separator_x(const char *s){
 /* Separator sits in the generous inter-word gap. Optical spacing estimate;
  * the full text's outer right edge is placed by native text_right, not this. */
 int width=0;for(int i=0;s[i];i++)width+=s[i]=='m'||s[i]=='w'?90:s[i]=='i'||s[i]=='j'||s[i]=='l'?34:s[i]=='-'?48:66;
 return 311*256-width*256/10-8*256;
}
static void layout_header_frame(unsigned short *buf){
 (void)buf;if(!layout_header_ready||!layout_owner)return;layout_header_ready=0;
 char label[48];
 if(layout_header_mode==0&&layout_scale[0])fw_snprintf(label,sizeof label,"%s   %s",layout_root,layout_scale);
 else fw_snprintf(label,sizeof label,"%s",layout_root);
 int color=layout_header_mono?OPUS_RGB(0xf0f0f0):OPUS_RGB(0xfff0cf);
 if(layout_header_mode!=1||!layout_drum_edit)
  opus_page_text_right(layout_owner,0,311,9,12,color,label);
 if(layout_header_mode==0&&layout_scale[0]){
  int x=layout_separator_x(layout_scale);
  opus_page_stroke_line(x,8*256,x,20*256,166,layout_header_mono?OPUS_RGB(0x484848):OPUS_RGB(0x5d3a61));
 }
}
#endif
