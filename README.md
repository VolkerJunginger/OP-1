# OP-1

Tools and plugins for the **original Teenage Engineering OP-1**.

## SYNC 1.0.1

**SYNC** brings USB MIDI Clock, Ableton Link, and analog Pulse Sync to the original OP-1. Link Audio sends Master, Tape 1–4, Synth, and Drum channels to Ableton Live. Requires R246 firmware and Opus Core 2.68 or later.

[**Download SYNC 1.0.1 (ZIP)**](https://raw.githubusercontent.com/VolkerJunginger/OP-1/main/SYNC/SYNC-1.0.1.zip) · [**English manual (PDF)**](https://raw.githubusercontent.com/VolkerJunginger/OP-1/main/SYNC/SYNC-1.0.1-Manual-EN.pdf) · [Setup, controls and credits](SYNC)

## FLUKE, HITS, FLUX and GRIP

**FLUKE 2.0.4** is a generative melody and drum sequencer. **HITS 1.0.4** is a 24-pad drum engine, **FLUX 1.0.5** adds two LFOs per HITS pad, and **GRIP 1.0.4** is a compressor. The set requires original OP-1 firmware 246 and Opus core 2.57 or newer.

GRIP 1.0.4 hides Kick SC and Hold on synth FX. Kick SC is available on drum FX with HITS selected; Hold is available on drum and master FX with HITS and FLUKE Drum active.

[Device page](https://volkerjunginger.github.io/OP-1/) · [Download all four](docs/downloads/op1-devices.zip) · [Illustrated manual](docs/manuals/FLUKE%20v.2.0.0%20Manual.pdf) · [Installation and source](Fluke)

## ESC Delay Trio

Three SoundHack delay ports by **ESC_CODE**, based on **Tom Erbe’s +bubbler, +delay and +pitchdelay**, built with the **Opus framework**.

| Device | Version | What it does |
|---|---|---|
| [Bubbler](Delay-Trio#bubbler-152) | 1.5.2 | Granular delay with density, timing variation, pitch and reverse grains |
| [Delayed](Delay-Trio#delayed-101) | 1.0.1 | Modulated delay with resonant filter, Freeze and Smooth LFO |
| [Pitch Delay](Delay-Trio#pitch-delay-101) | 1.0.1 | Pitch-shifting delay with octave and loop-depth controls |

[**Download all three devices + illustrated manual**](Delay-Trio/Delay-Trio-Bubbler-1.5.2-Delayed-1.0.1-Pitch-Delay-1.0.1.zip) · [**Manual PDF**](Delay-Trio/Delay%20Trio%20Illustrated%20Manual.pdf) · [**Installation, controls and source**](Delay-Trio)

![Bubbler](Delay-Trio/Screenshots/Bubbler.png)
![Delayed](Delay-Trio/Screenshots/Delayed.png)
![Pitch Delay](Delay-Trio/Screenshots/Pitch-Delay.png)

Requires **original OP-1 firmware 246 and Opus core 2.31+**.
Credit to [Tom Erbe / SoundHack](https://www.soundhack.com/) for the original algorithms, [LizardMan245 and the Opus contributors](https://github.com/LizardMan245/opus) for the framework, and [SquareWave Studio](https://squarewave-studio.github.io/op1-design-kit/) for design guidance. [Full credits and licences](Delay-Trio/Credits.md).

## Midi-Reverb 1.0.1

A mono ROM-based reverb and effects device by **ESC_CODE**, for the original OP-1 with **Opus core 2.31+**.Feedback, Cutoff, Mix and Program; Orange press changes ROM. Includes Original and approximate Low CPU engines and an optional Output-Filter. Master audio passes through.

[**Download Midi-Reverb**](https://raw.githubusercontent.com/VolkerJunginger/OP-1/main/Midi-Reverb/midireverb.opmod) · [**Installation, ROM requirements, controls and source**](Midi-Reverb) · [**Complete package**](Midi-Reverb/Midi-Reverb-1.0.1.zip)

![Midi-Reverb, ROM bank I](Midi-Reverb/Screenshots/ROM-bank-I.png)

**ROM dumps are required and are not included.** Supply your own compatible dumps and rename them as described in the device README. Credit to [Opus](https://github.com/LizardMan245/opus), [Eric Brombaugh and Paul Schreiber's reverse-engineering work](https://github.com/emeb/MIDIVerb_RE), and [thement's emulator](https://github.com/thement/midiverb_emulator). This independent project is not affiliated with or endorsed by Alesis.

The native build and desktop tests passed.
## Other devices

## Pulse Sync 1.1.9

A background plugin by **ESC_CODE** that synchronizes the original OP-1 to an external pulse clock through its audio input. Supports **24/48/96 PPQ** and **1/4, 1/8, 1/16 or 1/32 note clocks**, converted to the OP-1’s 24 PPQ MIDI clock. Includes adjustable pulse detection, silent clock monitoring, optional AutoStart/Stop and USB/Pulse clock priority.

[**Download Pulse Sync**](https://raw.githubusercontent.com/VolkerJunginger/OP-1/main/Pulse-Sync/pulsesync.opmod) · [**Manual, settings and source**](Pulse-Sync) · [**Changelog**](Pulse-Sync/CHANGELOG.md) · [**Complete package**](Pulse-Sync/downloads/Pulse-Sync-1.1.9.zip)

Requires **original OP-1 firmware #246 and Opus core 2.58+**. Select L+R (default), L or R to mute the same channels used for clock. Build and host tests passed. Hardware verification is pending.
