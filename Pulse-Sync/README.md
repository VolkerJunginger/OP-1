# Pulse Sync 1.0.1

A background Opus 2 plugin for the original OP-1 on firmware #246.
It converts line-level pulse clocks to 24 PPQ MIDI clock and suppresses
their dry monitoring signal.

[**Download plugin**](pulsesync.opmod) · [**Manual**](MANUAL.txt) · [**Complete package and source**](downloads/Pulse-Sync-1.0.1.zip)

Pulse Sync lets a drum machine, sequencer or modular clock set the OP-1 tempo
through its audio input. It runs in the background when loaded and converts
24, 48 or 96 PPQ pulses, or quarter, eighth, sixteenth or thirty-second note
clocks, into the OP-1's 24 PPQ MIDI clock. Optional input muting removes the
clock clicks from monitoring while keeping pulse detection active. Tape
behavior stays as provided by the OP-1.

## Setup

Copy `pulsesync.opmod` into the OP-1's `mods` folder, eject and restart.
Select line in, leave the input active, and select Sync on the tempo screen.
Set Input Rate to match the clock source. Open the Opus mods list, select
Pulse Sync and press the blue encoder to adjust settings. Use a line-level
signal; attenuate modular CV before connecting it.

Requires the **original OP-1, firmware #246 and Opus 2**.

## Controls

- Pulse Threshold uses a 1–10 scale, default 5. Higher levels require stronger pulses.
- AutoStart defaults to off. On sends MIDI Start with the first selected pulse
  and MIDI Stop after the pulse stream times out.
- Clock Priority defaults to USB. USB or Pulse chooses the preferred clock
  when both are active, with automatic fallback when that source disappears.

See MANUAL.txt for installation, timeout behavior and all input rates.

The threshold levels correspond to raw amplitudes 64, 128, 256, 512, 1024,
2048, 4096, 8192, 12000 and 16000. Default 5 retains the earlier default
trigger level of 1024. Old saved raw settings load the new default.

AutoStop waits two expected input intervals, clamped to 100 ms–4 seconds.
Before a second pulse establishes the interval, it waits 4 seconds. USB
activity expires after 500 ms. When USB takes over, its transport messages
control playback. Changing AutoStart to off leaves playback under manual control.

## 1.0.1

Fixes pulse tempo lock after USB MIDI use. The firmware uses the current time for Pulse Sync ticks and keeps the original timestamps for USB MIDI. Thanks to Hundekraft for reporting the issue in PR #1.

## Verification

The owner confirmed on an original OP-1 that 1.0.1 restores pulse tempo lock after USB MIDI use. The firmware signature checks and host tests passed. Automatic transport, simultaneous clock sources and CPU load still await hardware testing.

The stock MIDI receiver's two realtime sync-mode queries are intercepted
with checked firmware places. Injected messages carry an origin tag in their
unused realtime padding bytes. The filter returns the stock mode for the
selected source and a mode that skips realtime processing for the other.
The original receive queue contents and unrelated MIDI packets stay available
to the stock parser. The filter preserves its caller's registers and charges
its own additional work to the plugin.

## Build

From this folder, with Blackfin GCC installed and OP1_BFIN_TOOLCHAIN set
to its installation directory, run

    python3 Source/plugin-kit/plugin_tool.py build Source/plugin-kit/plugins/esc.pulsesync --out build/pulsesync.opmod
    python3 Source/plugin-kit/plugin_tool.py show build/pulsesync.opmod
    clang -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -ISource/plugin-kit/sdk Source/plugin-kit/plugins/esc.pulsesync/tests/host.c -o build/pulse-host
    ./build/pulse-host

The module retains id esc.pulsesync and author ESC_CODE.
GPL v3 and original upstream notices are included in LICENSE and NOTICE.md.
