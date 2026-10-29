# Pulse Sync 0.1.3

A background plugin for **Opus 2 on the original OP-1, firmware #246**. Feed a
**line-level 24 PPQ clock** into the audio input to follow an external clock.
The input stays active for detection while **mute clock input** removes the stock
dry-input monitoring clicks. No synth or effect slot is occupied. Play/stop stays
manual; tape behavior stays as provided by the OP-1's Sync mode.

## Download and use

Download [pulsesync.opmod](pulsesync.opmod?raw=true), copy it into the OP-1 disk's
`mods` folder, eject and restart. Select line in, keep the input active, and select
**Sync** on the tempo screen. The plugin's **mute clock input** defaults to on.

See [MANUAL.txt](MANUAL.txt) for the very short manual. Attenuate modular CV to a
suitable line level before connecting it to the OP-1's audio input.

Settings are in the Opus menu, T2, Pulse Sync, blue push: enable, mute clock input,
threshold (default 1024) and polarity (default positive). The panel includes pulse,
queue and clock-follow diagnostics. Internal input resampling is not automatically
excluded; use line in. Separate sampling/recording taps still have access to input.
Do not send a competing MIDI clock over USB.

## Identity update

ID: `esc.pulsesync` (Opus requires lowercase IDs). Author: `ESC_CODE`.
Version 0.1.3 changes identity/version metadata only; its executable code is
identical to the hardware-confirmed 0.1.2 build. Replace the old file rather than
loading both identities. The new ID starts a new settings folder.

## Validation

**The owner confirmed clock sync and input muting in version 0.1.2 on an original OP-1 on
29 October 2026.** That is a check on one setup, not a measurement of every
configuration. Hardware CPU and timing jitter remain unmeasured. The default
Opus CPU protection remains enabled. Hold Stop while powering on to start without
plugins if recovery is needed.

See [VALIDATION.txt](VALIDATION.txt) for the build record and [FIRMWARE.md](FIRMWARE.md)
for the firmware integration details.

## Complete source and build

All plugin code, tests, SDK headers and kit build tools are in
[Source/plugin-kit](Source/plugin-kit). The plugin is in `plugins/esc.pulsesync`.
Install Blackfin GCC 13.3.0 as described in the bundled kit's
[getting-started guide](Source/plugin-kit/docs/getting-started.md), then from this
folder:

```sh
export OP1_BFIN_TOOLCHAIN=/path/to/blackfin-toolchain
python3 Source/plugin-kit/plugin_tool.py build Source/plugin-kit/plugins/esc.pulsesync --out build/pulsesync.opmod
python3 Source/plugin-kit/plugin_tool.py show build/pulsesync.opmod
clang -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -ISource/plugin-kit/sdk Source/plugin-kit/plugins/esc.pulsesync/tests/host.c -o build/pulse-host
./build/pulse-host
```

The tests simulate pulse detection, queue bounds, mute policy and buffer preservation;
they do not emulate the OP-1 firmware or substitute for hardware listening tests.
The optional `tests/verify_firmware.py` reads a firmware file supplied by its owner;
no firmware is bundled.

## License and credits

GNU GPL v3; see [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md). Individual files
retain their original license notices, including any permission for later GPL versions.
Opus and its GPL-3.0 plugin kit are by [LizardMan](https://github.com/LizardMan245/opus).
The kit's original license and notices are preserved. No TE or Opus firmware is distributed.
