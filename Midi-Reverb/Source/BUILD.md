# Rebuild Midi-Reverb 1.0.1

Uses the existing Opus plugin kit at commit e47b95fce500c470447eae1ca31a638c67b6dea4 (core baseline 2.33), with the existing Blackfin compiler. Requires original OP-1 firmware 246 and Opus core 2.31 or later for the new native page interface.

Build Source/plugin with plugin_tool.py build and read the result with plugin_tool.py show. Validate plugin/screen.svg with plugin_tool.py svg. No ROMs are needed to build the plugin; they are needed to enable the corresponding banks on the device and for the import tests.

Desktop test:

    cc -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -I /path/to/plugin-kit/sdk Source/test.c Source/plugin/extra_banks.c -lm -o /tmp/midireverb-test
    /tmp/midireverb-test /path/to/midiverb.bin /path/to/midifex.bin /path/to/midiverb2.bin

The test uses mocked native opus_fx instances and page callbacks. It does not emulate the whole firmware, the FX manager, the Blackfin CPU, or physical display/audio timing.

0.2.9's dsp.c, programs.h, extra_banks.c, ROM-derived functions and lookup tables are unchanged. The 0.2.10 assembly and 0.2.11 block experiments are not included. The native adapter in device.c is new. An effect's eight words are: current preset, cutoff, feedback, mix, ROM index, remembered MidiVerb I preset, remembered MidiFex preset, remembered MidiVerb II preset. Each instance's audio state belongs to fx->state and delay line to fx->bulk. Stereo/master asks for no state or tank and passes audio through.

The adapter preserves the effective eight control words on repeated init calls for the same initialized instance during a sound reload. A factory call while the effect browser is open followed by default parameter words is treated as explicit re-selection and resets. A newly allocated/cleared managed instance starts fresh. Tests cover those calls; actual core and manager lifecycle behavior still needs device confirmation.

Version 1.0.0 changes only the version and display name from delivered 0.3.0. It does not include the cancelled 0.3.1 duplicate-bypass experiment.

Version 1.0.1 adds encoder acceleration to continuous controls.
