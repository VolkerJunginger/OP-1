import json,sys,subprocess,os
from pathlib import Path
SKILL=Path(os.environ.get('OP1_DESIGN_SKILL',str(Path.home()/'.codex/skills/op1-design')))
sys.path.insert(0,str(SKILL/'scripts'))
from preview_font import PreviewFont,PreviewFace,ASSETS
ROOT=Path(__file__).resolve().parents[1];OUT=Path(os.environ.get('OP1_PREVIEW_OUT',str(ROOT/'build/design')));OUT.mkdir(parents=True,exist_ok=True)
font=None
if not os.environ.get('OP1_PREVIEW_TEXT_CACHE'):
 font=PreviewFont.__new__(PreviewFont)
 font.profile=json.loads((ASSETS/'profile.json').read_text())
 font.tokens=json.loads((SKILL/'references/design-kit.json').read_text())['tokens']
 glyph_cache=ROOT/'tests/preview-glyph-cache.json'
 signatures={str(w):font.profile['fonts'][str(w)]['sha256'] for w in [300,400]}
 cached=json.loads(glyph_cache.read_text()) if glyph_cache.exists() else {}
 if cached.get('signatures')!=signatures:
  faces={w:PreviewFace(font.profile,font.profile['fonts'][str(w)]) for w in [300,400]}
  cached={'signatures':signatures,'glyphs':{str(w):{c:face.glyph(c) for c in font.profile['textCharacters']} for w,face in faces.items()}}
  glyph_cache.write_text(json.dumps(cached))
 class CachedFace(PreviewFace):
  def __init__(self,profile,glyphs):self.profile=profile;self.data=glyphs
  def glyph(self,char):return self.data[char]
 font.faces={w:CachedFace(font.profile,cached['glyphs'][str(w)]) for w in [300,400]}
tokens=font.tokens if font else {}
for line in (ROOT/'.test-build/view-states.jsonl').read_text().splitlines():
 s=json.loads(line);texts=[]
 for i in range(4):
  if i==3 and s['caption'].startswith('3/'):continue
  right=i>=2;lower=i in [1,3];x=164 if right else 9;y=98 if lower else 9
  tone=['blue','green','white','orange'][i]
  for suffix,value,ty,size in [('label',s['labels'][i].upper(),y,'browserRow'),('value',s['values'][i].upper(),y+20,'parameterValue')]:
   if i==3 and suffix=='value':ty-=4 # closer approximation of native lower readout
   texts.append({'id':f'{i}-{suffix}','value':value,'x':x,'y':ty,'width':147,'maxWidth':147,'size':size,'align':'right' if right else 'left','tone':tone if suffix=='label' else 'white'})
 texts.append({'id':'page','value':s['caption'].upper(),'x':88,'y':12,'width':84,'maxWidth':84,'size':'browserRow','align':'right','tone':'secondary'})
 nodes=[]
 palette={-10:'#698eff',-11:'#00ed95',-12:'#ffffff',-13:'#ff423f',14:'#ffffff',12:'#b9b9c9'}
 for k,o in enumerate(s['art']):
  rgb=palette[o['colour']];a=o['alpha']
  colour='#'+''.join(f'{int(rgb[i:i+2],16)*a//255:02x}' for i in [1,3,5])
  if o['circle']:
   x,y,r=o['x'],o['y'],o['r'];v=r*.55228475
   commands=[['M',x+r,y],['C',x+r,y+v,x+v,y+r,x,y+r],['C',x-v,y+r,x-r,y+v,x-r,y],['C',x-r,y-v,x-v,y-r,x,y-r],['C',x+v,y-r,x+r,y-v,x+r,y],['Z']]
  else:commands=o['commands']
  points=[]
  for c in commands:
   points += [[c[i],c[i+1]] for i in range(1,len(c),2)]
  nodes.append({'id':f'art-{k}','commands':commands,'points':points,'width':0 if o['fill'] else o['width']/256,'color':colour,'fill':colour if o['fill'] else 'none'})
 import xml.etree.ElementTree as ET
 native=ET.Element('svg',{'xmlns':'http://www.w3.org/2000/svg','width':'320','height':'160','viewBox':'0 0 320 160'})
 for node in nodes:
  fmt=lambda v:format(v,'.4f').rstrip('0').rstrip('.') if isinstance(v,float) else str(v)
  d=' '.join(c[0]+' '+' '.join(fmt(v) for v in c[1:]) for c in node['commands'])
  ET.SubElement(native,'path',{'id':node['id'],'d':d,'fill':node['fill'],'stroke':'none' if node['fill']!='none' else node['color'],'stroke-width':fmt(node['width'])})
 (OUT/(s['state']+'-art-native.svg')).write_bytes(ET.tostring(native))
 scene={'width':320,'height':160,'nodes':nodes,'texts':texts,'readouts':[]}
 cache=os.environ.get('OP1_PREVIEW_TEXT_CACHE')
 if cache:
  prior=Path(cache)/(s['state']+'.json');measured=json.loads(prior.read_text())
  assert [t['value'] for t in measured['texts']]==[t['value'] for t in texts]
  measured['nodes']=nodes
  host=ET.parse(Path(cache)/(s['state']+'.svg')).getroot()
  for child in list(host):
   if child.get('id','').startswith('art-'):host.remove(child)
  for i,child in enumerate(list(native)):host.insert(i+1,child)
  svg=ET.tostring(host)
 else:svg,measured=font.render(scene,tokens)
 # Page 3 owns a compact two-value orange column. The label placement is
 # an approximate native-wrapper position; explicit value/mode sizes match
 # their native page_text calls. Keep the kit's XS/400 and S/300 weights.
 if s['caption'].startswith('3/'):
  rows=[('orange-label',s['labels'][3].upper(),'browserRow',10,91,'orange'),
        ('orange-value',s['values'][3].upper(),'parameterValue',16,104,'white')]
  if s.get('mode'):rows.append(('transpose',s['mode'].upper(),'browserRow',10,123,'orange'))
  host=ET.fromstring(svg)
  for ident,text,role,height,y,tone in rows:
   m=font.layout(text,role);factor=height/(m['top']-m['bottom']);scale=m['scale']*factor
   width=m['width']*factor;left=311-width;baseline=y+m['top']*factor
   for cursor,path in m['placed']:
    ET.SubElement(host,'{http://www.w3.org/2000/svg}path',{
     'd':path,'fill':tokens['color'][tone],
     'transform':f"translate({left+(cursor-m['left'])*scale} {baseline}) scale({scale} {-scale})"})
   bounds={'x':left,'y':y,'width':width,'height':height}
   assert left>=8 and left+width<=312 and y+height<=146
   measured['texts'].append({'id':ident,'value':text,'size':role,'previewWeight':400 if role=='browserRow' else 300,
    'nativeSize':height,'inkBounds':bounds,'x':left,'y':y,'width':width,'align':'right'})
  svg=ET.tostring(host)
 (OUT/(s['state']+'.svg')).write_bytes(svg);(OUT/(s['state']+'.json')).write_text(json.dumps(measured))
 checker=(SKILL/'scripts/lib/layout.mjs').as_uri()
 program="import fs from 'node:fs';import {checkLayout} from "+json.dumps(checker)+";const issues=checkLayout(JSON.parse(fs.readFileSync(0,'utf8')));if(issues.length){console.error(issues.join('\\n'));process.exit(1);}"
 subprocess.run([os.environ.get('NODE_BIN','node'),'--input-type=module','-e',program],input=json.dumps(measured),text=True,check=True)
print('PASS: artwork and readout states rendered and checked; native wrapper positions remain pending.')
