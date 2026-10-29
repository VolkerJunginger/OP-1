# Copyright, license and third-party notices

Pulse Sync: Copyright (C) 2026 Volker Junginger.

Pulse Sync is free software, distributed under the GNU General Public License,
version 3; individual source files bearing GPL-3.0-or-later notices additionally
permit later versions. See LICENSE for the full license text. The program is
provided without warranty, including merchantability or fitness for a particular
purpose, as described in that license.

## Opus 2 plugin kit

Pulse Sync was developed with the Opus 2 plugin kit by **LizardMan**
(GitHub: LizardMan245). The bundled tools, SDK headers, documentation and examples
are licensed **GPL-3.0**, as declared upstream. Their original notices and full
LICENSE are retained in Pulse-Sync/Source/plugin-kit.

- Upstream: https://github.com/LizardMan245/opus
- Plugin kit: https://github.com/LizardMan245/opus/tree/main/plugin-kit
- Upstream license: https://github.com/LizardMan245/opus/blob/main/plugin-kit/LICENSE

The plugin-kit snapshot used to build the tested 0.1.2 binary is included, with
Pulse Sync added under plugins/volker.pulsesync. Existing upstream kit files are
preserved. Complete source and build scripts are supplied alongside the binary.
The Blackfin GCC toolchain is obtained separately using the included instructions.

## GNU binutils-derived build tooling

The bundled tools/re/op1re/bfdis.py and bfconst.py identify their derivation from
GNU binutils 2.38, copyright Free Software Foundation, Inc., under GPL-3.0-or-later.
Those origin and license notices are preserved in the files.

## Firmware and trademarks

No Teenage Engineering firmware, extracted firmware binary, Opus firmware update,
ROM image or private patcher data is included. The GPL license for this code does
not grant rights to those separate materials. Users obtain/install Opus separately
and supply their own firmware where a local verification step requires it.

This is an independent community plugin, not an official release or endorsement
by Teenage Engineering or the Opus author. OP-1 is a trademark of Teenage Engineering.
