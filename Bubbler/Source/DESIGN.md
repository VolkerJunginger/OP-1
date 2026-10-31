# Frozen release design

The optimized DSP implementation and generated lookup tables are unchanged from the approved 0.5.0 build. Source cleanup preserves every non-comment C token in the adapter, artwork, MIDI observer, codec and headers; the native image fingerprint verifies that compiler output also remains identical.

## Audio

Eight fixed-point grains read a 131,072-sample mono ring. Integer playback uses direct reads; fractional playback uses Catmull–Rom interpolation. At half speed, paired integer/half-integer samples evaluate the exact original cubic weights. Smooth grain envelopes and the feedback soft-clipping curve use lookup tables.

Zero resonance uses the existing one-pole feedback filter. Raised resonance uses precomputed Q28 coefficients from the original SoundHack second-order formula. Mono/stereo and settled/ramping paths preserve the same integer rounding. The 50% stereo Mix path is an exact mean.

Audio callbacks allocate nothing, touch no files and perform no drawing or floating-point calculation. The optional clipping table requests 514 bytes of fast memory after effect scratch; failure retains the ordinary-memory table with identical audio. Existing grain tables and scratch retain priority.

## Controls and storage

Native word 2 always controls Density. Words 4–7 pack Time Var, Filter Freq, Reverse, Pitch Var probability, Size, Size Var, Start Var and Reso. Current-format sounds from 0.4.1–0.5.0 are retained; no old-format migration code is present.

Orange push selects a plugin-wide base transpose. Key mode observes accepted positive-velocity notes relative to MIDI 60, then always continues stock note processing. Each grain independently receives a down/up octave with the Pitch Var probability. Extreme MIDI rates shorten grains to keep complete trajectories inside the ring.

The firmware-246 VoiceCtl entry at 0x01a85750 is integrated through the kit’s checked jump/original trampoline. Expected bytes are 00e80400. It is not a firmware or boot update. Other plugins holding that entry can prevent loading.

## Screen

Native vector drawing follows only Time, Feedback, Density and Mix. Shift pages change readouts without changing the illustration. The page-3 Pitch Var value and orange-push mode have dedicated native text slots; Key mode shows only the original keyboard outline. No readout-clearing rectangle or cached-glyph replay is used.

A bounded neutral-line fingerprint removes the unwanted stock plot baseline. It leaves other band changes intact. The existing owner-scoped frame token prevents controls and drawing from carrying into another effect’s page.
