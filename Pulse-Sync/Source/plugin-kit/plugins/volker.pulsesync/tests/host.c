/* Host simulations: these do not emulate the OP-1 firmware or USB interrupt. */
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define PULSE_HOST_TEST
#include "../pulsesync.c"
volatile short test_input[64];
volatile unsigned test_ring[1024], test_wr, test_rd, test_mode, test_input_mode, test_rx_phase, test_follow, test_fallback, test_com;
static int enabled = 1, threshold = 1024, polarity, mute_input=1;
static void (*block_cb)(void);
static void (*mute_cb)(int,short *);
static unsigned skips;
static unsigned original_calls;
int orig_01967614(int a, int b, int c) { ++original_calls; assert(a==11 && b==22 && c==33); return 77; }
int opus_setting(struct opl_self *s, const char *n) {
    (void)s;
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
    enabled=1;threshold=1024;polarity=0;mute_input=1;skips=0;pulse_reset(&detector);settings(0,0);
}
static void render(void) { short sound[64];
    for(unsigned i=0;i<64;i++)sound[i]=(short)(i*29-500);
    mute_cb(0,sound);
    for(unsigned i=0;i<64;i++)assert(sound[i]==(short)(i*29-500));
    assert(pulse_input(11,22,33)==77);block_cb();
}
static void drain(void) {
    while(test_rd!=test_wr) { assert(test_ring[test_rd&1023u]==0xf80fu);test_rd++; }
}
static void stream(unsigned rate,unsigned bpm,int neg) {
    reset();polarity=neg;settings(0,0);
    unsigned total=rate*8, expected=0, rng=42;
    for(unsigned t=0;t<total;t+=64) {
        for(unsigned i=0;i<64;i++) {
            unsigned n=t+i;
            /* Rational clock: fractional periods alternate by one sample. */
            unsigned hz24=bpm*24u, tick=n*hz24/(rate*60u);
            unsigned edge=(tick*rate*60u+hz24-1u)/hz24, phase=n-edge;
            if(n<total && phase==0 && tick!=0) ++expected;
            rng=rng*1664525u+1013904223u;
            int noise=(int)((rng>>24)&127)-64;
            int x=(!tick || n>=total)?0:phase<12?10000:phase<20?-5000:noise;
            test_input[i]=(short)(neg?-x:x);
        }
        render();drain();
    }
    assert(detected==expected);assert(sent==expected);assert(dropped==0);
}
int main(void) {
    struct opl_self self={sizeof self,"volker.pulsesync",0,0};assert(opl_init(&self)==0);
    for(unsigned r=0;r<2;r++)for(unsigned b=0;b<3;b++)for(int p=0;p<2;p++) {
        const unsigned bpms[]={60,120,300};stream(r?22050:44100,bpms[b],p);
    }
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
    ticks+=pulse_sample(&d,0,1024,0);ticks+=pulse_sample(&d,12000,1024,0);
    ticks+=pulse_sample(&d,0,1024,0);
    for(unsigned i=0;i<200;i++)ticks+=pulse_sample(&d,12000,1024,0);
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
    assert(test_wr==0 && test_ring[1023]==0xf80fu && sent==1);
    reset();test_wr=1022;detected=2;block_cb();assert(sent==1 && dropped==1 && test_wr==1023);
    /* A delayed consumer discards an old burst instead of accelerating time. */
    reset();detected=9;block_cb();assert(sent==0 && dropped==9);block_cb();assert(dropped==9);
    /* Input extreme and threshold extremes. */
    for(unsigned th=64;th<=16000;th+=15936) {
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
    reset();panel();assert(original_calls>0);
    puts("PASS: 12 noisy 24-PPQ streams (60/120/300 BPM, both rates/polarities), held gates, ringing, stale buffers, enable/sync modes and nonzero-input-flag regression, queue bounds/rollover, stale bursts, extremes, original-call forwarding, mute policy, audio/capture preservation, mute toggle continuity.");
}
