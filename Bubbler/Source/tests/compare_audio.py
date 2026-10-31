"""Sample-identical DSP regression against the frozen 0.5.0 release audio, including octave variation."""
import ctypes as C,json
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1]/'.test-build';R.mkdir(exist_ok=True)
libs=[]
for name in ['baseline','current']:
 l=C.CDLL(str((R/(name+'.dylib')).resolve()));l.bb_create.restype=C.c_void_p;l.bb_destroy.argtypes=[C.c_void_p]
 l.bb_run.argtypes=[C.c_void_p,C.POINTER(C.c_short),C.POINTER(C.c_short),C.POINTER(C.c_short),C.c_int]
 l.bb_set_transpose.argtypes=[C.c_int];l.bb_note.argtypes=[C.c_int];libs.append(l)
def ptr(a):return a.ctypes.data_as(C.POINTER(C.c_short))
rng=np.random.default_rng(50);handles=[l.bb_create() for l in libs];par=np.array([20480,12000,26214,16384,29984,0,4108,-16352],dtype=np.int16)
changes=0
for block in range(50000):
 if block%13==0:
  par=rng.integers(0,32768,8,dtype=np.int16)
  par[5]=int(rng.integers(0,256))+(int(rng.integers(0,101))<<8)
  par[7]=-16384+int(rng.integers(0,64))*256+int(rng.integers(0,256))
  if block%26==0:par[3]=[0,16384,32767][(block//26)%3]
  if block%39==0:par[7]=-16384+(int(par[7])&255)
  mode=int(rng.integers(0,6));note=int(rng.integers(0,128))
  for l in libs:l.bb_set_transpose(mode);l.bb_note(note)
  changes+=1
 a=rng.integers(-32768,32768,64,dtype=np.int16);b=rng.integers(-32768,32768,64,dtype=np.int16);stereo=block%7<3;outs=[]
 for l,h in zip(libs,handles):
  x=a.copy();y=b.copy();l.bb_run(h,ptr(x),ptr(y) if stereo else None,ptr(par),64);outs.append((x,y))
 assert np.array_equal(outs[0][0],outs[1][0]),('left',block,par.tolist())
 assert np.array_equal(outs[0][1],outs[1][1]),('right',block,par.tolist())
for l,h in zip(libs,handles):l.bb_destroy(h)
# Static paths stay settled long enough to exercise every fast kernel and wraps.
N=44100*8//64*64;x=rng.integers(-24000,24001,N,dtype=np.int16);y=rng.integers(-24000,24001,N,dtype=np.int16)
fixtures=[]
for stereo in [False,True]:
 for name,mode,note,reverse,var,res,mix in [
  ('default',0,60,0,0,0,16384),('resonant',0,60,0,0,63,16384),
  ('half-speed',2,60,0,0,0,16384),('half-reverse',2,60,255,0,32,32767),
  ('octave-up',5,60,0,0,0,16384),('random-octaves',0,60,128,100,63,16384),
  ('fractional-fifth',4,60,128,100,63,27000),('midi-high',1,127,255,100,63,32767),
  ('midi-low',1,0,0,100,1,0),('integer-stride',1,120,128,100,32,23000)]:
  p=np.array([20480,20000,32767,mix,29984,reverse+(var<<8),4108,-16384+(res<<8)+32],dtype=np.int16);outs=[]
  for l in libs:
   h=l.bb_create();l.bb_set_transpose(mode);l.bb_note(note);a=x.copy();b=y.copy();l.bb_run(h,ptr(a),ptr(b) if stereo else None,ptr(p),N);l.bb_destroy(h);outs.append((a,b))
  assert np.array_equal(outs[0][0],outs[1][0]),(name,stereo,'left')
  assert np.array_equal(outs[0][1],outs[1][1]),(name,stereo,'right')
  fixtures.append(name+('-stereo' if stereo else '-mono'))
report={'dynamic_samples_per_channel':50000*64,'parameter_changes':changes,'mono_stereo_switching':True,'all_midi_notes_and_octave_variation':True,'settled_fixtures':fixtures,'fixture_samples_each':N,'bit_exact':True}
(R/'audio-regression.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
