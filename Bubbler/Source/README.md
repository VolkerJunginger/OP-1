# Build and verify Bubbler

`plugin` contains the release source and generated native data. `baseline` contains the untouched DSP/codec from the owner-approved 0.5.0 audio build. `upstream` retains the original SoundHack source and licence. `plugin-kit` is the pinned project kit; its SOURCE.json records revision 81968daa84f865886526de9bf7cc86725ae493a1.

## Native build

Set `OP1_BFIN_TOOLCHAIN` to a Blackfin toolchain installation containing `bin/bfin-elf-gcc`, then run:

```sh
./build.sh
```

The release was built with Blackfin GCC 13.3.0 and the included BF524 options. The script regenerates `plugin/tables.h` and `plugin/bubbler_art_data.h`, builds `build/bubbler.opmod` and reads it back. No firmware is required for building.

## Checks

```sh
./check.sh
./compare.sh
python3 tests/verify_release_image.py build/bubbler.opmod
```

`check.sh` uses Clang with AddressSanitizer/UndefinedBehaviorSanitizer for DSP bounds, parameter packing, UI callbacks, MIDI/resonance and interpolation kernels. It writes temporary binaries and UI snapshots to `.test-build`.

`compare.sh` requires Python with NumPy (`PYTHON_BIN` can select the interpreter). It compares the release against the approved 0.5.0 audio over changing controls and settled mono/stereo fixtures. `./compare.sh --benchmark` additionally runs paired host timings; no new performance work is part of this release. Shared-library commands target macOS; adapt library flags/names for other hosts.

`verify_release_image.py` compares the native executable image and loading metadata against the frozen 0.5.0 fingerprint. Exact native identity requires the recorded compiler and pinned kit; another compiler can produce different instructions even if its audio regression passes.

The optional `tests/verify_note_hook.py OWNER_FIRMWARE.op1` checks the firmware-246 note-on entry without distributing firmware. See [DESIGN.md](DESIGN.md) for DSP and integration details.

## Artwork previews

After `check.sh`, run `tests/render_design.py` and then `tests/raster_design.cjs`. These use the installed `op1-design` skill, fontTools and Sharp. Set `OP1_DESIGN_SKILL`, `NODE_BIN` and `NODE_PATH` as needed. Generated previews go under `build/design`. The cached open-font outlines retain their OFL licence and kit asset signatures. Host readout placement is approximate.

Generated build outputs, caches and experiment files are excluded from the release source.
