# FLUX 1.0.5

A dedicated T4 modulator for HITS on the original OP-1. Requires Opus 2 core 2.33 or newer and HITS 1.0.2 or newer with plugin ID `esc.hits`. HITS and FLUKE each load alone. When both are installed, they connect automatically on opening HITS or FLUX.

## Install and play

1. Enable developer mode in the Opus menu if needed. Hold Synth, Drum, Tape and Mixer together to open the menu, then use T1.
2. Enter disk mode with Shift + COM, then T3. Copy `hits.opmod` and `flux.opmod` into the disk’s mods folder. Copy the same `fluke.opmod` whether you are using FLUKE alone or with HITS. Remove the old `hitsfluke.opmod` if present. Eject and restart. In the Opus menu, use T2 to check the installed plugins.
3. In Drum mode, select HITS with Shift + T1. Choose FLUKE as the sequencer if you want FLUKE playback. Use Shift + T4 to choose **FLUX**. Turn on the LFO on T4.
4. With FLUKE connected, hold Shift and press a note to edit it. If FLUKE is the selected sequencer, an unassigned pad gets a lane when there is room. Without FLUKE, FLUX follows the current HITS pad and shows its note name. Both keyboards and the edit prompt are hidden in that setup.
5. Push orange to toggle FLUKE Hold when FLUKE is selected and on. Push blue to switch between LFO A and B. The red waveform and edit brackets show which LFO the four encoder columns edit.

| Encoder | Parameter |
| --- | --- |
| Blue | Sine, triangle, ramp up, ramp down, square |
| Green | Period from 20 to 10,000 ms in 64 steps |
| White | Bipolar depth from 0 to 100% in 32 steps |
| Orange | LENGTH, LEVEL, PITCH, TIMBRE, COLOR, LOW CUT, HIGH CUT |

## Screen

With FLUKE connected, HITS shows its mini keyboard when FLUX is active or when FLUKE is selected and on with the HITS overlay enabled. Hold appears when FLUKE is active. FLUX hides its keyboard and displays “This device needs HITS” when the current kit uses another engine.

The native screen uses FLUKE's original drum keyboard SVG and perspective grid. It updates the key and waveform colors through SVG node handles and uses the same native font calls and lower-band text transform as FLUKE.

## Playback and storage

HITS routes modulation by the actual drum-engine pad number from MIDI, FLUKE or a played key. If FLUKE transposes the sequence, the keyboard still shows its assigned lanes, and the sounding pad's LFO pair is used.

LENGTH is sampled when each hit starts. The other targets follow the oscillator each audio block. PITCH can move by up to one octave in either direction at full depth. LENGTH and LEVEL are the defaults for LFO A and B. Both LFOs can target the same parameter. Their offsets sum and the final parameter is clamped to the HITS range. Depth starts at zero for both LFOs on every pad. There is no retrigger or tempo sync.

The phase clock follows rendered audio blocks independently of note activity. Loading a kit or reselecting the LFO type can reset its runtime phase. The settings use Opus's sound record. Kits saved with the earlier HITS MOD build must select FLUX and re-enter the settings because Opus stores the LFO type by name. Opus preset recall and migration from FLUX 1.1.x to the extended target settings remain to be checked on hardware.

## Desktop check

From the project root on macOS, using the pinned Opus SDK:

```sh
kit=work/opus-baseline-2026-11-15/plugin-kit
clang -std=c11 -Wall -Wextra -Werror -Wno-int-to-pointer-cast \
  -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I "$kit/sdk" work/grip-release/tests/test_hits_mod.c -o /tmp/hits-mod-test
/tmp/hits-mod-test
```

The check runs both plugins' C code together against the SDK callback sequence and compares rendered HITS audio across pads. It also checks waveform values, silent phase, bypass, slot isolation and display state. Hardware loading, UI, audio timing, persistence and CPU use remain unverified.

## 1.0.2 display fix

The defensive OFF guard skips both FLUX drawing paths, but does not select the host OFF presentation. Active FLUX in the synth slot shows Needs HITS, even when HITS is selected in the drum slot. Build/show, embedded SVG and ASan/UBSan passed. Native OFF appearance needs OP-1 confirmation.

### OFF screen limitation, confirmed 2026-11-18

The user reports that 1.0.2 still shows a different OFF screen. The SDK documents that custom LFO page hooks are not called while the LFO is off. The 1.0.2 guard therefore cannot fix the host OFF artwork. The pinned SDK and current upstream expose no native LFO OFF drawing service. `fw_fx_off_draw` is specifically the effect bypass overlay and cannot substitute for it. A verified host implementation or upstream support is needed to complete this request. No corrected OFF build has been validated.
