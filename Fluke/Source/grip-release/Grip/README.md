# GRIP 1.0.4

Compressor for synth, drum and master FX on the original OP-1, using Opus.

| FX location | Kick SC (white push) | HOLD (orange push) |
| --- | --- | --- |
| Synth, FX 1 and FX 2 | Hidden; button inactive | Hidden; button inactive |
| Drum, FX 1 and FX 2 | Available when HITS is selected | Available when HITS is installed and FLUKE Drum is active |
| Master | Hidden; button inactive | Available when HITS is installed and FLUKE Drum is active |

HITS provides the kick detector signal. The sidechain switch is shared for the session and starts off on boot. HOLD controls the FLUKE drum sequencer through HITS.

The main encoders set Threshold, Ratio, Attack and Release. Shift exposes Knee, Range, Makeup and Mix. Continuous encoder adjustments accelerate with faster turns.

GRIP loads independently. Its plugin ID is esc.grip and its effect registration is grip. Replace the previous grip.opmod and restart the OP-1.

## Verification

Blackfin build, module inspection, embedded SVG validation and the existing control tests with ASan/UBSan passed. Tests cover hidden Synth controls and rejected Synth button actions, Drum sidechain and HOLD, and Master HOLD. Hardware verification of the 1.0.4 change is pending.

## Source and licence

Keep Grip beside Hits when building because it includes the shared link and encoder headers. Use the repository SDK and plugin_tool.py. The source is covered by the included GPL-3.0 licence.
