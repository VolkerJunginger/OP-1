#ifndef FLUKE_DRUM_SCREEN_H
#define FLUKE_DRUM_SCREEN_H
static void *drum_keyboard_node,*drum_key_nodes[24],*drum_dot_node,*drum_play_node,*drum_box_node,*drum_lit_node;
static unsigned char *drum_key_colors[24],*drum_dot_color,*drum_glow_colors[3],*drum_box_colors[4],*drum_lit_colors[4],*drum_white_colors[14];
static unsigned drum_row_texts[6][41], drum_edit_text[41], drum_held_root_text[41];
static const unsigned char drum_edit_red[4]={255,66,63,255};
static int drum_text_ready;
static const unsigned char drum_row_color[4]={183,183,183,255};
static void drum_screen_init(void *scr){
 void *doc=*(void **)scr;char id[20];
 drum_lit_node=fw_svg_node_find(doc,"drumlit",0);
 for(int i=0;i<4;i++){fw_snprintf(id,sizeof id,"dl%i",i);drum_lit_colors[i]=fw_screen_fill(scr,id);}
 for(int i=0;i<14;i++){fw_snprintf(id,sizeof id,"dw%i",i);drum_white_colors[i]=fw_screen_fill(scr,id);}
 drum_box_node=fw_svg_node_find(doc,"drumbox",0);
 for(int i=0;i<4;i++){fw_snprintf(id,sizeof id,"db%i",i);drum_box_colors[i]=fw_screen_line_style(scr,id);}
 drum_keyboard_node=fw_svg_node_find(doc,"drumkeyboard",0);drum_dot_node=fw_svg_node_find(doc,"drumdot",0);drum_play_node=fw_svg_node_find(doc,"drumplay",0);
 if(drum_dot_node)drum_dot_color=fw_screen_fill(scr,"drumdot");
 for(int k=0;k<24;k++){fw_snprintf(id,sizeof id,"dk%i",k);drum_key_nodes[k]=fw_svg_node_find(doc,id,0);if(drum_key_nodes[k]){drum_key_colors[k]=fw_screen_fill(scr,id);}}
 for(int i=0;i<3;i++){fw_snprintf(id,sizeof id,"drumglow%i",i);if(fw_svg_node_find(doc,id,0))drum_glow_colors[i]=fw_screen_line_style(scr,id);}
 if(!drum_text_ready){for(int i=0;i<6;i++)fw_text_new(drum_row_texts[i],8*1024,35+8*i,6,drum_row_color,0);fw_text_new(drum_edit_text,311*1024,9,12,drum_edit_red,2);
 fw_text_new(drum_held_root_text,244*1024,9,12,caption_white,2);
 drum_text_ready=1;}
 show(drum_keyboard_node,0);show(drum_dot_node,0);show(drum_play_node,0);show(drum_box_node,0);show(drum_lit_node,0);
}
static void drum_paint(unsigned char *out,int r,int g,int b){if(!out)return;if(draw_mono){r=(r*77+g*150+b*29)/256;g=b=r;}out[0]=(unsigned char)r;out[1]=(unsigned char)g;out[2]=(unsigned char)b;out[3]=255;}
static void drum_note_name(char *out,unsigned bytes,int key){
 static const char *const names[12]={"c","c#","d","d#","e","f","f#","g","g#","a","a#","b"};
 int midi=53+key;fw_snprintf(out,bytes,"%s%i",names[midi%12],midi/12-1);
}
static DrumLane *drum_display_lane(int edit,int count){return edit>=0?&drums.lanes[edit]:count?&drums.lanes[0]:&drums.master;}
static void drum_screen_draw(void *scr,int dt){
 (void)dt;int count=bounded(drums.count,0,DRUM_LANES),edit=bounded(drum_run.edit,-1,count-1);draw_global_key=-1;draw_mono=drum_notice_frames>0;layout_header_mono=draw_mono;
 for(int i=0;i<14;i++)drum_paint(drum_white_colors[i],root_color[0],root_color[1],root_color[2]);
 show(color_art,!draw_mono);show(mono_art,draw_mono);show(popup,0);
 show(keyboard_beds[0],0);show(keyboard_beds[1],0);show(drum_keyboard_node,1);show(drum_dot_node,0);show(drum_play_node,0);show(drum_box_node,0);show(drum_lit_node,0);
 for(int k=0;k<16;k++){for(int v=0;v<2;v++)show(melody_lines[v][k],0);show(note_nodes[k],0);show(rest_nodes[k],0);show(active_nodes[k],0);show(mono_notes[k],0);show(mono_rests[k],0);show(mono_active[k],0);}
 /* Drum header uses one right-hand field, without a root/scale divider. */
 for(int i=0;i<2;i++)show(header_dividers[i],0);
 for(int k=0;k<24;k++){
  int lane=drum_find(k),edited=lane>=0&&lane==edit;
  show(drum_key_nodes[k],lane>=0);
  const unsigned char *selected=key_selected_green;
  if(edited)drum_paint(drum_key_colors[k],255,66,63);else drum_paint(drum_key_colors[k],selected[0],selected[1],selected[2]);
 }
 for(int i=0;i<4;i++){static const int c[4][3]={{18,51,43},{17,100,71},{4,155,101},{0,237,149}};drum_paint(drum_lit_colors[i],c[i][0],c[i][1],c[i][2]);}
 drum_paint(drum_glow_colors[0],9,59,45);drum_paint(drum_glow_colors[1],4,155,101);drum_paint(drum_glow_colors[2],0,237,149);
 fw_screen_draw(scr);
 int pg=edit>=0&&fw_shift_held();char value[24];
 layout_drum_edit=edit>=0;
 if(edit>=0){
  char name[12];
  drum_note_name(name,sizeof name,drums.keys[edit]);
  fw_snprintf(value,sizeof value,"edit %s",name);
  fw_set_text(drum_edit_text,value);
  fw_text_draw(drum_edit_text,fw_screen_raster(),identity);
  if(drum_run.latch){
   drum_note_name(value,sizeof value,fluke_drum_root()-53);
   fw_set_text(drum_held_root_text,value);
   fw_text_draw(drum_held_root_text,fw_screen_raster(),identity);
  }
 }else{
  drum_note_name(value,sizeof value,fluke_drum_root()-53);
  text(0,value);
 }
 text(1,"");text(15,"");
 static const char *const labels[DS_PARAMS]={"style","density","length","rate","swing","rotate","random","dir","rhythm","pulses"};
 DrumLane *display=drum_display_lane(edit,count);
 for(int encoder=0;encoder<4;encoder++){
  int p=edit<0?(encoder==3?DS_SWING:drum_map[0][encoder]):drum_parameter(display,pg,encoder);
  text(2+encoder,labels[p]);
  int v=display->p[p];
  if(edit<0){
   if(p==DS_SWING)v=drums.master.p[DS_SWING];
   else if(count){int total=0;for(int i=0;i<count;i++)total+=drums.lanes[i].p[p];v=(total+count/2)/count;}
  }
  if(p==DS_STYLE)fw_snprintf(value,sizeof value,"%s",drum_styles[v]);
  else if(p==DS_RATE)fw_snprintf(value,sizeof value,"%s",rate_names[v]);
  else if(p==DS_DIRECTION){static const char *const directions[5]={"fwd","rev","f+r","jump","rjump"};fw_snprintf(value,sizeof value,"%s",directions[v]);}
  else fw_snprintf(value,sizeof value,"%i",v);
  text(6+encoder,value);
 }
 text(10,pg?"":"new");text(11,pg||edit<0?"":display->p[DS_RHYTHM]?"density":"pulses");text(12,"");if(!pg)text(drum_run.latch?14:13,"hold");
 int order[6];for(int i=0;i<count;i++)order[i]=i;
 for(int i=0;i<count;i++)for(int j=i+1;j<count;j++)if(drums.keys[order[j]]<drums.keys[order[i]]){int t=order[i];order[i]=order[j];order[j]=t;}
 int rows=count;
 for(int row=0;row<rows;row++){
  int lane=order[row],y=38+8*row;
  DrumLane *b=&drums.lanes[lane];DrumVoice *v=&drum_run.voices[lane];
  {char name[12];drum_note_name(name,sizeof name,drums.keys[lane]);fw_set_text(drum_row_texts[row],name);int row_y=35+8*row;int row_m[9]={1065,0,0,0,1086,0,8*(1024-1065),layout_upper_y(layout_plot_y(row_y))-row_y*1086,1024};fw_text_draw(drum_row_texts[row],fw_screen_raster(),row_m);}
  for(int step=0;step<b->p[DS_LENGTH];step++){
   int hit=drum_base_hit(b,step);if(drum_run.running&&v->current==step)hit=v->last_hit;
   int x=30+18*step,scale=hit?1085:360;
   int m[9]={scale,0,0,0,scale*102/100,0,x*1024,layout_upper_y(layout_plot_y(y)),1024};
   int playing=hit&&drum_run.running&&v->current==step&&v->delay<0;
   if(playing&&drum_lit_node){int lit[9]={1085,0,0,0,1107,0,x*1024,layout_upper_y(layout_plot_y(y)),1024};show(drum_lit_node,1);fw_node_set_matrix(drum_lit_node,lit);fw_group_draw(drum_lit_node,*(void **)scr,identity,fw_screen_raster());show(drum_lit_node,0);continue;}
   if(hit&&lane==edit)drum_paint(drum_dot_color,255,66,63);else if(hit)drum_paint(drum_dot_color,134,166,255);else drum_paint(drum_dot_color,255,255,255);
   for(int k=0;k<4;k++){if(playing)drum_paint(drum_box_colors[k],0,237,149);else if(lane==edit)drum_paint(drum_box_colors[k],255,66,63);else drum_paint(drum_box_colors[k],186,186,186);}
   if(hit&&drum_box_node){int box[9]={1055,0,0,0,1076,0,x*1024,layout_upper_y(layout_plot_y(y)),1024};show(drum_box_node,1);fw_node_set_matrix(drum_box_node,box);fw_group_draw(drum_box_node,*(void **)scr,identity,fw_screen_raster());show(drum_box_node,0);show(drum_lit_node,0);}
   if(drum_dot_node){show(drum_dot_node,1);fw_node_set_matrix(drum_dot_node,m);fw_shape_draw(drum_dot_node,*(void **)scr,identity,fw_screen_raster());show(drum_dot_node,0);}
  }

 }
 if(draw_mono){static const char *const messages[6]={"","saved","recalled","no save","save fail","saving"};show(popup,1);fw_group_draw(popup,*(void **)scr,identity,fw_screen_raster());show(popup,0);text(16,messages[drum_notice]);drum_notice_frames--;}
}
#endif
