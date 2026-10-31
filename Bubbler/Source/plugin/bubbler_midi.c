/* SPDX-License-Identifier: GPL-3.0-or-later
 * Firmware #246 VoiceCtl note-on observer. Entry verified against the owner's
 * stock and Opus firmware; stock note processing always continues unchanged.
 */
#include "bubbler_dsp.h"
extern void orig_01a85750(void *event,int param);
void bubbler_note_on(void *event,int param)
{
    const int *e=event;
    if(e && e[1]>0)bb_note(e[0]);
    orig_01a85750(event,param);
}
