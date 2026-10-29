# Firmware integration evidence — 2026-10-29

This is a deep plugin tied to original OP-1 R246. Read-only analysis used the owner's stock `op1_246.op1` and Opus `op1_246_opus2.op1` under Downloads/OP1/OS. No firmware was edited or installed. Extracted binary/disassembly scratch stays outside the deliverable. The supplied verification script checks plain LDR bytes without exporting a firmware.

## Input producer

The selected-input meter at **0x01967614** has entry bytes `00e80000` (`LINK 0x0`). Both local firmware files have the same entry and following meter code. At 0x0196762c..30 it loads **0xff800cb4**; the loop at 0x01967664..78 reads 64 signed 16-bit input samples. Related source independently identifies this selected-input buffer and **0xff8010b0** as an input-mode word. The initial interpretation of this word as a reliable resample selector was contradicted by the user's hardware test: line in was selected while this flag was nonzero. Version 0.1.1 reads it only for diagnostics and never gates detection on it.

The manifest uses the kit's **checked entry jump**. Its generated `orig_01967614` trampoline replays `LINK 0x0` and jumps to **0x01967618**. `pulse_input(a,b,c)` forwards the three register arguments, calls the original first, retains its return value, then reads the buffer and charges only its extra detector work with `opus_cpu_charge`. No ADC, source, gain, monitoring, tape or transport state is written. Whether this selected-input routine remains active in every desired UI state is a hardware check.

## Incoming MIDI

- Receive ring **0x01ab0510**, 1024 four-byte USB-MIDI packets.
- Write counter **0x01ab1510**, read counter **0x01ab1514**; free-running counters, slots indexed with `& 0x3ff`.
- Parser **0x019e31a4** loads that queue directly. Entry-to-dispatch path contains no USB-configuration check.
- Wrapper **0x019e3cc4** calls the parser in stock. Opus changes that call at **0x019e3cc8**, consistent with its event/USB handling; the prototype does not patch or call that wrapper.
- The SDK documents **opus_on_block** before the normal MIDI drain, in sequencer-thread context. This plugin uses it to publish ticks for normal processing.
- Packet bytes **0f f8 00 00**, little-endian word **0x0000f80f** (cable 0, single-byte real-time Clock).
- Queue publication disables interrupts only across capacity check, one packet write and write-counter publication, with SSYNC barriers. The reader counter is never changed. At least one slot remains empty. A backlog greater than four is dropped instead of replayed.

The parser's real-time branch at **0x019e3938** accepts selected Sync modes **2** and **4**. The getter at **0x01a5e794** directly returns **[0x018c1870]**. The real-time dispatch table at **0x018e462c** points F8 at **0x019e3988**, FA at 0x019e39d0, FB at 0x019e3a50 and FC at 0x019e3aba. We only insert F8.

F8 adds four to **[0x018b6070]**, the internal pending-tick accumulator, and periodically calls **0x01a5eb30**. That routine calls the clock-history update **0x019de7a4** at 0x01a5eb46; its zero-timestamp path obtains the current timer via 0x01a0dce4. This gives a static path from injected F8 packets to both pending ticks and tempo estimation without emulating USB reception. Actual lock/reacquisition and clock jitter remain untested.

At startup the plugin checks the parser entry and F8-handler byte signatures. The core independently checks the declared input hook before patching it. Refusal on mismatches is intentional. These signatures are compatibility checks, not a complete verification of the firmware.

## Sources

- Opus SDK and kit: https://github.com/LizardMan245/opus/tree/main/plugin-kit
- Opus API timing/rules: https://github.com/LizardMan245/opus/blob/main/plugin-kit/docs/api.md
- Checked jump/trampoline format: https://github.com/LizardMan245/opus/blob/main/plugin-kit/docs/manifest.md
- Related R246 queue layout: https://github.com/pcppcp/op1r-src/blob/main/synth_sdk/devupd/du_fw.c
- Local Hundekraft 3.0.3 source snapshot: `ext/kennel/synth_sdk/kennel/kn_input.c` (input bus/mode), `devupd/du_fw.c` (queue/counters), `dispatcher.c` (MIDI Start/clock-state notes). These provided leads; the hook, selected-input buffer reference, parser, mode getter, F8 dispatch and tempo-history call were checked against local firmware bytes.
- OP-1 input controls: https://teenage.engineering/guides/op-1/original/recording-external-sources
- OP-1 Sync-mode behavior: https://teenage.engineering/guides/op-1/original/tempo

The bundled kit is the previously downloaded local snapshot used for this build; it is not a claim to track upstream main automatically. The compiler is Blackfin GCC 13.3.0. No emulator or physical OP-1 run was performed in this chat.

## 0.1.1 correction and diagnostics

Hardware feedback on 0.1.0: the photo shows Pulse Sync loaded with 763 detected pulses, 762 queued packets, zero dropped, peak 12530 and input blocks 68519; the panel labels the current state INPUT RESAMPLE. The user confirms line in is selected, incoming pulses are visible, counters appear static and tempo does not change. This establishes a false mode label/block in the prototype, not successful tempo tracking.

0.1.1 removes that input-mode gate. It keeps the same checked producer hook and queue injection, and adds read-only diagnostics:

- sequence callback count (`seq`), and write-minus-read queue depth (`q`).
- stock F8 phase counter (`rx`) at 0x018b6010, verified from P4=0x018b600c plus four in the F8 handler. It cycles 0..23, so periodic screen refreshes may alias it; one static rx number alone is inconclusive.
- FOLLOW at 0x018c1898, FALLBACK at 0x018c1874, matching the stock effective-sync routine and related clock-state source.
- COM/controller mode at 0x018b6074, which the stock parser checks before accepting real-time messages. The plugin reports this state but does not rewrite it.

Only control/diagnostic reads were added. No firmware state except receive queue entries/write counter is written. Internal resampling is no longer automatically excluded: the user must select line in for this prototype. The detector still runs only when the wrapped input routine actually runs. A stopped input or sequence count remains a callback/CPU/routing issue to investigate, not proof that removing the mode gate solves every symptom.


## 0.1.2 dry-input monitor suppression

User report: 0.1.1 sync works, but the incoming clock pulses are audible. This confirms clock following on the user's hardware for that version; dry-input suppression is a new hardware check in 0.1.2.

0.1.2 registers an `opus_on_pre_fx` audio callback and calls the **documented** `opus_skip_dry_input` service when the plugin and its new mute_input setting are on, in selected Sync modes 2/4. The callback does not read or modify its sound buffer, the captured input, tape state or input enable/gain settings. The original producer hook and MIDI queue path are retained.

The SDK defines the service as skipping stock dry input while keeping capture available. In the owner's local Opus image the service export resolves to 0x01e192e0: it sets the core's skip-request flag. The core consumes/clears that flag at 0x01e0c028 and clears its caller-local dry-monitor flag at SP+0x34. These are read-only analysis observations; the plugin imports the API name and never hardcodes those core addresses. Core flag consumption is block-scheduled, so a previously issued request may affect a transition block after muting is switched off. Silence/routing over all modes and startup transitions is not established by this static trace.

New setting `mute_input off,on on "mute clock input"` is packed into bit 18 of the atomic config word. The detector-reset comparison excludes that bit: toggling monitoring must not interrupt clock-edge state. Calls to the service are inside the audio callback, never the sequencer/display/setting handlers. All normal CPU protection remains.


## Owner hardware confirmation — 2026-10-29

After installing version 0.1.2 the owner confirmed that both clock sync and dry-input muting work. Earlier statements above about those checks being pending describe the development history. Hardware CPU cycles, jitter and exhaustive page/routing/coexistence checks were not measured or established by that report.


## 0.1.3 identity update

ID is now esc.pulsesync; author ESC_CODE. Uppercase IDs are rejected by the Opus kit. The complete executable image, imports, relocations, entry, memory and hook layout match 0.1.2 exactly. Only manifest metadata changes; the new id uses a separate settings folder.
