# Fluke 1.5.3 — Melody + Drum variation

FLUKE 2.0.1 for original OP-1 firmware 246 and Opus core 2.57 or newer. See the [device README](../../../README.md) for downloads and installation. This version adds encoder acceleration to continuous adjustments.

## New in 1.5.3

Hold enabled by tape Autostart is now tied to tape transport. When tape playback/recording stops, the automatically held Melody or Drum sequence stops, Hold turns off and sounding notes are released. Recording-to-playback transitions do not stop it. A manually enabled Hold is not claimed by Autostart; toggling Hold off and back on explicitly makes it manual. Clock resets preserve whether Hold was automatic. Switching owners, reloading and changing Drum keyboard behavior clear stale automatic ownership.

## New in 1.5.2

Drum New changes the pattern and generated parameters without starting playback or enabling Hold. Running sequences continue, and their existing Hold state is preserved. This applies in ALL and Edit; Melody New is unchanged.

A shared Opus **Autostart** setting applies in both modes:

- **Play & Rec** (shown as PLAY+REC): tape playback or recording enables Hold and starts the selected Fluke sequencer.
- **Rec** (default): tape recording enables Hold; ordinary playback does not.
- **Off**: no automatic Hold. Manual Hold and its existing Play synchronization remain available.

The actual tape state is read through the pinned SDK tempo service, so arming Record alone does not trigger playback. Playback starts on the host grid; a running sequence is not retriggered just because recording begins. Changing the setting does not start a transport already in progress. Selecting/reloading Fluke still clears Hold and starts with fresh defaults.

The Pages manual now includes consistent annotations on the original supplied OP-1 picture and a reference sheet of desktop-rendered Fluke screen states. No new PDF was exported.

## Install

Replace fluke.opmod in the OP-1 mods folder, eject and restart. Install only one Fluke build at a time: this variation uses the same esc.fluke ID. Choose Fluke as a sequencer in Synth for Melody or Drum for six-lane rhythm sequencing.

## Drum selection and editing

The display shows the OP-1 keyboard: 24 keys, two octaves beginning on F. Hold Shift and press a note to add it and edit its sequence. Up to six notes can be selected. A seventh is ignored. Press Shift plus the edited note again to return to ALL; Shift plus another selected note edits that lane. Selection gestures do not trigger playback.

On a fresh load, Drum starts with generated lanes for **F3, G3 and C#4**, in ALL mode. Enable Hold to begin; ordinary keys in ALL transpose/play by default, or audition in Preview mode. Adding a lane never starts playback. New preserves the existing stopped/running and Hold states in ALL and Edit.

The white key faces use the same warm-white color as the top-right root/scale indicator. Matching green (#08CC83) fills selected white keys; selected black keys fill the same green. Only selected black keys have thin black outlines. Valid keys in the Melody global-key diagram use the same treatment. The bottom Scale caption stays white. The edited key is red. The header says ALL or EDIT followed by the note. Editing one lane keeps the others playing. All selected lane rows, note dots and square corner markers remain visible in both ALL and Edit. The currently playing note is a filled green square with a layered glow in both Drum and Melody. It replaces that note’s dot and corner brackets. Melody also shows its green line above the current step; Drum has no green line. Silent steps remain white. Save/recall feedback keeps these markers monochrome.

**Remove a note: first enter its Edit mode, then hold Cut and press that same note (no Shift needed).** The lane is deleted and the view returns to ALL. A different note is ignored while Cut is held; ALL does not delete lanes.

With Drum keys set to Preview, ordinary keyboard presses in Drum ALL play the loaded drum sounds directly for prelistening. They do not select lanes, change patterns, or start/stop the sequencer. Use Orange/Hold for playback. Shift + note still selects a lane. In Edit, ordinary keys retain their sequence start/sustain behavior. Orange press toggles Hold independently from Melody. Switching instruments stops the outgoing sequence and clears its Hold flag, so returning shows Hold Off. This instrument-change stop preserves the current patterns; selecting a different sequencer and reloading Fluke still resets defaults. Direct keyboard audition is handled by the host and is separate from the sequencer’s four-note limit. Preview notes are not latched in plugin key tracking, so every previewed key remains available for Shift selection even when the host handles its release itself.

## Controls

| View | Blue | Green | White | Orange |
| --- | --- | --- | --- | --- |
| ALL Main turn | Length | Density | Rotate | Global Swing |
| ALL Main press | New all | No function | No function | Hold |
| Edit Main turn | Length | Density / Pulses | Rotate | Rate |
| Edit Main press | New lane | Density/Pulses toggle | No function | Hold |
| Edit hold Shift, turn | Random | Style | Direction | Lane Swing |

In Edit, turns affect only the edited lane. In ALL, Length, Density and Rotate change each lane relative to its existing value. Fractions accumulate between detents. Four clockwise Density detents change values 4 and 12 to 5 and 15; two Rotate detents move an 8-step lane once and a 16-step lane twice. Limits apply independently.

ALL shows rounded numeric averages for Length, Density and Rotate instead of MIX. These describe the lanes, rather than one common absolute setting. ALL Density turns select Density mode for every lane; Pulses remains available individually in Edit. Green press has no function in ALL. ALL has one page only. Holding Shift keeps the same Length, Density, Rotate and Swing controls, values, New and Hold visible. Shifted encoder turns use those same functions. Only Blue/New and Orange/Hold are press actions; Green and White presses have no function. Edit retains its Shift page.

Global Swing is a single 0–100 value controlled by Orange in ALL, with or without Shift. It adds to each lane's individual Swing, with the result limited to 100. It does not change lane Rates. A newly added lane begins with zero individual Swing, so the global value is not applied twice. The complete save includes Global Swing.

New regenerates the complete settings of one lane in Edit, or every selected lane in ALL: Length, Density, Rate, Rotate, Style, Direction, lane Swing, Random, rhythm mode and Pulses. Assigned drum notes and the global Swing stay unchanged. New in ALL chooses a shared groove location and Style. The original Styles retain related Length families (8, 12 or 16, with occasional half-length lanes), beat-aligned rotation and mainly forward Direction. The six added families start with 16 steps at 1/16, Forward and zero rotation, keeping the musical beat locations recognizable. Length, Rate, Direction and Rotate remain freely editable afterward. Rates are related subdivisions: 1/32, 1/16, 1/8 and 1/4. Lane Density depends on its rhythmic role; lane Swing varies from 0–25 (50–70 for newly generated Shuffle lanes) and Random from 0–35. Rotate and Pulses respect the new Length. New in Edit varies only that lane around the existing shared groove. The added families keep their basic bar framework; the original Styles also allow local rotation and Direction variations. Individual parameters may occasionally repeat by chance. Other lanes remain untouched in Edit. Existing saved patterns keep their stored rankings; use New to generate patterns with the stronger Style weighting. Future selected lanes inherit the ALL template settings and receive their own pattern seed.

## Added musical families

These additions use general rhythmic relationships observed while reviewing René-Pierre Bardet’s *260 Drum Machine Patterns*. They are original role-aware generation rules, not transcriptions of numbered examples or a trained model. No book pages or extracted pattern collection are included.

| Style | Generative character |
| --- | --- |
| Rock | Stable backbeat, steady timekeeping, variation around the foundation part. |
| Funk | Syncopated foundation, snare details and interlocking percussion. |
| Disco | Quarter-note foundation, backbeat and complementary offbeat cymbal roles. |
| Reggae | More space, a third-beat foundation and offbeat support. |
| Latin | Anticipations and layered counter-rhythms around a stable pulse; broadly Latin-inspired. |
| Shuffle | Paired eighth-note pulse with Swing-controlled lilt. |

New keeps the new families’ bar framework intact so their character survives parameter generation. Disco’s timekeeper and offbeat role begin with complementary activity. Reggae starts with fewer foundation/backbeat hits. Candidate selection additionally discourages unnecessary overlap between timekeeper and offbeat parts, while the four-note runtime cap remains in place. At high Density, extra details can intentionally move beyond the simplest genre skeleton.

The six roles describe sequencing functions, not detection of which OP-1 samples are loaded. A foundation lane will only sound like a kick if its selected note plays a kick. The new Styles are available on Shift + Green in Drum Edit; ALL keeps the same single-page controls. Melody Styles are unchanged.

## Style-dependent Drum timing

The existing Drum Style also chooses a repeatable timing template. The combined lane Swing plus global Swing scales it; the sum is limited to 100. At zero combined Swing, every onset stays exactly on its step boundary. A global Swing of 0 can still leave individual lane Swing active: set that lane’s Swing to 0 as well for exact timing.

The template adds small pushes or delays to the familiar alternate-step Swing. Backbeat parts can sit slightly behind steady foundation anchors; Offbeat hats can anticipate the beat. Tresillo, Clave, Break, Fill and Random use different fixed timing shapes. These are musical roles, not automatic detection of the loaded samples. There are no new controls and no timing jitter.

Style-specific micro-offsets are limited to ±3 host ticks (about ±15.6 ms at 120 BPM), and scale with Swing and step duration. The ordinary alternate-step Swing remains available over its existing range. Shuffle instead delays the offbeat eighth notes: a combined Swing near 66 produces a roughly 2:1 eighth-note ratio. Its delay is bounded below the following step; zero Swing remains exact. Quarter-note foundation anchors and visible step 1 stay exact. Very small amounts may round to zero at 96 PPQN.

Early hits use one-step lookahead. They do not move the host clock or accumulate timing drift. At startup, an otherwise early first event is clipped to the starting boundary because it has no prior time to play in. Cut and recall still restart at the synchronized boundary. The runtime four-note cap also applies to early and delayed events.

This Drum timing enhancement draws on the role/context-dependent microtiming and groove-template ideas discussed by Christian Cvachovec in *Mikrorhythmus in der digitalen Musikproduktion* (2012), sections 2.3 and 3.1.3. Fluke uses original timing templates; no document assets or code are included. Melody’s existing Swing behavior is unchanged.

## Choosing a new pattern

New now prepares eight alternatives and selects the highest-scoring candidate. Each alternative includes the complete lane parameters described above. In ALL this evaluates complete kits; in Edit only the chosen lane changes, evaluated alongside the other current lanes.

The lightweight selection heuristics consider:

- How much the base rhythm changes from the current pattern, with foundation and backbeat changes weighted more heavily than hats.
- Excessive activity in supporting parts, unusually empty parts and nearly identical lanes.
- More than four simultaneous base onsets, before the runtime four-note limit is applied.
- More freedom to depart from the current pattern for a fill role.

The comparison evaluates a bar at the exact 96-PPQN host tick resolution, including Rate, Direction, Rotate, combined Swing and Style-dependent timing offsets. Collision penalties consider both simultaneous onsets and overlapping half-step notes, including retriggers. It evaluates base hits; the evolving Random phrase detail is applied during playback. All eight proposals are considered rather than imposing a retry loop. The best available candidate is chosen even if some penalties remain. The runtime four-note limit remains authoritative.

These are original Fluke heuristics inspired by the seed-relative, role-weighted variation approach discussed in Richard Vogl’s 2018 thesis, *Deep Learning Methods for Drum Transcription and Drum Pattern Generation*, chapter 12:
https://epub.jku.at/obvulihs/download/pdf/3294852#page=150

No trained neural model, thesis code or training dataset is included. Candidate scores express these design preferences, not a measured guarantee that a rhythm sounds better. Listening on the OP-1 is still needed. The work runs only when New is pressed; sequencer ticks do not generate or score candidates.

## Groove balance and interlocking parts

New’s existing eight-candidate search now evaluates three additional qualities without adding controls:

- **Balance:** the Style’s small timing pushes or delays are assessed separately from ordinary long/short Swing. Foundation anchors are kept tight, while a backbeat can sit behind the pulse and an Offbeat hat can anticipate it.
- **Timing intensity:** the scorer measures the total size of onset displacement, independently of whether that displacement is early or late. Role- and Style-dependent ranges discourage excessive distortion of supporting parts while allowing fills more freedom.
- **Relationships:** timekeeper, offbeat and percussion parts are evaluated together. The scorer favors distinct contributions, a few common reference points and complementary emphasis across the phrase. Nearly duplicate supporting parts are discouraged. Foundation/backbeat unisons can remain structural, including Reggae’s one-drop.

These are soft preferences for choosing among the eight candidates, not hard rules for every pattern. Density, Random and the existing parameter generation still work. The first candidate is not automatically accepted. Edit still changes only one lane, assessed alongside the others. Scores are invariant to the order of a pair of lanes.

The added metrics run only for New. They do not change playback timing, the cyclic host-clock schedule, zero-Swing exactness, Style selection, saved patterns or the four-note cap. Rhythmic connections tolerate small timing differences. As before, candidates are assessed over a one-bar window; this is not a complete model of every longer polyrhythmic cycle.

The design is informed by Malcolm Braff’s discussions of groove balance, potential energy and interacting cyclic layers:

- https://general-theory-of-rhythm.org/diversity-of-grooves-its-all-about-da-weight/
- https://general-theory-of-rhythm.org/salsa-is-a-major-chord/
- https://general-theory-of-rhythm.org/113/

Fluke uses original bounded heuristics, not a reproduction of the full theory. “Intensity” here means onset displacement in host-clock ticks, not physical energy. The salsa analogy does not turn Latin patterns into literal Euclidean 3:5 patterns. Listening tests remain necessary to assess musical improvement.

## Drum rhythms

The six lanes share one groove shape, with separate foundation, backbeat, hat, offbeat, percussion and fill roles. These are rhythm roles, not automatic recognition of the loaded drum samples: any chosen OP-1 note can play any sound. The first three startup lanes use foundation, backbeat and hat roles. Added notes take an available role; deleting a lane retains the roles of the remaining lanes.

Density reveals progressively weaker steps around that role’s strongest beats. Increasing it adds detail while retaining existing base hits. The current Style has enough influence to produce distinct distributions around that backbone; Random Style permits wider seeded placement variation. There are now 14 Drum Styles. The original eight remain in the same selector positions: Straight, Backbeat, Offbeat, Tresillo, Clave, Break, Fill and Random. Six added families follow them: Rock, Funk, Disco, Reggae, Latin and Shuffle (displayed SHUFL). All screen labels fit within six characters. Each emphasizes different step positions. Pulses distributes the chosen number of hits evenly across the lane length; New can change its starting phase. Style strongly changes Density placement, alongside the lane’s role and shared groove. In Pulses mode, Style selects a different starting phase while keeping the chosen pulse count and even spacing. With zero or full pulses, phase shifts naturally make no difference. Swing-related Style timing still applies when Swing is nonzero.

Random, from 0 to 100, chooses an extra density amount once at the start of each lane traversal. That amount stays fixed for the phrase, adding the next strongest steps in its backbone. The next traversal can choose a different amount. It does not toss a new coin on every step. At 0 the base rhythm is unchanged; at 100 every rest receives a hit. Use 0 for an exact Euclidean pulse count. It does not permanently write extra hits into the pattern.

Playback allows at most four active sequencer notes and four note-on events per clock tick. When six lanes collide, excess hits are skipped and priority rotates between busy moments so every lane gets opportunities to play. The saved/base rhythm remains intact. Drum sample tails depend on the OP-1 drum engine; the limit controls sequencer note events.

Each lane has its own Length, Rate, Swing and Direction. Directions are Forward, Reverse, Forward/Reverse, Jump (odd positions then even positions), and Reverse Jump. Rotate affects the visible steps; shortening a lane preserves hidden notes for later expansion. Drum mode has no scale or melodic Range.

## Shared groove design

The drum engine learns from Grids’ shared-map and step-strength approach. Fluke uses four original six-role groove sketches, smoothly blended using a shared internal X/Y position. New in ALL chooses that position; there are no additional controls. Each lane has its own ranked step strengths, seed details and settings. Ordinary playback uses these prepared ranks; only small phrase-detail choices occur during clock ticks.

This is an original Fluke implementation inspired by the design principle, not a Grids port. No Grids source code or original pattern tables are included. The sketches are not trained on recorded drum loops. See the original design references:

- https://github.com/pichenettes/eurorack/blob/master/grids/pattern_generator.cc
- https://pichenettes.github.io/mutable-instruments-documentation/modules/grids/manual/

Melody snapshots keep their file layout. Drum saves now use format 2 to include all 14 ranking tables. Older eight-Style format-1 Drum saves are read and converted in memory, preserving every original parameter, note assignment and first eight ranking tables. Loading does not rewrite the files; Lift writes the expanded format. Older Fluke builds cannot read the new format-2 saves. Recalling older drum patterns preserves their saved ranks and settings. New in ALL creates a shared groove and resolves any duplicate role assignments inherited from older seeds.

## Hold and OP-1 Play synchronization

Held playback now uses the host callback’s phase instead of counting from the instant Hold was pressed. Enabling Hold joins the next boundary for the selected Rate. Melody restarts its phrase there. Drum uses the first selected lane as its start reference, and each lane aligns its own Rate phase to the tracked host clock. Depending on Rate, the first note may wait briefly for its boundary.

The plugin observes OP-1 Play before and after the stock handler without consuming it, including when another page is showing. With Hold On, Play restarts the held pattern on the host grid. Play with Hold Off still establishes the clock origin for a later held start. MIDI Start also resynchronizes playback. Held New/add-lane starts use the same grid alignment. Existing microtiming templates, Cut/recall boundary behavior, and the four-note cap remain.

For a known common starting point, enable Hold and press OP-1 Play. The SDK reports a repeating 0–47 position at 96 PPQN; Fluke tracks its wraps. If Fluke is first selected partway through playback without observing a start, that short position alone cannot reveal an absolute bar count for every slow division. Pressing Play or receiving MIDI Start establishes the shared origin. Hardware transport timing still needs OP-1 verification.

Switching instruments still stops the outgoing sequence and clears Hold. This update does not take over the stock transport keys or add controls. The warm-white keyboard faces still match the top-right root/scale indicator; original bright-green selections and red Edit are retained.

## Save, recall and restart

In ALL, Lift captures every selected lane, its settings and note assignment. Drop restores that saved drum pattern, retaining the edited note if it is present. Drum and Melody have separate saves. Lift and Drop also work in Edit and still capture/restore the complete drum pattern. Cut alone does not jump in Edit; Cut + the edited note removes its lane. SAVED and RECALLED briefly show a monochrome overlay.

Cut in ALL queues a jump to visible step 1 at the next step boundary of the first selected lane. Recall while running uses that lane's current boundary too, then restarts all recalled lanes together at step 1. Host clock timing continues. Since lanes may use different rates, this reference boundary gives one definite synchronized restart. Recall while stopped remains stopped. Current Hold and held-key state are preserved.

Selecting Fluke again resets the selected mode to defaults. Melody gets a fresh pattern, default parameters and no global tonic. Drum gets fresh F3, G3 and C#4 lanes in ALL, with default parameters. Both start stopped with Hold off. Edit selection, transient key gestures, pending jumps/recalls and feedback overlays are cleared. Each mode remains separate. Removing/reloading the device or rebooting starts with defaults: a new Melody and generated F3, G3 and C#4 Drum lanes, stopped in ALL mode. A saved pattern is loaded into the recall cache and only restored when Drop is pressed.

## Melody

Main: Blue Length/New, Green Density/Pulses, White Rotate/Scale, Orange Rate/Hold. Hold Shift: Blue Range, Green Style, White Direction, Orange Swing. Range runs from -3 to +3, default +1. Styles: Scale, Chord, Fifth, Root, Triad, Walk, Leap, Motif, Reply, Anchor.

Play the keyboard to set the root. With Global Key enabled in Opus settings (default on), Shift + note sets a global tonic for the selected scale. For example, choose Minor and Shift + C for C-MINOR. All generated notes then stay within that key. Root notes outside the key are ignored for transposition but still sustain playback while held. Shift + the exact global tonic in any octave clears the lock; another tonic changes it. The keyboard diagram replaces the page indicator while a global key is set. This setting affects Melody only.

Lift saves Melody and Drop recalls it; Cut queues a jump to step 1 on a clock boundary. Saves do not replace the current global tonic. Reloading initializes a fresh default Melody; Drop explicitly restores a saved one.

## Verification and source

Native Blackfin build, SVG validation and host tests with AddressSanitizer/UndefinedBehaviorSanitizer passed. Tests include six-lane transport, weighted edits, revised ALL/Edit controls, global Swing timing, startup lanes, Euclidean spacing, save isolation and recovery, mode switching, note cleanup and existing Melody/global-key behavior. These are desktop results, not physical OP-1 validation. Preview PNGs use approximate desktop lettering, not a captured device screen.

Source and pinned SDK headers are included. See Building.md and NOTICE.md for build instructions, provenance and licenses. The native port uses GPL-3.0; Marbles-derived Melody components retain Emilie Gillet's MIT notice. There is no Sting generator implementation in this version.

## Screen layout in 1.5.0

The lower parameter area is 4% shorter, with that space added to the plot. Notes, squares and lane labels are slightly larger. The top-right label is enlarged; Melody root and scale form one right-aligned group, and Drum ALL / Edit use the same right margin as the sequencer. The logo, keyboard palette and controls are retained.

The header uses the documented native frame/text API for an exact outer right edge. The small inner divider uses optical spacing; its placement and frame ordering require an OP-1 check. Included previews use approximate desktop lettering and are not hardware captures.

## Keyboard palette in 1.5.0

Selected keys use matching green (#08CC83) in both Melody and Drum: large dots on white keys and filled black keys, with black outlines on selected black keys only. White key faces remain warm white; Edit stays red. Playing squares and the Melody play indicator retain their previous colors.

## Caption visibility correction

Encoder-press captions (New, Pulses/Density, Scale and Hold) are raised from y134 to y128 and reduced from size10 to size9 in both color and monochrome views, in Melody and Drum modes. They now end above y139, leaving more clearance from the screen bottom. Keyboard dots and selected black keys retain matching green fills. Only selected black keys have fixed black outlines.

Desktop checks passed. Final caption visibility still needs confirmation on the OP-1.

## Caption visibility correction

Encoder-press captions (New, Pulses/Density, Scale and Hold) are raised from y134 to y128 and reduced from size10 to size9 in both color and monochrome views, in Melody and Drum modes. They now end above y139, leaving more clearance from the screen bottom. Keyboard dots and selected black keys retain matching green fills. Only selected black keys have fixed black outlines.

Desktop checks passed. Final caption visibility still needs confirmation on the OP-1.

## Keyboard dots in 1.5.0

Round keyboard dots have green fill only, with no outline. Selected black-key indicators retain their black outlines. Raised press captions from 1.4.2 are retained.

## Full-key display in 1.5.0

Selected white keys fill green across their full face instead of showing a dot. Selected black keys remain green with black outlines; Edit remains red. Black keys stay above the white key selection fills. The upper artwork is 2% taller, with matching Drum marker/row placement. Raised press labels from 1.4.2 are retained. Native header text retains its nearest whole-unit font height.

Desktop checks passed; OP-1 display confirmation remains pending.

## Full-key display in 1.4.4

Selected white keys fill green across their full face instead of showing a dot. Selected black keys remain green with black outlines; Edit remains red. Black keys stay above the white key selection fills. The upper artwork is 2% taller, with matching Drum marker/row placement. Raised press labels from 1.4.2 are retained. Native header text retains its nearest whole-unit font height.

Desktop checks passed; OP-1 display confirmation remains pending.

## Keyboard height in 1.5.0

Only the miniature keyboard is taller: five successive 2% increases from 1.4.4 (1.1041 height factor). Width, green palette, and the rest of the screen are unchanged. Applies in Melody and Drum mode.

## Full-key display in 1.4.4

Selected white keys fill green across their full face instead of showing a dot. Selected black keys remain green with black outlines; Edit remains red. Black keys stay above the white key selection fills. The upper artwork is 2% taller, with matching Drum marker/row placement. Raised press labels from 1.4.2 are retained. Native header text retains its nearest whole-unit font height.

Desktop checks passed; OP-1 display confirmation remains pending.

## Keyboard height in 1.4.5

Only the miniature keyboard is taller: five successive 2% increases from 1.4.4 (1.1041 height factor). Width, green palette, and the rest of the screen are unchanged. Applies in Melody and Drum mode.

## Expanded keyboard in 1.5.0

The approved keyboard uses the maximum height within the current header layout, ending near y30 with the plot beginning at y31. Its width is 1.1576 times 1.4.5; black keys use the approved wider proportions (1.4912 times their original local width). Melody and Drum keys remain centered; the green palette, other artwork, control labels and engine behavior are unchanged. Native hardware legibility remains pending.

## Full-key display in 1.4.4

Selected white keys fill green across their full face instead of showing a dot. Selected black keys remain green with black outlines; Edit remains red. Black keys stay above the white key selection fills. The upper artwork is 2% taller, with matching Drum marker/row placement. Raised press labels from 1.4.2 are retained. Native header text retains its nearest whole-unit font height.

Desktop checks passed; OP-1 display confirmation remains pending.

## Keyboard height in 1.4.5

Only the miniature keyboard is taller: five successive 2% increases from 1.4.4 (1.1041 height factor). Width, green palette, and the rest of the screen are unchanged. Applies in Melody and Drum mode.

## Expanded keyboard in 1.4.6

The approved keyboard uses the maximum height within the current header layout, ending near y30 with the plot beginning at y31. Its width is 1.1576 times 1.4.5; black keys use the approved wider proportions (1.4912 times their original local width). Melody and Drum keys remain centered; the green palette, other artwork, control labels and engine behavior are unchanged. Native hardware legibility remains pending.

## Synth keyboard correction in 1.5.0

Selected black-key indicators in Melody inherited the keyboard transform and also applied it themselves. Removed the duplicate child transforms so selected and unselected keys share the same top edge and width. Both color and monochrome artwork are corrected. Drum artwork is identical to 1.4.6. Verified against the reported photo and XML structure; hardware confirmation of the corrected build remains pending.

## Full-key display in 1.4.4

Selected white keys fill green across their full face instead of showing a dot. Selected black keys remain green with black outlines; Edit remains red. Black keys stay above the white key selection fills. The upper artwork is 2% taller, with matching Drum marker/row placement. Raised press labels from 1.4.2 are retained. Native header text retains its nearest whole-unit font height.

Desktop checks passed; OP-1 display confirmation remains pending.

## Keyboard height in 1.4.5

Only the miniature keyboard is taller: five successive 2% increases from 1.4.4 (1.1041 height factor). Width, green palette, and the rest of the screen are unchanged. Applies in Melody and Drum mode.

## Expanded keyboard in 1.4.6

The approved keyboard uses the maximum height within the current header layout, ending near y30 with the plot beginning at y31. Its width is 1.1576 times 1.4.5; black keys use the approved wider proportions (1.4912 times their original local width). Melody and Drum keys remain centered; the green palette, other artwork, control labels and engine behavior are unchanged. Native hardware legibility remains pending.

## Synth keyboard correction in 1.4.7

Selected black-key indicators in Melody inherited the keyboard transform and also applied it themselves. Removed the duplicate child transforms so selected and unselected keys share the same top edge and width. Both color and monochrome artwork are corrected. Drum artwork is identical to 1.4.6. Verified against the reported photo and XML structure; hardware confirmation of the corrected build remains pending.

## Drum editing in 1.5.0

Lift and Drop work in ALL and Edit, saving or recalling the complete drum pattern in the existing independent Drum save slot. Drop preserves the edited note when present in the saved pattern; otherwise it returns to ALL. Recall remains synchronized. In Edit, active notes in the edited lane are red; other lanes stay blue. Playing notes remain glowing green squares. Cut remains restricted to ALL for jump-to-start.

## Full-key display in 1.4.4

Selected white keys fill green across their full face instead of showing a dot. Selected black keys remain green with black outlines; Edit remains red. Black keys stay above the white key selection fills. The upper artwork is 2% taller, with matching Drum marker/row placement. Raised press labels from 1.4.2 are retained. Native header text retains its nearest whole-unit font height.

Desktop checks passed; OP-1 display confirmation remains pending.

## Keyboard height in 1.4.5

Only the miniature keyboard is taller: five successive 2% increases from 1.4.4 (1.1041 height factor). Width, green palette, and the rest of the screen are unchanged. Applies in Melody and Drum mode.

## Expanded keyboard in 1.4.6

The approved keyboard uses the maximum height within the current header layout, ending near y30 with the plot beginning at y31. Its width is 1.1576 times 1.4.5; black keys use the approved wider proportions (1.4912 times their original local width). Melody and Drum keys remain centered; the green palette, other artwork, control labels and engine behavior are unchanged. Native hardware legibility remains pending.

## Synth keyboard correction in 1.4.7

Selected black-key indicators in Melody inherited the keyboard transform and also applied it themselves. Removed the duplicate child transforms so selected and unselected keys share the same top edge and width. Both color and monochrome artwork are corrected. Drum artwork is identical to 1.4.6. Verified against the reported photo and XML structure; hardware confirmation of the corrected build remains pending.

## Drum editing in 1.4.8

Lift and Drop work in ALL and Edit, saving or recalling the complete drum pattern in the existing independent Drum save slot. Drop preserves the edited note when present in the saved pattern; otherwise it returns to ALL. Recall remains synchronized. In Edit, active notes in the edited lane are red; other lanes stay blue. Playing notes remain glowing green squares. Cut remains restricted to ALL for jump-to-start.

## Edit boxes in 1.5.0

The edited Drum lane now has red corner boxes as well as red note dots. Playing notes stay glowing green squares; other lanes retain their normal colors.

## Full-key display in 1.4.4

Selected white keys fill green across their full face instead of showing a dot. Selected black keys remain green with black outlines; Edit remains red. Black keys stay above the white key selection fills. The upper artwork is 2% taller, with matching Drum marker/row placement. Raised press labels from 1.4.2 are retained. Native header text retains its nearest whole-unit font height.

Desktop checks passed; OP-1 display confirmation remains pending.

## Keyboard height in 1.4.5

Only the miniature keyboard is taller: five successive 2% increases from 1.4.4 (1.1041 height factor). Width, green palette, and the rest of the screen are unchanged. Applies in Melody and Drum mode.

## Expanded keyboard in 1.4.6

The approved keyboard uses the maximum height within the current header layout, ending near y30 with the plot beginning at y31. Its width is 1.1576 times 1.4.5; black keys use the approved wider proportions (1.4912 times their original local width). Melody and Drum keys remain centered; the green palette, other artwork, control labels and engine behavior are unchanged. Native hardware legibility remains pending.

## Synth keyboard correction in 1.4.7

Selected black-key indicators in Melody inherited the keyboard transform and also applied it themselves. Removed the duplicate child transforms so selected and unselected keys share the same top edge and width. Both color and monochrome artwork are corrected. Drum artwork is identical to 1.4.6. Verified against the reported photo and XML structure; hardware confirmation of the corrected build remains pending.

## Drum editing in 1.4.8

Lift and Drop work in ALL and Edit, saving or recalling the complete drum pattern in the existing independent Drum save slot. Drop preserves the edited note when present in the saved pattern; otherwise it returns to ALL. Recall remains synchronized. In Edit, active notes in the edited lane are red; other lanes stay blue. Playing notes remain glowing green squares. Cut remains restricted to ALL for jump-to-start.

## Edit boxes in 1.4.9

The edited Drum lane now has red corner boxes as well as red note dots. Playing notes stay glowing green squares; other lanes retain their normal colors.

## Drum keyboard behavior in 1.5.0

The Opus setting Drum keys offers Transpose (default) and Preview. In Transpose, ordinary keys in ALL start/sustain the drum sequence and shift its notes by the played MIDI note minus C3 (MIDI48). C3 preserves the assigned notes. Releasing all keys stops playback unless Hold is on. While running, key changes affect future note-ons without resetting lane positions or timing. Already sounding notes release at their original pitches. Keys in Edit retain their existing start/sustain behavior. Preview passes ordinary ALL keys to the drum sound as before. Changing this setting stops playback and clears Hold/transpose to avoid retained keys.

Shift+note adds/selects lanes without starting playback or enabling Hold; a new lane still joins an already running sequence on its clock. New leaves playback and Hold unchanged. Reload resets the runtime transpose to C3 while keeping the Opus setting preference. Save files retain their previous format and note assignments.

The original OP-1 guide documents keyboard transposition and C as the original-pitch key for Endless/Pattern: https://teenage.engineering/guides/op-1/original/sequencers . The guide does not specify mid-sequence phase behavior; keeping the playhead unchanged is Fluke behavior requested here. Desktop tests verify it; hardware confirmation remains pending.

## Changes in 1.5.1

This build uses the newly reviewed Opus2 beta4 build6 baseline, with the plugin kit pinned at e47b95fce500c470447eae1ca31a638c67b6dea4. See Opus-Baseline-Review.md for the release/API/tool review and pending hardware checks.

Drum Transpose now behaves like Endless's drum pitch path: it preserves each drum assignment and changes the voice pitch through the stock note-on semitone argument. The first physical C is neutral (the OS note60), higher/lower keys shift pitch relative to it. This replaces 1.5.0's MIDI48 reference and its shifting of drum-key assignments. It does not restart or alter the rhythm; already sounding hits finish normally. Preview and no-add-autostart remain.

New Melody and Drum patterns start Rotate0. Rotate cycles from0 through Length inclusive and then0 (backward from0 reaches Length). At Length16:14,15,16,0,1. Rotate=Length has the same ordering as0 but remains a selectable endpoint. In Drum ALL, reaching the common master0 resets every lane to0. Saved patterns accept this endpoint; earlier builds may reject a saved pattern containing Rotate=Length.

Native build/SVG and21 desktop sanitizer checks passed. Hardware pitch, core loading and display/CPU confirmation remain pending.
