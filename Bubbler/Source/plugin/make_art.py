from pathlib import Path
import json
root=Path(__file__).parent
source=json.loads((root/'artwork.json').read_text())
# Closed shapes remain separate; never connect disconnected outlines.
shapes=[]
for node in source['boy']:
 chunks=[];chunk=[]
 for c in node['commands']:
  if c[0]=='M' and chunk:chunks.append(chunk);chunk=[]
  chunk.append(c)
 if chunk:chunks.append(chunk)
 for i,commands in enumerate(chunks):
  if len(commands)<2:continue
  shapes.append({'id':node['id']+str(i),'commands':commands,'fill':bool(node.get('fill'))})
# Lift the feet and lower body slightly to respect the y140 content guide.
for shape in shapes:
 for c in shape['commands']:
  for i in range(2,len(c),2):
   if c[i]>125:c[i]=125+(c[i]-125)*.8
h='/* Generated from artwork.json by make_art.py. Original artwork; GPL-3.0-or-later. */\n'
h+='struct bb_art_cmd { short op; short p[6]; };\nstruct bb_art_shape { unsigned first,count,fill; };\n'
cmds=[];records=[]
for shape in shapes:
 records.append((len(cmds),len(shape['commands']),int(shape['fill'])))
 for c in shape['commands']:
  coords=[round(v*16) for v in c[1:]];coords += [0]*(6-len(coords))
  cmds.append("    { '%s', { %s } },"%(c[0],', '.join(map(str,coords))))
h+='static const struct bb_art_cmd BB_BOY_CMDS[] = {\n'+'\n'.join(cmds)+'\n};\n'
h+='static const struct bb_art_shape BB_BOY_SHAPES[] = {\n'+''.join('    { %i, %i, %i },\n'%r for r in records)+'};\n'
h+='static const short BB_BUBBLES[12][3] = {\n'+''.join('    { %i, %i, %i },\n'%tuple(v) for v in source['bubbles'])+'};\n'
(root/'bubbler_art_data.h').write_text(h)
