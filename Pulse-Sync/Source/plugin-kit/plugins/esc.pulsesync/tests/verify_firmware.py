"""Read-only check against the owner's firmware; never extracts/writes a firmware."""
import argparse
import binascii
import hashlib
import io
import json
import lzma
import struct
import tarfile
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument('firmware', type=Path)
a = ap.parse_args()
package = a.firmware.read_bytes()
assert struct.unpack_from('<I', package)[0] == binascii.crc32(package[4:]), 'package CRC'
with tarfile.open(fileobj=io.BytesIO(lzma.decompress(package[4:], format=lzma.FORMAT_ALONE))) as t:
    ldr = t.extractfile('OP1_vdk.ldr').read()
blocks = []
off = 0
while off + 16 <= len(ldr):
    flags, address, size, arg = struct.unpack_from('<IIII', ldr, off)
    if not flags & 0x1500:
        blocks.append((address, ldr[off + 16:off + 16 + size]))
    off += 16 + (0 if flags & 0x100 else size)
assert off == len(ldr), 'LDR chain'
def read(address, size):
    result, coverage = bytearray(size), bytearray(size)
    for start, data in blocks:
        lo, hi = max(address, start), min(address + size, start + len(data))
        if hi > lo:
            result[lo-address:hi-address] = data[lo-start:hi-start]
            coverage[lo-address:hi-address] = b'\1' * (hi-lo)
    assert all(coverage), f'{address:#x} not wholly in plain blocks'
    return bytes(result)
expected = {
    0x01967614: '00e80000c4040ce1',  # input meter and generated trampoline prologue
    0x019e31a4: '00e80600e305a66f08e1100548e1ab01',  # stock receive parser / queue address
    0x019e3988: '20e41900206420e6190060a0000c0918',  # F8 adds four internal ticks
    0x01a5e794: '09e1701849e18c0108911000',  # selected sync-mode getter
    0x018e462c: '88399e0134329e01d0399e01503a9e01ba3a9e01',  # F8..FC dispatch table
    0x01a5eb46: 'fbe32ffe',  # tempo handler calls timestamp-history update
}
for address, wanted in expected.items():
    assert read(address, len(bytes.fromhex(wanted))).hex() == wanted, f'mismatch at {address:#x}'
services = ['fw_snprintf','opus_cpu_charge','opus_on_block','opus_on_panel','opus_on_setting','opus_panel_text','opus_setting']
plain = b''.join(data for _,data in blocks)
print(json.dumps({'file': a.firmware.name, 'sha256': hashlib.sha256(package).hexdigest(),
    'firmware_signatures': 'PASS', 'checks': len(expected),
    'import_names_present': {name: (name.encode()+b'\0') in plain for name in services},
    'scope': 'static bytes and name presence only; not loader execution or hardware'}, indent=2))
