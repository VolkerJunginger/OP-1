# Bubbler 1.0.0
### ESC_CODE · Original OP-1 / Opus

Bubbler is a mono granular delay. It reads pieces of recorded sound from a delay buffer, gives them smooth envelopes and overlaps them into echoes, clouds and reversed textures. The master slot retains the stereo dry signal.

## Controls

| Encoder | Page 1 — normal | Page 2 — hold Shift | Page 3 — Shift + right |
|---|---|---|---|
| Blue | Time | Time Var | Filter Freq |
| Green | Feedback | Start Var | Reso |
| White | Density | Size | Reverse |
| Orange | Mix | Size Var | Pitch Var |

Hold **Shift** and press **right/left** to move between pages 2 and 3. Release Shift for page 1. Hold Shift again to reopen the last extra page. The counter shows **1/3, 2/3 or 3/3**.

### Page 1

- **Time:** base delay, **30 ms–1 second**.
- **Feedback:** returns processed sound to the delay, **0–95%**. Higher values create longer repeats.
- **Density:** grain creation rate, **0–100%**, with up to **eight active grains**. At zero, existing grains finish and new grains stop.
- **Mix:** **0% dry to 100% wet**.

### Page 2

- **Time Var:** varies each grain’s delay position. Increase it to scatter the echoes.
- **Start Var:** varies when grains begin. Low values give regular timing; higher values make a looser texture.
- **Size:** nominal grain length, **2–50% of Time**. Short grains make fine textures; longer grains preserve more recognizable sound.
- **Size Var:** varies grain length around the Size setting.

Time Var, Start Var and Size Var run from **0–100%**.

### Page 3

- **Filter Freq:** feedback low-pass cutoff, approximately **100 Hz–19 kHz**. Turn down for darker repeats.
- **Reso:** filter resonance, **0–100%**. Higher values emphasize the cutoff and can ring with feedback.
- **Reverse:** probability of backwards playback: **0% none, 100% all**.
- **Pitch Var:** probability of adding a random octave to each new grain. At **0%**, no octave is added. At **100%**, every grain receives either **−12 or +12 semitones**. Intermediate values mix unchanged and octave-shifted grains.

## Orange push — base pitch

On **page 3**, keep **Shift** held and **push orange** to cycle:

**Off → Key → −12 → −7 → +7 → +12**

This sets each new grain’s base transposition. Its value appears below Pitch Var. **Key mode shows only a keyboard icon** and follows the latest MIDI or OP-1 keyboard note. **MIDI 60 (C4) is unison**; releasing a note leaves its pitch selected.

Pitch Var adds its octave on top of the base pitch. For example, **+7** with full variation produces **−5 or +19 semitones**. Very high MIDI transpositions may use shorter grains.

Base pitch applies to all active Bubbler instances and **starts Off on every restart**. The encoder settings are stored with each sound.

## The picture

The boy brightens with Mix and always stays visible. More Feedback adds green bubbles; Time changes the blue wand; Density brings in white glints and sparkles. The illustration follows **only page 1**—turning page 2 or 3 controls does not change it.

## CPU and scope

The owner reports approximately **3% CPU at default settings** and **5% at the observed maximum** on the OP-1. These are diagnostics readings, not a guarantee for every setup.

This version uses the current Bubbler parameter format. Sounds from **0.4.1–0.5.0** keep their settings. For earlier builds, select another effect and then Bubbler to load fresh defaults. The original desktop plugin’s 10-second range, time lock, pitch-button chord selection and just intonation are not included.
