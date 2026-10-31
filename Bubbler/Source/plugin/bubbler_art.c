/* SPDX-License-Identifier: GPL-3.0-or-later
 * Original native-vector adaptation of the approved imagegen concept.
 * Display thread only; no raster loading, files, allocations or audio hooks.
 */
#include "opus_plugin.h"
#include "bubbler_art.h"
#include "bubbler_art_data.h"
/* Clean underlay pixels, before the stock plot is drawn over them. The SDK
 * documents column-major RGB565 at 256*(rows-1-x)+y on every panel type. */
#define REPAIR_W 200
#define REPAIR_H 24
static unsigned short clean[REPAIR_W*REPAIR_H];
static int clean_rows,clean_x,clean_y,clean_w,clean_h,clean_valid;
void bb_art_begin_frame(void) {
    clean_valid=0;
}
static unsigned pixel_index(int rows,int x,int y) {
    return (unsigned)(256*(rows-1-x)+y);
}
static void capture_clean(unsigned short *pixels,int rows)
{
    clean_valid=0;
    if(!pixels || rows<320 || rows>400) return;
    int x0=88*rows/320,x1=(244*rows+319)/320;
    int y0=68*rows/320,y1=(84*rows+319)/320;
    int w=x1-x0+1,h=y1-y0+1;
    if(w>REPAIR_W || h>REPAIR_H) return;
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)
    clean[y*w+x]=pixels[pixel_index(rows,x0+x,y0+y)];
    clean_rows=rows;
    clean_x=x0;
    clean_y=y0;
    clean_w=w;
    clean_h=h;
    clean_valid=1;
}
static int neutral(unsigned short p)
{
    int r=((p>>11)&31)*2,g=(p>>5)&63,b=(p&31)*2;
    int lo=r<g?r:g;
    lo=lo<b?lo:b;
    int hi=r>g?r:g;
    hi=hi>b?hi:b;
    return hi-lo<=10 && hi>=10;
}
/* On the target core the native baseline can already exist at underlay
 * time. Match only its long neutral rows inside the central picture band,
 * remove those pixels, then redraw the artwork over the repaired background.
 * Text corners are outside this band; a tall/wide popup cancels the repair. */
static void erase_existing_plot(unsigned short *pixels,int rows)
{
    if(!pixels || rows<320 || rows>400)return;
    int x0=88*rows/320,x1=(244*rows+319)/320;
    int y0=68*rows/320,y1=(84*rows+319)/320;
    int ys[4],firsts[4],lasts[4],count=0;
    for(int y=y0;y<=y1;++y) {
        int first=-1,last=-1,n=0;
        for(int x=x0;x<=x1;++x) {
            if(neutral(pixels[pixel_index(rows,x,y)])) {
                if(first<0)first=x;
                last=x;
                ++n;
            }
        }
        if(n<100*rows/320)continue;
        int span=last-first+1;
        if(span<110*rows/320 || span>140*rows/320 ||
        first<92*rows/320 || first>110*rows/320 ||
        last<218*rows/320 || last>240*rows/320 || count==4)return;
        ys[count]=y;
        firsts[count]=first;
        lasts[count]=last;
        ++count;
    }
    if(!count || ys[count-1]-ys[0]+1>4)return;
    for(int k=0;k<count;++k)for(int x=firsts[k];x<=lasts[k];++x) {
        unsigned short *p=&pixels[pixel_index(rows,x,ys[k])];
        if(neutral(*p))*p=0;
    }
}
void bb_art_repair_plot(void)
{
    if(!clean_valid)return;
    clean_valid=0;
    int rows=0;
    unsigned short *pixels=opus_page_pixels(&rows);
    if(!pixels || rows!=clean_rows)return;
    int changed[REPAIR_H],number=0,first_row=-1,last_row=-1;
    for(int y=0;y<clean_h;++y) {
        int first=clean_w,last=-1,n=0,grey=0;
        for(int x=0;x<clean_w;++x) {
            unsigned short p=pixels[pixel_index(rows,clean_x+x,clean_y+y)];
            if(p!=clean[y*clean_w+x]) {
                if(first==clean_w)first=x;
                last=x;
                ++n;
                grey+=neutral(p);
            }
        }
        if(!n)continue;
        int x0=clean_x+first,x1=clean_x+last,span=last-first+1;
        /* A full-length, thin neutral stock baseline, not a popup or label.
                 * Any other changes anywhere in this band cancel the repair. */
        if(n<100*rows/320 || span<110*rows/320 || span>140*rows/320 ||
        x0<92*rows/320 || x0>110*rows/320 ||
        x1<218*rows/320 || x1>240*rows/320 || grey*4<n*3)return;
        if(number>=4)return;
        changed[number++]=y;
        if(first_row<0)first_row=y;
        last_row=y;
    }
    if(!number || last_row-first_row+1>4)return;
    for(int k=0;k<number;++k) {
        int y=changed[k];
        for(int x=0;x<clean_w;++x)
        pixels[pixel_index(rows,clean_x+x,clean_y+y)]=clean[y*clean_w+x];
    }
}
/* Lift only the illustration; native values and page counter keep their anchors. */
#define ART_UP 3
static int clamp(int x,int a,int b) {
    return x<a?a:x>b?b:x;
}
static void path(const struct bb_art_cmd *p,unsigned n)
{
    opus_page_path();
    for(unsigned i=0;i<n;++i) {
        const short *c=p[i].p;
        if(p[i].op=='M') opus_page_move(c[0]*16,(c[1]*16-ART_UP*256));
        else if(p[i].op=='L') opus_page_line_to(c[0]*16,(c[1]*16-ART_UP*256));
        else if(p[i].op=='C') opus_page_curve_to(c[0]*16,(c[1]*16-ART_UP*256),c[2]*16,(c[3]*16-ART_UP*256),c[4]*16,(c[5]*16-ART_UP*256));
        else if(p[i].op=='Z') opus_page_close();
    }
}
static void boy(int alpha)
{
    if(!alpha) return;
    for(unsigned i=0;i<sizeof BB_BOY_SHAPES/sizeof BB_BOY_SHAPES[0];++i) {
        const struct bb_art_shape *s=&BB_BOY_SHAPES[i];
        path(BB_BOY_CMDS+s->first,s->count);
        if(s->fill) opus_page_fill_path(OPUS_COLOR_E4,alpha);
        else opus_page_stroke(320,OPUS_COLOR_E4,alpha,OPUS_CAP_ROUND,OPUS_JOIN_ROUND);
    }
}
static void ellipse(int x,int y,int rx,int ry,int colour)
{
    int cx=x*256,cy=(y-ART_UP)*256,a=rx*256,b=ry*256,kx=rx*141,ky=ry*141;
    opus_page_path();
    opus_page_move(cx+a,cy);
    opus_page_curve_to(cx+a,cy+ky,cx+kx,cy+b,cx,cy+b);
    opus_page_curve_to(cx-kx,cy+b,cx-a,cy+ky,cx-a,cy);
    opus_page_curve_to(cx-a,cy-ky,cx-kx,cy-b,cx,cy-b);
    opus_page_curve_to(cx+kx,cy-b,cx+a,cy-ky,cx+a,cy);
    opus_page_close();
    opus_page_stroke(448,colour,255,OPUS_CAP_ROUND,OPUS_JOIN_ROUND);
}
static void wand(int delay)
{
    int r=4+delay/8;
    ellipse(143,80,r,7,OPUS_COLOR_E1);
    opus_page_stroke_line(143*256,(87-ART_UP)*256,143*256,(91-ART_UP)*256,448,OPUS_COLOR_E1);
    int length=3+delay/4;
    for(int i=-1;i<=1;++i)
    opus_page_stroke_line((137-length)*256,(80+i*4-ART_UP)*256,136*256,(80+i*3-ART_UP)*256,448,OPUS_COLOR_E1);
}
static void bubbles(int count,int grain)
{
    for(int i=0;i<count;++i) {
        int x=BB_BUBBLES[i][0],y=BB_BUBBLES[i][1]-ART_UP,r=BB_BUBBLES[i][2];
        opus_page_dot(x,y,r,0,OPUS_COLOR_E2);
        if(r<4) continue;
        int radius=(r-2)*256;
        /* A short white highlight lengthens with the grain-size setting. */
        int cy=y*256,cx=x*256;
        int end_x=cx-radius*(80-grain*45/31)/100;
        int end_y=cy-radius*(40+grain*50/31)/100;
        opus_page_path();
        opus_page_move(cx-radius*9/10,cy-radius/10);
        opus_page_curve_to(cx-radius*9/10,cy-radius*5/10,
        end_x-radius/10,end_y+radius/10,end_x,end_y);
        opus_page_stroke(480,OPUS_COLOR_TEXT,255,OPUS_CAP_ROUND,OPUS_JOIN_ROUND);
    }
}
static void sparkles(int grain)
{
    /* Small crosses and curved four-point diamonds echo bubble glints.
         * Fixed accents stay within the picture, away from native readouts. */
    int count=(grain*4+15)/31;
    static const short crosses[2][2]= {
        {
            132,115
        } , {
            211,116
        }
    } ;
    for(int i=0;i<2;++i) {
        if(count<1+i*2)continue;
        int x=crosses[i][0]*256,y=(crosses[i][1]-ART_UP)*256,r=3*256;
        opus_page_stroke_line(x-r,y,x+r,y,448,OPUS_COLOR_TEXT);
        opus_page_stroke_line(x,y-r,x,y+r,448,OPUS_COLOR_TEXT);
    }
    static const short stars[2][3]= {
        {
            142,44,4
        } , {
            226,45,5
        }
    } ;
    for(int i=0;i<2;++i) {
        if(count<2+i*2)continue;
        opus_page_path();
        int x=stars[i][0]*256,y=(stars[i][1]-ART_UP)*256,r=stars[i][2]*256,k=r/3;
        opus_page_move(x,y-r);
        opus_page_curve_to(x+k,y-k,x+k,y-k,x+r,y);
        opus_page_curve_to(x+k,y+k,x+k,y+k,x,y+r);
        opus_page_curve_to(x-k,y+k,x-k,y+k,x-r,y);
        opus_page_curve_to(x-k,y-k,x-k,y-k,x,y-r);
        opus_page_close();
        opus_page_stroke(384,OPUS_COLOR_TEXT,255,OPUS_CAP_ROUND,OPUS_JOIN_ROUND);
    }
}
void bb_art_draw(struct opl_self *self,const short par[4],unsigned page)
{
    int mix=clamp(par[3],0,32767);
    int alpha=128+(mix*127+16383)/32767;
    int count=1+clamp(par[1],0,32767)*11/32767;
    int delay=clamp(par[0],0,32767)>>11,grain=clamp(par[2],0,32767)>>10;
    int rows=0;
    unsigned short *pixels=opus_page_pixels(&rows);
    /* Native readouts may already be composited when this hook runs.
         * Never clear their regions or capture/restore them as artwork. Drawing
         * vectors directly avoids retaining a previous page's glyph pixels. */
    erase_existing_plot(pixels,rows);
    boy(alpha);
    wand(delay);
    bubbles(count,grain);
    sparkles(grain);
    const char *counter=page==0?"1/3":page==1?"2/3":"3/3";
    opus_page_text_right(self,0,172,10,12,OPUS_COLOR_DIM,counter);
    capture_clean(pixels,rows);
}
