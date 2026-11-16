# Pulse Sync changelog

## 1.1.9

- Use the Opus 2.58 stereo Line In callback for channel-specific clock detection and muting.
- Select L+R (default), L or R. The Shift + Mic label shows the selection, and switching Pulse Sync off restores both inputs.
- Remove experimental automatic channel detection, lookahead and jack latching.

## 1.0.7

- Limit clock input muting to Line In, so other sources can be monitored.
- Show a small blue Pulse Sync label beneath the plug on the Shift + Mic screen when Line In is selected and Mute Clock Input is enabled.

## 1.0.1

- Restore pulse tempo lock after USB MIDI use. Thanks to Hundekraft for reporting the issue in [PR #1](https://github.com/VolkerJunginger/OP-1/pull/1).

## 1.0.0

- First release on main. Supports seven input rates, threshold 1–10, optional automatic transport and USB/Pulse clock priority.
