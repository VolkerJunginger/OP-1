"""Paired audio-only host timings. These are not OP-1 CPU measurements."""
import ctypes as C,json,statistics
from pathlib import Path
R=Path(__file__).resolve().parents[1]/'.test-build';R.mkdir(exist_ok=True)
libs=[]
for name in ['baseline','current']:
 l=C.CDLL(str((R/(name+'.dylib')).resolve()));l.bb_bench_mode.argtypes=[C.POINTER(C.c_short),C.c_int,C.c_int,C.c_int,C.c_int];l.bb_bench_mode.restype=C.c_double;libs.append(l)
results={}
for name,stereo,mode,note,res,var,reverse,mix in [
 ('default-mono',0,0,60,0,0,0,16384),('default-stereo',1,0,60,0,0,0,16384),
 ('resonance-mono',0,0,60,63,0,0,16384),('resonance-stereo',1,0,60,63,0,0,16384),
 ('half-speed',0,2,60,0,0,0,16384),('half-speed-reverse',0,2,60,32,0,255,25000),
 ('octave-up',0,5,60,0,0,0,16384),('random-octaves',0,0,60,63,100,128,16384),
 ('midi-integer-strides',0,1,120,32,100,128,23000),('fractional-fifth',0,4,60,32,0,128,23000)]:
 p=(C.c_short*8)(20480,12000,26214,mix,29984,reverse+(var<<8),4108,-16384+(res<<8)+32)
 times=[[],[]]
 for trial in range(7):
  for i in ([0,1] if trial%2==0 else [1,0]):times[i].append(libs[i].bb_bench_mode(p,200000,stereo,mode,note))
 before,after=map(statistics.median,times)
 results[name]={'baseline_seconds':before,'optimized_seconds':after,'speedup':before/after,'host_time_reduction_percent':100*(1-after/before),'runs':7,'blocks_per_run':200000}
 print(name,round(before/after,3),'x',flush=True)
(R/'host-timings.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))
