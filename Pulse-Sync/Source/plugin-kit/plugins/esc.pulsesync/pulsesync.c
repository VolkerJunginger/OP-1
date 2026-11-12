/* Pulse Sync 1.0.7, GPL-3.0-or-later. R246 only; see FIRMWARE.md. */
#include "opus_plugin.h"
#include "op1fw.h"
#include "pulse_core.h"
#ifdef PULSE_HOST_TEST
extern volatile short test_input[64];
extern volatile unsigned test_ring[1024], test_wr, test_rd, test_mode, test_input_mode, test_rx_phase, test_follow, test_fallback, test_com, test_input_source, test_edit_mode;
#define INPUT test_input
#define RING test_ring
#define WR test_wr
#define RD test_rd
#define MODE test_mode
#define INPUT_MODE test_input_mode
#define INPUT_SOURCE test_input_source
#define EDIT_MODE test_edit_mode
#define RX_PHASE test_rx_phase
#define FOLLOW test_follow
#define FALLBACK test_fallback
#define COM_MODE test_com
static unsigned test_cycles;
static unsigned cycles(void) { return test_cycles; }
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
#define INPUT_SOURCE (*(volatile const unsigned *)0x01dd4d50u)
#define EDIT_MODE (*(volatile const unsigned *)0xff907d58u)
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

#define RATE_SHIFT 19u
#define DETECTOR_CONFIG 0x3bffffu /* Threshold, enable, polarity and rate. */
#define AUTOSTART (1u << 22)
#define PULSE_PRIORITY (1u << 23)
#define RATE_MASK 7u
#define MIDI_TAG 0x50530000u
#define USB_TIMEOUT 200000000u
#define MIN_PULSE_TIMEOUT 40000000u
#define FIRST_PULSE_TIMEOUT 1600000000u

#define MAX_CLOCK_INTERVAL 1600000000u /* Four seconds at 400 MHz. */

enum { PPQ24, PPQ48, PPQ96, CLOCK_QUARTER, CLOCK_EIGHTH, CLOCK_SIXTEENTH, CLOCK_THIRTYSECOND };
static const unsigned ticks_per_pulse[] = {24, 12, 6, 3};
static const char *const rate_label[] = {"24 ppq", "48 ppq", "96 ppq", "1/4", "1/8", "1/16", "1/32"};
static unsigned clock_rate, divider_phase, last_edge, interval, subdivision, next_tick;
static unsigned have_edge;

enum clock_source { SOURCE_NONE, SOURCE_USB, SOURCE_PULSE };
static volatile unsigned selected_source;
static unsigned usb_seen, usb_last, pulse_seen, pulse_last, pulse_period;
static unsigned transport_running;
static const unsigned threshold_levels[] = {
    64, 128, 256, 512, 1024, 2048, 4096, 8192, 12000, 16000
};

static unsigned pulse_timeout(void)
{
    if (!pulse_period) return FIRST_PULSE_TIMEOUT;
    unsigned timeout = pulse_period < MAX_CLOCK_INTERVAL / 2u
        ? pulse_period * 2u : MAX_CLOCK_INTERVAL;
    return timeout < MIN_PULSE_TIMEOUT ? MIN_PULSE_TIMEOUT : timeout;
}

static unsigned choose_source(unsigned cfg, unsigned now)
{
    unsigned pulse_active = pulse_seen && now - pulse_last <= pulse_timeout();
    unsigned usb_active = usb_seen && now - usb_last <= USB_TIMEOUT;
    if (cfg & PULSE_PRIORITY) {
        if (pulse_active) return SOURCE_PULSE;
        if (usb_active) return SOURCE_USB;
    } else {
        if (usb_active) return SOURCE_USB;
        if (pulse_active) return SOURCE_PULSE;
    }
    return SOURCE_NONE;
}

/* Sequencer parser only: refreshed for each realtime packet before dispatch. */
volatile unsigned pulse_use_current_time;

/* Called by the two verified stock realtime-message sync-mode queries.
 * Injected messages use the otherwise unused realtime padding bytes as a tag.
 * Stock R246 reads only the status byte in this branch. */
int clock_sync_mode(const unsigned *packet)
{
    unsigned start = cycles();
    unsigned cfg = config;
    int result = (int)MODE;
    unsigned status = (*packet >> 8) & 255u;
    unsigned tagged = (*packet & 0xffff0000u) == MIDI_TAG;
    if (cfg & (1u << 16)) {
        if (!tagged && status == 0xf8u) {
            usb_seen = 1;
            usb_last = cycles();
        }
        unsigned source = choose_source(cfg, cycles());
        if (tagged && source == SOURCE_USB) result = 0;
        if (!tagged && source == SOURCE_PULSE &&
            (status == 0xf8u || status == 0xfau || status == 0xfbu || status == 0xfcu))
            result = 0;
    }
    pulse_use_current_time = tagged && status == 0xf8u &&
                             (result == 2 || result == 4);
    opus_cpu_charge(me, cycles() - start);
    return result;
}

static int queue_message(unsigned status)
{
    unsigned mask = irq_off();
    unsigned wr = WR, rd = RD;
    int ok = pulse_ring_room(wr, rd);
    if (ok) {
        RING[wr & 1023u] = MIDI_TAG | (status << 8) | 0x0fu;
        publish();
        WR = wr + 1u;
        publish();
    }
    irq_restore(mask);
    if (!ok) ++dropped;
    return ok;
}

/* Observe external USB packets without consuming or editing their queue slots. */
static void observe_usb(unsigned now)
{
    unsigned mask = irq_off();
    unsigned rd = RD, count = WR - rd;
    if (count <= 1023u) {
        for (unsigned i = 0; i < count; ++i) {
            unsigned packet = RING[(rd + i) & 1023u];
            if ((packet & 0xffff0000u) != MIDI_TAG &&
                (packet & 0xff0fu) == 0xf80fu) {
                usb_seen = 1;
                usb_last = now;
                break;
            }
        }
    }
    irq_restore(mask);
}

static void reset_conversion(void)
{
    divider_phase = have_edge = interval = subdivision = next_tick = 0;
}

/* Schedule interior ticks from the last measured interval. The next real
 * edge replaces the predicted boundary, and silence cannot free-run. */
static unsigned convert_pulses(unsigned rate, unsigned count, unsigned now)
{
    if (rate <= PPQ96) {
        unsigned mask = (1u << rate) - 1u;
        unsigned ticks = 0;
        while (count--) {
            if (!divider_phase) ++ticks;
            divider_phase = (divider_phase + 1u) & mask;
        }
        return ticks;
    }

    unsigned elapsed = now - last_edge;
    if (have_edge && (elapsed > MAX_CLOCK_INTERVAL || (interval && elapsed > 2u * interval)))
        reset_conversion();

    if (count) {
        unsigned divisions = ticks_per_pulse[rate - CLOCK_QUARTER];
        unsigned valid = have_edge && count == 1 && elapsed >= divisions;
        last_edge = now;
        have_edge = count == 1;
        interval = valid ? elapsed : 0;
        subdivision = valid ? elapsed / divisions : 0;
        next_tick = subdivision;
        return valid;
    }

    if (!subdivision || elapsed >= interval || elapsed < next_tick)
        return 0;

    /* A delayed callback may miss ticks. Never replay them as a burst. */
    do {
        next_tick += subdivision;
    } while (next_tick <= elapsed);
    if (next_tick >= subdivision * ticks_per_pulse[rate - CLOCK_QUARTER])
        subdivision = 0;
    return 1;
}
static void settings(const char *name, int value)
{
    unsigned level = (unsigned)opus_setting(me, "threshold");
    if (level < 1 || level > 10) level = 5;
    unsigned threshold = threshold_levels[level - 1];
    unsigned enabled = (unsigned)opus_setting(me, "enabled");
    unsigned negative = (unsigned)opus_setting(me, "polarity");
    unsigned mute = (unsigned)opus_setting(me, "mute_input");
    unsigned rate = (unsigned)opus_setting(me, "input_rate");
    if (rate > CLOCK_THIRTYSECOND) rate = PPQ24;
    unsigned automatic = (unsigned)opus_setting(me, "autostart");
    unsigned priority = (unsigned)opus_setting(me, "priority");
    (void)name; (void)value;
    config = threshold | (enabled << 16) | (negative << 17) | (mute << 18) | (rate << RATE_SHIFT) | (automatic << 22) | (priority << 23);
}
/* The kit generates the checked trampoline for this name. */
int orig_01967614(int a, int b, int c);
int pulse_input(int a, int b, int c)
{
    int result = orig_01967614(a, b, c);
    unsigned start = cycles(), cfg = config, peak = 0, ticks = 0;
    unsigned rate = (cfg >> RATE_SHIFT) & RATE_MASK;
    unsigned blank = rate <= PPQ96 ? 88u >> rate : 88u;
    ++input_blocks;
    if ((cfg & DETECTOR_CONFIG) != previous_config) {
        pulse_reset(&detector); previous_config = cfg & DETECTOR_CONFIG;
    }
    {
        for (unsigned i = 0; i < 64; ++i) {
            int sample = INPUT[i];
            unsigned magnitude = (unsigned)(sample < 0 ? -sample : sample);
            if (magnitude > peak) peak = magnitude;
            if (cfg & (1u << 16))
                ticks += pulse_sample(&detector, sample, cfg & 65535u, (cfg >> 17) & 1u, blank);
        }
    } /* Source is selected by the user: no unverified mode interpretation. */
    input_peak = peak;
    detected += ticks;
    opus_cpu_charge(me, cycles() - start);
    return result;
}
/* Stock mic/line control uses mode 0 for external input and 2 for off.
 * Its source enum is separate: 0 mic, 1 line, 2 USB, 3 radio, 4 resample. */
static int line_monitor_muted(unsigned cfg, unsigned mode)
{
    return (cfg & 0x50000u) == 0x50000u && (mode == 2 || mode == 4) &&
           INPUT_MODE == 0 && INPUT_SOURCE == 1;
}

static void mute_clock_input(int instrument, short *buf)
{
    (void)instrument; (void)buf;
    if (line_monitor_muted(config, MODE))
        opus_skip_dry_input();
}

/* Shift + Mic enters stock edit mode 5. The banner tracks the selection
 * even with input off, so the user can see what will happen when enabled. */
static void draw_mute_notice(unsigned short *buf)
{
    (void)buf;
    if (EDIT_MODE == 5 && INPUT_SOURCE == 1 && (config & 0x50000u) == 0x50000u)
        opus_page_text_right(me, 0, 85, 118, 9, OPUS_COLOR_E1, "pulse sync");
}
/* Sequencer thread, before the normal stock MIDI drain. */
static void block(void)
{
    unsigned pulse_count = detected;
    unsigned count = pulse_count - consumed;
    unsigned now = cycles();
    unsigned cfg = config;
    unsigned rate = (cfg >> RATE_SHIFT) & RATE_MASK;
    ++sequence_blocks;
    consumed = pulse_count;

    if (!(cfg & (1u << 16)) || (MODE != 2 && MODE != 4)) {
        reset_conversion();
        pulse_seen = usb_seen = transport_running = 0;
        selected_source = SOURCE_NONE;
        clock_rate = rate;
        return;
    }
    if (rate != clock_rate) {
        reset_conversion();
        pulse_seen = 0;
        pulse_period = 0;
        clock_rate = rate;
        count = 0;
    }
    if (count > 4) {
        dropped += count;
        reset_conversion();
        count = 0;
    }

    unsigned was_active = pulse_seen && now - pulse_last <= pulse_timeout();
    if (count) {
        unsigned elapsed = now - pulse_last;
        if (was_active && elapsed && elapsed <= MAX_CLOCK_INTERVAL)
            pulse_period = elapsed / count;
        else
            pulse_period = 0;
        pulse_last = now;
        pulse_seen = 1;
    }
    observe_usb(now);
    unsigned source = choose_source(cfg, now);
    if (source != selected_source) reset_conversion();
    selected_source = source;

    if (!(cfg & AUTOSTART)) transport_running = 0;
    if (source == SOURCE_USB) {
        transport_running = 0; /* The USB transport now owns playback. */
        reset_conversion();
        return;
    }
    if (source == SOURCE_NONE) {
        if (transport_running && queue_message(0xfcu)) transport_running = 0;
        reset_conversion();
        return;
    }
    if (count && !was_active) reset_conversion();
    if ((cfg & AUTOSTART) && count && !transport_running) {
        if (!queue_message(0xfau)) return;
        transport_running = 1;
    }
    unsigned ticks = convert_pulses(rate, count, now);
    for (unsigned i = 0; i < ticks; ++i) {
        if (queue_message(0xf8u)) ++sent;
    }
}
static void panel(void)
{
    char buf[40];
    unsigned mode = MODE;
    const char *state = !(config & (1u << 16)) ? "off" :
        (mode != 2 && mode != 4) ? "select sync" : COM_MODE ? "exit com mode" : FOLLOW && !FALLBACK ? "following" : "ready";
    opus_panel_text(me, 0, 174, 29, 11, OPUS_COLOR_E2, state);
    fw_snprintf(buf, sizeof buf, "%s pulses %u", rate_label[(config >> RATE_SHIFT) & RATE_MASK], detected);
    opus_panel_text(me, 1, 174, 43, 9, OPUS_COLOR_TEXT, buf);
    fw_snprintf(buf, sizeof buf, "sent %u drop %u", sent, dropped);
    opus_panel_text(me, 2, 174, 57, 9, OPUS_COLOR_TEXT, buf);
    fw_snprintf(buf, sizeof buf, "peak %u", input_peak);
    opus_panel_text(me, 3, 174, 71, 9, OPUS_COLOR_E1, buf);
    fw_snprintf(buf, sizeof buf, "input %u", input_blocks);
    opus_panel_text(me, 4, 174, 85, 9, OPUS_COLOR_DIM, buf);
    fw_snprintf(buf, sizeof buf, "seq %u q %u", sequence_blocks, WR - RD);
    opus_panel_text(me, 5, 174, 99, 9, OPUS_COLOR_DIM, buf);
    const char *source = selected_source == SOURCE_USB ? "usb" :
        selected_source == SOURCE_PULSE ? "pulse" : "waiting";
    fw_snprintf(buf, sizeof buf, "%s rx %u", source, RX_PHASE);
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
    clock_rate = (config >> RATE_SHIFT) & RATE_MASK;
    opus_on_setting(self, settings);
    opus_on_block(self, block);
    opus_on_pre_fx(self, mute_clock_input);
    opus_on_panel(self, panel);
    opus_on_frame(self, draw_mute_notice);
    return 0;
}
