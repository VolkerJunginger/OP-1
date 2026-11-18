# FLUKE, HITS, FLUX and GRIP

Four Opus plugins for the **original Teenage Engineering OP-1**, firmware **246**.

[Device page](https://volkerjunginger.github.io/OP-1/) · [Download all four](../docs/downloads/op1-devices.zip) · [Illustrated manual](../docs/manuals/FLUKE%20v.2.0.0%20Manual.pdf) · [Build from source](Source/README.md)

| Device | Version | What it does |
| --- | --- | --- |
| [FLUKE](../docs/downloads/fluke.opmod) | 2.0.1 | Generative melody and six-lane drum sequencer |
| [HITS](../docs/downloads/hits.opmod) | 1.0.1 | 24 drum pads and 17 synthesis models |
| [FLUX](../docs/downloads/flux.opmod) | 1.0.2 | Two independent LFOs per HITS pad |
| [GRIP](../docs/downloads/grip.opmod) | 1.0.1 | Compressor for synth, drum, and master FX |

All four devices now accelerate continuous encoder adjustments. FLUX 1.0.2 also shows “needs hits” when selected in the Synth slot. The native LFO OFF display remains an open issue. Build and host tests passed. Hardware verification of these updates is pending.

## Install

The full set requires **Opus core 2.57 or newer**. Put the selected `.opmod` files in the OP-1 disk’s `mods` folder, eject, and restart. Replace earlier builds by filename, including any FLUX build with a higher development version. Remove the old `hitsfluke.opmod` if installed.

- **FLUKE:** Choose it in the Synth or Drum sequencer list. In Drum mode, Shift + Note selects a lane, which HITS and FLUX can follow when installed.
- **HITS:** In Drum mode use Shift + T1 to choose HITS.
- **FLUX:** With HITS selected, choose FLUX using Shift + T4. On T4, push blue to switch LFO A/B. Orange push toggles Hold when FLUKE is active.
- **GRIP:** Choose it in a synth, drum, or master FX slot. On synth or drum FX, white push toggles Kick SC when HITS is the selected drum engine. Orange push toggles Hold when HITS is installed and FLUKE Drum is active.

FLUX requires HITS. FLUKE, HITS, and GRIP each load independently. The related controls appear automatically when their companion plugins are available.

Builds and desktop tests passed. Screen images are desktop previews.

## Credits

Created and adapted for Opus by Volker Junginger / ESC_CODE, with GRIP credited to op1lab. FLUKE and HITS draw on Mutable Instruments’ Marbles, Grids, and Braids. See the [linked credits on the device page](https://volkerjunginger.github.io/OP-1/#credits) and [source notices](Source/README.md).

[Original FLUKE 1.0.0 release](https://github.com/VolkerJunginger/OP-1/releases/tag/fluke-v1.0.0)
