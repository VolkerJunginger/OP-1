# Build and verification

Release date: 2026-11-18. Bubbler 1.5.2, Delayed 1.0.1, Pitch Delay 1.0.1.

This release adds encoder acceleration to continuous controls on all three devices. Bubbler also restores Shift + Left/Right page navigation. Each plugin includes its own freestanding memset implementation. Import inspection confirms that no C-library memset or memcpy is requested from Opus.

Use the Opus plugin kit pinned in SDK-BASELINE.json (commit e47b95fce500c470447eae1ca31a638c67b6dea4, core baseline 2.33), with a Blackfin compiler selected through OP1_BFIN_TOOLCHAIN. The native page interface requires core 2.31 or later.

Build each Source/DEVICE/plugin with `python3 /path/to/plugin-kit/plugin_tool.py build Source/DEVICE/plugin`. Inspect each output using `show`, and validate its screen.svg using `svg`.

Run `python3 Source/test.py /path/to/plugin-kit` for the desktop tests. They use clang with AddressSanitizer and UndefinedBehaviorSanitizer and model native instances and page callbacks. They cover independent same-type controls and audio/history, page changes, encoder pushes, drawing boundaries, reselection defaults, dry master and Bubbler pitch-mode packing. Delayed also passed Smooth LFO transition checks, 1.536 million samples of Smooth Off reference equivalence, and 2.56 million stress samples.

Desktop tests do not emulate the complete firmware, its second-slot manager or the hardware display/audio timing. The maintainer confirmed a hardware test of Bubbler 1.5.2. Hardware verification of Delayed and Pitch Delay 1.0.1 is pending.

The attached illustrated PDF is redistributed byte-for-byte. Source notices, original SoundHack terms and GPL licence are included.
