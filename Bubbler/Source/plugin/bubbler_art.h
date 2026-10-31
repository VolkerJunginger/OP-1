/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BUBBLER_ART_H
#define BUBBLER_ART_H
struct opl_self;
void bb_art_begin_frame(void);
void bb_art_repair_plot(void);
void bb_art_draw(struct opl_self *self,const short par[4],unsigned page);
#endif
