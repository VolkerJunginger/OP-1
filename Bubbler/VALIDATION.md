# Release verification — 1.0.0

## Owner hardware record — 2026-10-31

The owner reported Bubbler 0.5.0 at approximately **3% CPU with default settings**, reaching **5% at the observed maximum**, and selected it as the release. These are diagnostics percentages; exact worst audio cycles and a complete settings/routing matrix were not supplied.

The 1.0.0 native executable image, zero-fill size, entry point, imports, relocations, exports and firmware-hook data match 0.5.0 exactly. The release manifest records the owner’s 0.5.0 result precisely; it does not claim a separate exhaustive 1.0.0 hardware run. CPU metadata remains `unmeasured` because a percentage is not an exact cycle count.

## Desktop checks

- Blackfin BF524 build passes with warnings as errors; release metadata reads esc.bubbler / ESC_CODE / 1.0.0.
- Native image identity against the approved build is recorded in `validation/native-image.json`; a reproduction script is included.
- Sanitized DSP checks cover dirty-buffer silence, resets, ring wraps, all grains, mono/stereo, control extremes and all eight fast/fallback table/scratch configurations.
- Parameter codec: 100,000 round trips. UI tests cover page memory, scoped arrows/pushes, paired release, startup Off, frozen illustration on Shift pages and readout-safe baseline repair.
- MIDI tests cover all 128 notes, forward/reverse trajectory bounds, six base modes and 0/50/100% random-octave probability. Resonance tests cover all 8,192 cutoff/resonance pairs.
- Interpolation kernels: 12,000 boundary/phase/stride comparisons against the original cubic calculation.
- Audio regression against the frozen 0.5.0 reference: 3,200,000 dynamically controlled samples and 20 settled fixtures, with exact output equality.
- All 15 callback-generated UI states match the approved build. Existing host previews use open Iosevka; they remain approximate native-wrapper illustrations.

The observed 5% peak is not an exhaustive upper bound for every plugin stack, routing, MIDI sequence or future setup. No further optimization or feature changes are included in this release.
