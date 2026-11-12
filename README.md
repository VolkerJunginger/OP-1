# OP-1

Tools and plugins for the **original Teenage Engineering OP-1**.

## ESC Delay Trio

Three SoundHack delay ports by **ESC_CODE**, based on **Tom Erbe’s +bubbler, +delay and +pitchdelay**, built with the **Opus framework**.

| Device | Version | What it does |
|---|---|---|
| [Bubbler](Delay-Trio#bubbler-150) | 1.5.0 | Granular delay with density, timing variation, pitch and reverse grains |
| [Delayed](Delay-Trio#delayed-100) | 1.0.0 | Modulated delay with resonant filter, Freeze and Smooth LFO |
| [Pitch Delay](Delay-Trio#pitch-delay-100) | 1.0.0 | Pitch-shifting delay with octave and loop-depth controls |

[**Download all three devices + illustrated manual**](Delay-Trio/Delay-Trio-Bubbler-1.5.0-Delayed-1.0.0-Pitch-Delay-1.0.0.zip) · [**Manual PDF**](Delay-Trio/Delay%20Trio%20Illustrated%20Manual.pdf) · [**Installation, controls and source**](Delay-Trio)

![Bubbler](Delay-Trio/Screenshots/Bubbler.png)
![Delayed](Delay-Trio/Screenshots/Delayed.png)
![Pitch Delay](Delay-Trio/Screenshots/Pitch-Delay.png)

Requires **original OP-1 firmware 246 and Opus core 2.31+**.
Credit to [Tom Erbe / SoundHack](https://www.soundhack.com/) for the original algorithms, [LizardMan245 and the Opus contributors](https://github.com/LizardMan245/opus) for the framework, and [SquareWave Studio](https://squarewave-studio.github.io/op1-design-kit/) for design guidance. [Full credits and licences](Delay-Trio/Credits.md).

## Midi-Reverb 1.0

A mono ROM-based reverb and effects device by **ESC_CODE**, for the original OP-1 with **Opus core 2.31+**.Feedback, Cutoff, Mix and Program; Orange press changes ROM. Includes Original and approximate Low CPU engines and an optional Output-Filter. Master audio passes through.

[**Download Midi-Reverb**](https://raw.githubusercontent.com/VolkerJunginger/OP-1/main/Midi-Reverb/midireverb.opmod) · [**Installation, ROM requirements, controls and source**](Midi-Reverb) · [**Complete package**](Midi-Reverb/Midi-Reverb-1.0.0.zip)

![Midi-Reverb, ROM bank I](Midi-Reverb/Screenshots/ROM-bank-I.png)

**ROM dumps are required and are not included.** Supply your own compatible dumps and rename them as described in the device README. Credit to [Opus](https://github.com/LizardMan245/opus), [Eric Brombaugh and Paul Schreiber's reverse-engineering work](https://github.com/emeb/MIDIVerb_RE), and [thement's emulator](https://github.com/thement/midiverb_emulator). This independent project is not affiliated with or endorsed by Alesis.

The native build and desktop tests passed.
## Other devices

## Pulse Sync 1.0.1

A background plugin by **ESC_CODE** that synchronizes the original OP-1 to an external pulse clock through its audio input. Supports **24/48/96 PPQ** and **1/4, 1/8, 1/16 or 1/32 note clocks**, converted to the OP-1’s 24 PPQ MIDI clock. Includes adjustable pulse detection, silent clock monitoring, optional AutoStart/Stop and USB/Pulse clock priority.

[**Download Pulse Sync**](https://raw.githubusercontent.com/VolkerJunginger/OP-1/main/Pulse-Sync/pulsesync.opmod) · [**Manual, settings and source**](Pulse-Sync) · [**Complete package**](Pulse-Sync/downloads/Pulse-Sync-1.0.1.zip)

Requires **original OP-1 firmware 246 and Opus 2**. [Opus attribution and licences](Pulse-Sync/NOTICE.md).


- [Fluke](Fluke) — device, manual and source.
