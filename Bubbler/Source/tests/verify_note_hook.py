"""Optional owner-firmware check; no firmware or extracted code is distributed."""
import hashlib,io,json,lzma,struct,sys,tarfile
from pathlib import Path
result=[]
for filename in sys.argv[1:]:
 raw=Path(filename).read_bytes()
 with tarfile.open(fileobj=io.BytesIO(lzma.decompress(raw[4:],format=lzma.FORMAT_ALONE))) as archive:
  data=archive.extractfile('OP1_vdk.ldr').read()
 offset=0;actual=None
 while offset+16<=len(data):
  flags,address,length,argument=struct.unpack_from('<IIII',data,offset)
  if not flags&0x1500 and address<=0x01a85750<address+length:
   actual=data[offset+16+0x01a85750-address:offset+20+0x01a85750-address].hex()
  offset+=16+(0 if flags&256 else length)
 assert actual=='00e80400',(filename,actual)
 result.append({'file':Path(filename).name,'sha256':hashlib.sha256(raw).hexdigest(),'address':'0x01a85750','expected_entry':actual})
assert result,'Pass the owner-supplied firmware file(s) as arguments'
print(json.dumps(result,indent=2))
