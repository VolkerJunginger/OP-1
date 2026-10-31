"""Verify the frozen native image, imports, relocations and hook."""
from pathlib import Path
import hashlib,json,sys
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'plugin-kit/tools/op1mod'))
from plugin import read_opmod
filename=Path(sys.argv[1]) if len(sys.argv)>1 else root/'build/bubbler.opmod'
info=read_opmod(filename.read_bytes())
def plain(v):
 if isinstance(v,bytes):return v.hex()
 if isinstance(v,(tuple,list)):return [plain(x) for x in v]
 return v
meta={k:plain(info[k]) for k in ['zero','entry','imports','relocs','exports','places','flags']}
digest=hashlib.sha256(info['image']+json.dumps(meta,sort_keys=True,separators=(',',':')).encode()).hexdigest()
expected=json.loads((root/'baseline/native-image.json').read_text())
assert digest==expected['execution_sha256'],'Native image differs from the frozen reference (check compiler/kit)'
print('PASS: executable image, imports, relocations and hook match approved Bubbler 0.5.0')
