/* Host simulations: these do not emulate the OP-1 firmware or USB interrupt. */
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define PULSE_HOST_TEST
#include "../pulsesync.c"
volatile short test_input[64];
volatile unsigned test_ring[1024], test_wr, test_rd, test_mode, test_input_mode, test_rx_phase, test_follow, test_fallback, test_com;
static int enabled = 1, threshold = 5, polarity, mute_input=1, input_rate, autostart, priority;
static void (*block_cb)(void);
static void (*mute_cb)(int,short *);
static unsigned skips;
static unsigned original_calls;
int orig_01967614(int a, int b, int c) { ++original_calls; assert(a==11 && b==22 && c==33); return 77; }
int opus_setting(struct opl_self *s, const char *n) {
    (void)s;
    if (!strcmp(n, "input_rate")) return input_rate;
    if (!strcmp(n, "autostart")) return autostart;
    if (!strcmp(n, "priority")) return priority;
    return !strcmp(n,"enabled") ? enabled : !strcmp(n,"threshold") ? threshold : !strcmp(n,"polarity") ? polarity : mute_input;
}
void opus_on_setting(struct opl_self *s, void (*f)(const char *,int)) { (void)s; (void)f; }
void opus_on_block(struct opl_self *s, void (*f)(void)) { (void)s; block_cb=f; }
void opus_on_panel(struct opl_self *s, void (*f)(void)) { (void)s; (void)f; }
void opus_on_pre_fx(struct opl_self *s, void (*f)(int,short *)) { (void)s;mute_cb=f; }
void opus_skip_dry_input(void) { ++skips; }
void opus_cpu_charge(struct opl_self *s, unsigned n) { (void)s; (void)n; }
void opus_panel_text(struct opl_self *s,int slot,int x,int y,int size,int colour,const char *t) {
    (void)s;(void)slot;(void)x;(void)y;(void)size;(void)colour;(void)t;
}
int fw_snprintf(char *b, unsigned n, const char *f, ...) { va_list a; va_start(a,f);int r=vsnprintf(b,n,f,a);va_end(a);return r; }
static void reset(void) {
    memset((void *)test_input,0,sizeof test_input);memset((void *)test_ring,0,sizeof test_ring);
    test_wr=test_rd=0;test_mode=2;test_input_mode=0;test_rx_phase=test_follow=test_com=0;test_fallback=1;sequence_blocks=0;
    detected=sent=dropped=input_peak=input_blocks=0;consumed=previous_config=0;
    enabled=1;threshold=5;polarity=0;mute_input=1;skips=0;pulse_reset(&detector);input_rate=0;autostart=priority=0;usb_seen=pulse_seen=pulse_period=transport_running=0;
    selected_source=SOURCE_NONE;test_cycles=0;clock_rate=0;reset_conversion();settings(0,0);
}
static void render(void) { short sound[64];
    for(unsigned i=0;i<64;i++)sound[i]=(short)(i*29-500);
    mute_cb(0,sound);
    for(unsigned i=0;i<64;i++)assert(sound[i]==(short)(i*29-500));
    assert(pulse_input(11,22,33)==77);block_cb();
}
static void drain(void) {
    while(test_rd!=test_wr) { assert(test_ring[test_rd&1023u]==(MIDI_TAG | 0xf80fu));test_rd++; }
}
static void stream(unsigned rate,unsigned bpm,int neg,unsigned input) {
    reset();polarity=neg;input_rate=(int)input;settings(0,0);block_cb();
    unsigned total=rate*8, expected=0, rng=42;
    for(unsigned t=0;t<total;t+=64) {
        for(unsigned i=0;i<64;i++) {
            unsigned n=t+i;
            /* Rational clock: fractional periods alternate by one sample. */
            unsigned hz24=bpm*(24u << input), tick=(uint64_t)n*hz24/(rate*60u);
            unsigned edge=((uint64_t)tick*rate*60u+hz24-1u)/hz24, phase=n-edge;
            if(n<total && phase==0 && tick!=0) ++expected;
            rng=rng*1664525u+1013904223u;
            int noise=(int)((rng>>24)&127)-64;
            int x=(!tick || n>=total)?0:phase<12?10000:phase<20?-5000:noise;
            test_input[i]=(short)(neg?-x:x);
        }
        render();drain();
    }
    assert(detected==expected);assert(sent==(expected+(1u<<input)-1u)/(1u<<input));assert(dropped==0);
}
static void sparse_clock(unsigned rate)
{
    reset();
    input_rate = (int)rate;
    settings(0, 0);
    block_cb();
    unsigned divisions = ticks_per_pulse[rate - CLOCK_QUARTER];
    unsigned step = 1000000u, period = step * divisions;
    unsigned start = UINT_MAX - period / 2;
    test_cycles = start;
    ++detected;
    block_cb();
    assert(sent == 0);

    /* Verify exact output deadlines across counter rollover and stop. */
    for (unsigned slot = 1; slot < divisions * 4u; ++slot) {
        test_cycles = start + slot * step - 1u;
        block_cb();
        unsigned before = sent;
        test_cycles++;
        if (slot == divisions || slot == 2u * divisions) ++detected;
        block_cb();
        assert(sent - before == (slot >= divisions && slot < 3u * divisions));
        drain();
    }
    assert(sent == divisions * 2u);
    /* An expired tempo must be learned from two fresh pulses. */
    test_cycles += period * 3u;
    ++detected;
    block_cb();
    assert(sent == divisions * 2u);
    test_cycles += period;
    ++detected;
    block_cb();
    assert(sent == divisions * 2u + 1u);
    test_mode = 0;
    block_cb();
    test_mode = 2;
    test_cycles += step;
    block_cb();
    assert(sent == divisions * 2u + 1u);
}

/* Process realtime packets through the same source filter called by stock R246. */
static unsigned drain_transport(void)
{
    unsigned events = 0;
    while (test_rd != test_wr) {
        unsigned packet = test_ring[test_rd & 1023u];
        unsigned status = (packet >> 8) & 255u;
        if (clock_sync_mode(&packet)) {
            if (status == 0xf8u) events |= 1u;
            if (status == 0xfau) events |= 2u;
            if (status == 0xfcu) events |= 4u;
        }
        ++test_rd;
    }
    return events;
}

static void incoming(unsigned status)
{
    assert(pulse_ring_room(test_wr, test_rd));
    test_ring[test_wr++ & 1023u] = (status << 8) | 15u;
}

static void transport_tests(void)
{
    /* AutoStart precedes the very first F8, including divided PPQ modes. */
    for (unsigned rate = PPQ24; rate <= CLOCK_THIRTYSECOND; ++rate) {
        reset();
        autostart = 1;
        input_rate = (int)rate;
        settings(0, 0);
        block_cb();
        test_cycles = 10000000u;
        ++detected;
        block_cb();
        assert(((test_ring[test_rd & 1023u] >> 8) & 255u) == 0xfau);
        unsigned initial = drain_transport();
        assert(initial & 2u);
        assert(transport_running);
        unsigned period = rate >= CLOCK_QUARTER
            ? ticks_per_pulse[rate - CLOCK_QUARTER] * 1000000u : 10000000u;
        test_cycles += period;
        ++detected;
        block_cb();
        unsigned ongoing = drain_transport();
        assert(!(ongoing & 2u));
        if (rate == PPQ24 || rate >= CLOCK_QUARTER) assert(ongoing & 1u);
        unsigned timeout = pulse_timeout();
        test_cycles += timeout;
        block_cb();
        assert(!(drain_transport() & 4u));
        ++test_cycles;
        block_cb();
        assert(drain_transport() == 4u);
        block_cb();
        assert(drain_transport() == 0u);
        ++test_cycles;
        ++detected;
        block_cb();
        assert(drain_transport() & 2u);
    }

    /* AutoStart off produces only timing messages. */
    reset();
    ++detected;
    block_cb();
    assert(drain_transport() == 1u);
    test_cycles += FIRST_PULSE_TIMEOUT + 1u;
    block_cb();
    assert(drain_transport() == 0u);

    /* USB preference sees activity, suppresses pulse transport, and later falls back. */
    reset();
    autostart = 1;
    settings(0, 0);
    incoming(0xf8u);
    ++detected;
    block_cb();
    assert(selected_source == SOURCE_USB && !transport_running);
    assert(drain_transport() == 1u);
    test_cycles = USB_TIMEOUT + 1u;
    ++detected;
    block_cb();
    assert(selected_source == SOURCE_PULSE);
    assert(drain_transport() & 2u);

    /* Pulse preference filters external transport while retaining notes and packets. */
    reset();
    priority = 1;
    settings(0, 0);
    incoming(0xfau);
    incoming(0xf8u);
    incoming(0xfbu);
    incoming(0xfcu);
    unsigned note = 0x00643c09u;
    unsigned note_copy = note;
    unsigned original_wr = test_wr;
    ++detected;
    block_cb();
    assert(selected_source == SOURCE_PULSE);
    assert(test_ring[0] == 0xfa0fu && test_ring[1] == 0xf80fu);
    assert(test_wr == original_wr + 1u);
    assert(clock_sync_mode(&note) == 2 && note == note_copy);
    assert(drain_transport() == 1u);
    test_cycles = FIRST_PULSE_TIMEOUT + 1u;
    incoming(0xf8u);
    block_cb();
    assert(selected_source == SOURCE_USB);
    assert(drain_transport() == 1u);

    /* USB ownership must not receive a pulse-generated Stop on handover. */
    reset();
    autostart = 1;
    settings(0, 0);
    ++detected;
    block_cb();
    assert(drain_transport() & 2u);
    test_cycles = 10000000u;
    incoming(0xf8u);
    block_cb();
    assert(selected_source == SOURCE_USB && !transport_running);
    assert(drain_transport() == 1u);

    /* Getter is queried twice by the stock mode-4 branch. Filter both queries. */
    reset();
    test_mode = 4;
    priority = 1;
    settings(0, 0);
    ++detected;
    block_cb();
    drain_transport();
    unsigned usb_tick = 0xf80fu;
    assert(clock_sync_mode(&usb_tick) == 0);
    assert(clock_sync_mode(&usb_tick) == 0);
    unsigned tagged_tick = MIDI_TAG | 0xf80fu;
    assert(clock_sync_mode(&tagged_tick) == 4 && pulse_use_current_time);
    assert(clock_sync_mode(&tagged_tick) == 4 && pulse_use_current_time);
    unsigned tagged_start = MIDI_TAG | 0xfa0fu;
    assert(clock_sync_mode(&tagged_start) == 4 && !pulse_use_current_time);
    test_mode = 2;
    assert(clock_sync_mode(&tagged_tick) == 2 && pulse_use_current_time);
    test_mode = 4;
    enabled = 0;
    settings(0, 0);
    assert(clock_sync_mode(&usb_tick) == 4 && !pulse_use_current_time);

    /* Lowercase settings migrate invalid old raw thresholds to the default. */
    reset();
    const unsigned levels[] = {64,128,256,512,1024,2048,4096,8192,12000,16000};
    for (unsigned i = 1; i <= 10; ++i) {
        threshold = (int)i;
        settings(0, 0);
        assert((config & 65535u) == levels[i - 1u]);
    }
    threshold = 1024;
    settings(0, 0);
    assert((config & 65535u) == 1024u);

    /* Long quarter-note acquisition, missing pulses, and timer rollover. */
    reset();
    autostart = 1;
    input_rate = CLOCK_QUARTER;
    settings(0, 0);
    block_cb();
    test_cycles = UINT_MAX - 500000000u;
    ++detected;
    block_cb();
    assert(drain_transport() == 2u);
    test_cycles += 600000000u; /* 40 BPM quarter notes */
    ++detected;
    block_cb();
    assert(drain_transport() == 1u);
    assert(transport_running && pulse_period == 600000000u);

    /* A full queue must not mark a failed Start as delivered. */
    reset();
    autostart = 1;
    settings(0, 0);
    test_wr = 1023;
    ++detected;
    block_cb();
    assert(!transport_running && dropped == 1u);
    test_rd = test_wr;
    test_cycles += 10000000u;
    ++detected;
    block_cb();
    assert(drain_transport() & 2u);
}

int main(void) {
    struct opl_self self={sizeof self,"esc.pulsesync",0,0};assert(opl_init(&self)==0);
    for(unsigned r=0;r<2;r++)for(unsigned b=0;b<3;b++)for(int p=0;p<2;p++) {
        const unsigned bpms[]={60,120,300};for(unsigned input=0;input<=PPQ96;input++) stream(r?22050:44100,bpms[b],p,input);
    }
    for(unsigned rate=CLOCK_QUARTER;rate<=CLOCK_THIRTYSECOND;rate++) sparse_clock(rate);
    /* No rising edge at startup while a gate is already high; no repeat on a held gate. */
    reset();for(unsigned i=0;i<64;i++)test_input[i]=12000;
    for(unsigned i=0;i<100;i++)render();assert(detected==0);
    memset((void *)test_input,0,sizeof test_input);render();
    for(unsigned i=0;i<64;i++)test_input[i]=12000;
    for(unsigned i=0;i<100;i++)render();assert(detected==1 && sent==1);
    /* Block callback alone cannot re-detect a stale audio buffer. */
    for(unsigned i=0;i<100;i++)block_cb();assert(detected==1 && sent==1);
    /* Retrigger noise inside blanking cannot become a delayed second trigger. */
    struct pulse_detector d={0,0};unsigned ticks=0;
    ticks+=pulse_sample(&d,0,1024,0,88);ticks+=pulse_sample(&d,12000,1024,0,88);
    ticks+=pulse_sample(&d,0,1024,0,88);
    for(unsigned i=0;i<200;i++)ticks+=pulse_sample(&d,12000,1024,0,88);
    assert(ticks==1);
    /* Disabled and non-sync modes never inject; nonzero input flag does not block line input. */
    reset();enabled=0;settings(0,0);test_input[1]=10000;render();assert(sent==0 && detected==0);
    enabled=1;settings(0,0);test_mode=0;render();assert(detected==1 && sent==0);
    test_mode=2;block_cb();assert(sent==0); /* no backlog on entering Sync */
    reset();test_input_mode=1;test_input[2]=10000;render();assert(sent==1 && input_peak==10000);
    /* Full/invalid ring: leave data and both counters alone. */
    reset();test_wr=1023;test_rd=0;test_ring[1023]=0x12345678;detected=1;block_cb();
    assert(test_wr==1023 && test_rd==0 && test_ring[1023]==0x12345678 && dropped==1);
    reset();test_wr=2;test_rd=3;detected=1;block_cb();assert(test_wr==2 && dropped==1);
    /* Free-running counter rollover and last free slot. */
    reset();test_wr=UINT_MAX;test_rd=UINT_MAX-5;detected=1;block_cb();
    assert(test_wr==0 && test_ring[1023]==(MIDI_TAG | 0xf80fu) && sent==1);
    reset();test_wr=1022;detected=2;block_cb();assert(sent==1 && dropped==1 && test_wr==1023);
    /* A delayed consumer discards an old burst instead of accelerating time. */
    reset();detected=9;block_cb();assert(sent==0 && dropped==9);block_cb();assert(dropped==9);
    /* Input extreme and threshold extremes. */
    for(unsigned th=1;th<=10;th+=9) {
        reset();threshold=(int)th;polarity=1;settings(0,0);test_input[2]=-32768;render();
        assert(detected==1 && input_peak==32768);
    }
    /* Mute policy: active sync only, no edits to capture/audio, either instrument. */
    reset();short sound[64];for(unsigned i=0;i<64;i++)sound[i]=(short)(i*31-300);
    test_input[3]=12345;mute_cb(1,sound);assert(skips==1 && test_input[3]==12345);
    for(unsigned i=0;i<64;i++)assert(sound[i]==(short)(i*31-300));
    mute_input=0;settings(0,0);mute_cb(0,sound);assert(skips==1);
    mute_input=1;enabled=0;settings(0,0);mute_cb(0,sound);assert(skips==1);
    enabled=1;settings(0,0);test_mode=0;mute_cb(0,sound);assert(skips==1);
    test_mode=4;mute_cb(0,sound);assert(skips==2);
    /* Toggling mute must not reset an already-high detector and invent a tick. */
    reset();test_input[1]=10000;render();unsigned before=detected;
    mute_input=0;settings(0,0);render();assert(detected==before);
    transport_tests();
    reset();panel();assert(original_calls>0);
    puts("PASS: 36 noisy 24/48/96-PPQ streams and all four sparse rates (60/120/300 BPM, both rates/polarities), held gates, ringing, stale buffers, enable/sync modes and nonzero-input-flag regression, queue bounds/rollover, stale bursts, extremes, original-call forwarding, mute policy, audio/capture preservation, mute toggle continuity, threshold mapping, AutoStart/Stop and clock-source arbitration.");
}
