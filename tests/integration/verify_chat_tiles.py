"""Pixel-compare rendered frames against the current, unmodified source PNGs.
Run with a Python installation containing Pillow, after fr_labels_tests.
"""
from pathlib import Path
from PIL import Image
import argparse,hashlib,json,struct
parser=argparse.ArgumentParser();parser.add_argument('--configuration',default='Release-0.6.0');args=parser.parse_args()
root=Path(__file__).resolve().parents[2];build=root/'build'/args.configuration;source=root/'assets/chat';output=build/'chat-label-fixtures'
names=['Upper Left Corner','Upper Middle Tile','Upper Right Corner','Left Middle Tile','Middle Tile','Right Middle Tile','Bottom Left Corner','Bottom Middle Tile','Bottom Right Corner','Thin Left Tile','Thin Middle Tile','Thin Right Tile']
tiles=[Image.open(source/(name+'.png')).convert('RGBA') for name in names]
assert all(im.size==(16,16) for im in tiles)
fill=tiles[4].getpixel((8,8));reports=[]
for path in sorted(output.glob('*.rgba')):
 data=path.read_bytes();w,h=struct.unpack_from('<II',data);assert w%16==0 and h%16==0,(path.name,'tile dimensions or added pixels')
 actual=Image.frombytes('RGBA',(w,h),data[8:]);expected=Image.new('RGBA',(w,h))
 for y in range(0,h,16):
  for x in range(0,w,16):
   col=0 if x==0 else 2 if x==w-16 else 1
   index=9+col if h==16 else (0 if y==0 else 6 if y==h-16 else 3)+col
   expected.paste(tiles[index],(x,y))
 changes=[]
 for y in range(h):
  for x in range(w):
   a,b=actual.getpixel((x,y)),expected.getpixel((x,y))
   if a==b:continue
   assert a==(0,0,0,255) and b==fill,(path.name,'tile artwork changed',(x,y),a,b)
   assert 3<=x<w-3 and 3<=y<h-3,(path.name,'less than three pixels of padding',(x,y))
   changes.append((x,y))
 assert changes,(path.name,'text missing')
 # At the original location, every pixel of the decorative corner is intact.
 assert actual.crop((0,h-8,9,h)).tobytes()==expected.crop((0,h-8,9,h)).tobytes(),(path.name,'corner icon modified')
 actual.save(output/(path.stem+'.png'))
 bounds=[min(x for x,y in changes),min(y for x,y in changes),max(x for x,y in changes),max(y for x,y in changes)]
 if h>16:assert abs(bounds[1]-(h-1-bounds[3]))<=2,(path.name,'unbalanced vertical whitespace',bounds,h)
 reports.append({'sample':path.stem,'tiles':[h//16,w//16],'pixels':[w,h],'text_bounds':bounds,'changed_pixels':len(changes),'other_pixels_match_originals':True})
assert {r['tiles'][0] for r in reports}>={1,2,3}
hashes={name+'.png':hashlib.sha256((source/(name+'.png')).read_bytes()).hexdigest() for name in names}
for name,sha in hashes.items():assert hashlib.sha256((build/'Fonts/CHAT BUBBLE TILES'/name).read_bytes()).hexdigest()==sha,('stale build tile',name)
(output/'verified.json').write_text(json.dumps({'source_tile_sha256':hashes,'samples':reports},indent=2)+'\n')
print(json.dumps(reports,indent=2));print('PASS: all 12 original assets match; all non-text RGBA pixels and corner icons intact; >=3px padding; 1/2/3+ rows; no added pointer.')
