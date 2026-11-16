# Device source

Build with the [Opus 2.0 beta 4.2 Plugin Kit](https://github.com/LizardMan245/opus/tree/e265bcacaefd797ce8ca11b8d220e8b6e0971adb/plugin-kit) and a Blackfin toolchain. The SDK revision is recorded in [SDK-2.58.json](SDK-2.58.json).

Run `python3 /path/to/plugin-kit/plugin_tool.py doctor`. From this directory, run the kit's `build` command with one of the source paths below and `--out DEVICE.opmod`.

| Device | Build directory |
| --- | --- |
| FLUKE | [fluke-dual/esc.fluke](fluke-dual/esc.fluke) |
| HITS | [grip-release/Hits](grip-release/Hits) |
| FLUX | [grip-release/HitsMod](grip-release/HitsMod) |
| GRIP | [grip-release/Grip](grip-release/Grip) |

Keep the directory structure when building. FLUKE, FLUX, and GRIP include shared headers from HITS.

The original FLUKE project licence is [GPL-3.0](../LICENSE). FLUKE includes [Marbles’ MIT notice](fluke-dual/esc.fluke/LICENSE-Marbles.txt). HITS retains the upstream MIT notices in its Braids-derived sources. GRIP includes its [GPL-3.0 licence](grip-release/Grip/LICENSE).
