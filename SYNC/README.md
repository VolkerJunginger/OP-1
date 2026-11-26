# SYNC 1.0.1

SYNC is an Opus system mod by **ESC_CODE** for the **original Teenage Engineering OP-1**. It follows USB MIDI Clock, Ableton Link, or an analog pulse. Link Audio sends the OP-1's Master, Tape, Synth, and Drum channels to Ableton Live over USB.

[**Download SYNC 1.0.1 (ZIP)**](https://raw.githubusercontent.com/VolkerJunginger/OP-1/main/SYNC/SYNC-1.0.1.zip) · [**English manual (PDF)**](https://raw.githubusercontent.com/VolkerJunginger/OP-1/main/SYNC/SYNC-1.0.1-Manual-EN.pdf) · [**Corresponding source (ZIP)**](https://raw.githubusercontent.com/VolkerJunginger/OP-1/main/SYNC/SYNC-1.0.1-Source.zip)

## Install

1. Use an original OP-1 running firmware **R246** with **Opus Core 2.68 or later**. Connect the OP-1 to your computer over USB.
2. Enter Disk mode with **Shift + COM**, then **T3**. If you have the standalone Pulse Sync mod, remove it and restart before installing SYNC. Unzip the download and copy `SYNC.opmod` into the OP-1's `mods` folder. Eject the drive and restart if the OP-1 does not restart automatically.
3. Hold **Synth + Drum + Tape + Mixer** to open the Opus menu. Press **T2**, select **SYNC** with the blue encoder, and open its settings. Check that SYNC is loaded.

The Opus mod list shows **1.0.1**. If you are updating, remove the previous SYNC mod and restart before installing the new file.

**What changed in 1.0.1.** The Link Audio USB queue now reserves space for every active subscription before processing an audio block. Host tests exercised all seven channels with five and eight simulated listeners. This build passed testing on the original OP-1.

## Select a clock

| Clock | Clock source | OP-1 transport |
| --- | --- | --- |
| Off | Internal clock | Controlled from the OP-1 |
| MIDI (default) | MIDI Clock over USB | MIDI Start, Continue, and Stop |
| Link | Tempo and phase from the Link session | Shared start and stop with **Link Start Stop** enabled |
| Pulse | Analog pulse via Line In | Start and stop with **AutoStart** enabled |

The OP-1 Tempo screen shows the selected external source between **EXT** and the green **SYNC** box.

### USB MIDI

1. Set **Clock → MIDI** in SYNC.
2. In Live's MIDI settings, enable **Sync** for the MIDI **output** to the OP-1. Start playback in Live.
3. Open the OP-1's Tempo screen and change the tempo in Live to check that the OP-1 follows.

USB MIDI carries notes and CC in every clock mode. In Live, enable **Track** on the OP-1 MIDI input to play Live instruments or record notes. Enable **Track** on the OP-1 MIDI output to send notes to the OP-1.

### Ableton Link

1. Set **Clock → Link** in SYNC and enable **Link** in Live's Control Bar. The OP-1 joins the Link session over USB networking.
2. Change the tempo in Live and check the OP-1's Tempo screen. For shared transport, enable **Link Start Stop** in SYNC and start/stop synchronization in Live.

To switch back to USB MIDI Clock, select **Clock → MIDI**, confirm **Sync** is enabled for the connected OP-1 MIDI output in Live, and start playback. These steps also apply when Link Audio is off.

### Analog pulse

1. Connect your clock source to the OP-1's Line In. Set **Clock → Pulse** in SYNC. Press **Shift + Mic** to open the OP-1 input screen and select **LINE IN**.
2. Choose **L+R**, **L**, or **R** in SYNC. The input screen displays **SYNC L+R**, **SYNC L**, or **SYNC R**.
3. Match **Input Rate** to the signal. **24 PPQ** means 24 pulses per quarter note. Other options are **48 PPQ**, **96 PPQ**, **1/4**, **1/8**, **1/16**, and **1/32**. Adjust **Pulse Threshold** and **Pulse Polarity** as needed. Enable **AutoStart** for pulse-controlled start and stop.

## Record Link Audio in Live

Link Audio sends audio from the OP-1 to Live. Choose the clock independently, for example MIDI Clock with Link Audio enabled.

1. Use **Ableton Live 12.4 or later**. Enable **Audio** in Live's Link settings, then enable **Link** in the Control Bar.
2. Set **Link Audio → On** in SYNC and enable the sources you want to send.

| Setting | Option | Sources in Live |
| --- | --- | --- |
| Link Audio Master/Tape | Off | No Master or Tape channels |
| Link Audio Master/Tape | On | Master |
| Link Audio Master/Tape | Tape | Tape 1 through Tape 4 |
| Link Audio Master/Tape | Both | Master and Tape 1 through Tape 4 |
| Link Audio Synth TX | On | Synth |
| Link Audio Drum TX | On | Drum |

3. In Live, create an audio track. Choose **OP-1** as its input type and select a channel such as **Master**. Use **Monitor → In** to hear it. Arm the track to record.

Set **Link Audio → Off** to stop streaming. SYNC saves your source selections. With **Clock → MIDI**, an already connected USB MIDI port stays active. Setting both **Clock** and **Link Audio** to **Off** lets SYNC return to the standard OP-1 USB configuration.

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| Live receives notes, but the OP-1 does not follow its tempo | Enable **Sync** on Live's MIDI output to the OP-1, select **Clock → MIDI**, and start playback. If USB reconnects, check which port Live currently lists. |
| Live sees the OP-1, but there is no audio | Enable **Link** and **Audio** in Live, turn on **Link Audio** and at least one source in SYNC, and check the track input and monitor settings. |
| Pulse does not change the tempo | Check **LINE IN**, the selected input side, rate, polarity and threshold. The blue SYNC label shows the selected input before the first pulse arrives. |

## Credits and licenses

**SYNC and Pulse Sync** were developed by **Volker Junginger (ESC_CODE)**. SYNC's pulse module is based on **Pulse Sync 1.1.11**, copyright © 2026 Volker Junginger, under GNU GPL v3.

**Ableton Link and Link Audio** were developed by **Ableton AG**. SYNC uses a C port based on **Ableton Link 4.1.1** under the GPL licensing option. The **Opus** plugin kit is by **LizardMan**. The **lwIP** networking stack is by the **Swedish Institute of Computer Science** and other contributors.

[Third-party notices](THIRD_PARTY_NOTICES.md) · [GNU GPL v3](LICENSE) · [Ableton Link source and license](https://github.com/Ableton/link/blob/5ba907ae027a8c2ec8824463e37296dcefc5e9be/LICENSE.md) · [Ableton Live release notes](https://www.ableton.com/en/release-notes/live-12/) · [Ableton MIDI sync guide](https://help.ableton.com/hc/en-us/articles/209071149-Synchronizing-Live-via-MIDI)
