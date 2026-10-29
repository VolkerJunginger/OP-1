/* Pulse Sync 0.1.2, GPL-3.0-or-later. R246 only; see FIRMWARE.md. */
#include "opus_plugin.h"
#include "op1fw.h"
#include "pulse_core.h"
#ifdef PULSE_HOST_TEST
extern volatile short test_input[64];
extern volatile unsigned test_ring[1024], test_wr, test_rd, test_mode, test_input_mode, test_rx_phase, test_follow, test_fallback, test_com;
#define INPUT test_input
#define RING test_ring
#define WR test_wr
#define RD test_rd
#define MODE test_mode
#define INPUT_MODE test_input_mode
#define RX_PHASE test_rx_phase
#define FOLLOW test_follow
#define FALLBACK test_fallback
#define COM_MODE test_com
static unsigned cycles(void) { return 0; }
static unsigned irq_off(void) { return 0; }
static void irq_restore(unsigned mask) { (void)mask; }
static void publish(void) { }
#else
#define INPUT ((volatile const short *)0xff800cb4u)
#define RING ((volatile unsigned *)0x01ab0510u)
#define WR (*(volatile unsigned *)0x01ab1510u)
#define RD (*(volatile unsigned *)0x01ab1514u)
#define MODE (*(volatile const unsigned *)0x018c1870u)
#define INPUT_MODE (*(volatile const unsigned *)0xff8010b0u)
#define RX_PHASE (*(volatile const unsigned *)0x018b6010u)
#define FOLLOW (*(volatile const unsigned char *)0x018c1898u)
#define FALLBACK (*(volatile const unsigned char *)0x018c1874u)
#define COM_MODE (*(volatile const unsigned char *)0x018b6074u)
static unsigned cycles(void) { unsigned n; __asm__ volatile("%0 = CYCLES;" : "=d"(n)); return n; }
static unsigned irq_off(void) { unsigned n; __asm__ volatile("cli %0;" : "=d"(n) : : "memory"); return n; }
static void irq_restore(unsigned n) { __asm__ volatile("sti %0;" : : "d"(n) : "memory"); }
static void publish(void) { __asm__ volatile("ssync;" : : : "memory"); }
#endif
static struct opl_self *me;
static struct pulse_detector detector;
/* A single packed word is published by the key thread. */
static volatile unsigned config;
static volatile unsigned detected, input_peak, input_blocks, sent, dropped;
static unsigned consumed, previous_config;
static volatile unsigned sequence_blocks;
static void settings(const char *name, int value)
{
    unsigned threshold = (unsigned)opus_setting(me, "threshold");
    unsigned enabled = (unsigned)opus_setting(me, "enabled");
    unsigned negative = (unsigned)opus_setting(me, "polarity");
    unsigned mute = (unsigned)opus_setting(me, "mute_input");
    (void)name; (void)value;
    config = threshold | (enabled << 16) | (negative << 17) | (mute << 18);
}
/* The kit generates the checked trampoline for this name. */
int orig_01967614(int a, int b, int c);
int pulse_input(int a, int b, int c)
{
    int result = orig_01967614(a, b, c);
    unsigned start = cycles(), cfg = config, peak = 0, ticks = 0;
    ++input_blocks;
    if ((cfg & 0x3ffffu) != previous_config) {
        pulse_reset(&detector); previous_config = cfg & 0x3ffffu;
    }
    {
        for (unsigned i = 0; i < 64; ++i) {
            int sample = INPUT[i];
            unsigned magnitude = (unsigned)(sample < 0 ? -sample : sample);
            if (magnitude > peak) peak = magnitude;
            if (cfg & (1u << 16))
                ticks += pulse_sample(&detector, sample, cfg & 65535u, (cfg >> 17) & 1u);
        }
    } /* Source is selected by the user: no unverified mode interpretation. */
    input_peak = peak;
    detected += ticks;
    opus_cpu_charge(me, cycles() - start);
    return result;
}
/* Audio callback: suppress the stock dry monitor, leaving capture and sound buffers intact.
 * The core consumes this request at its dry-input routing hook. */
static void mute_clock_input(int instrument, short *buf)
{
    unsigned cfg = config;
    (void)instrument; (void)buf;
    if ((cfg & 0x50000u) == 0x50000u && (MODE == 2 || MODE == 4))
        opus_skip_dry_input();
}
/* Sequencer thread, before the normal stock MIDI drain. */
static void block(void)
{
    unsigned now = detected, count = now - consumed;
    ++sequence_blocks;
    consumed = now;
    if (!(config & (1u << 16)) || (MODE != 2 && MODE != 4)) return;
    if (count > 4) { dropped += count; return; } /* Do not replay a stale burst. */
    for (unsigned i = 0; i < count; ++i) {
        unsigned mask = irq_off(), wr = WR, rd = RD;
        int ok = pulse_ring_room(wr, rd);
        if (ok) {
            RING[wr & 1023u] = 0x0000f80fu;
            publish();
            WR = wr + 1u;
            publish();
        }
        irq_restore(mask);
        if (ok) ++sent; else ++dropped;
    }
}
static void panel(void)
{
    char buf[40];
    unsigned mode = MODE;
    const char *state = !(config & (1u << 16)) ? "off" :
        (mode != 2 && mode != 4) ? "select sync" : COM_MODE ? "exit com mode" : FOLLOW && !FALLBACK ? "following" : "24 ppq ready";
    opus_panel_text(me, 0, 174, 29, 11, OPUS_COLOR_E2, state);
    fw_snprintf(buf, sizeof buf, "pulses %u", detected);
    opus_panel_text(me, 1, 174, 43, 9, OPUS_COLOR_TEXT, buf);
    fw_snprintf(buf, sizeof buf, "sent %u drop %u", sent, dropped);
    opus_panel_text(me, 2, 174, 57, 9, OPUS_COLOR_TEXT, buf);
    fw_snprintf(buf, sizeof buf, "peak %u", input_peak);
    opus_panel_text(me, 3, 174, 71, 9, OPUS_COLOR_E1, buf);
    fw_snprintf(buf, sizeof buf, "input %u", input_blocks);
    opus_panel_text(me, 4, 174, 85, 9, OPUS_COLOR_DIM, buf);
    fw_snprintf(buf, sizeof buf, "seq %u q %u", sequence_blocks, WR - RD);
    opus_panel_text(me, 5, 174, 99, 9, OPUS_COLOR_DIM, buf);
    fw_snprintf(buf, sizeof buf, "rx %u mode %u", RX_PHASE, INPUT_MODE);
    opus_panel_text(me, 6, 174, 113, 9, OPUS_COLOR_TEXT, buf);
    fw_snprintf(buf, sizeof buf, "ext %u fallback %u", (unsigned)FOLLOW, (unsigned)FALLBACK);
    opus_panel_text(me, 7, 174, 125, 9, OPUS_COLOR_TEXT, buf);
}
static int firmware_ok(void)
{
#ifndef PULSE_HOST_TEST
    static const unsigned char parser[] = {0x00,0xe8,0x06,0x00,0xe3,0x05,0xa6,0x6f,0x08,0xe1,0x10,0x05,0x48,0xe1,0xab,0x01};
    static const unsigned char tick[] = {0x20,0xe4,0x19,0x00,0x20,0x64,0x20,0xe6,0x19,0x00,0x60,0xa0,0x00,0x0c,0x09,0x18};
    volatile const unsigned char *p = (volatile const unsigned char *)0x019e31a4u;
    volatile const unsigned char *t = (volatile const unsigned char *)0x019e3988u;
    for (unsigned i = 0; i < sizeof parser; ++i) if (p[i] != parser[i]) return 0;
    for (unsigned i = 0; i < sizeof tick; ++i) if (t[i] != tick[i]) return 0;
#endif
    return 1;
}
int opl_init(struct opl_self *self)
{
    if (!firmware_ok()) return -246;
    me = self;
    settings(0, 0);
    opus_on_setting(self, settings);
    opus_on_block(self, block);
    opus_on_pre_fx(self, mute_clock_input);
    opus_on_panel(self, panel);
    return 0;
}
