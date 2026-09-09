from campaign_native_control import *
import argparse
ap=argparse.ArgumentParser();ap.add_argument('mode',choices=['prepare','verify']);args=ap.parse_args()
def native_safe(r):return 'safe=1 ' in read(r,'wallet-check.txt')
marks={};deadline=time.monotonic()+160
while time.monotonic()<deadline:
 if all(native_safe(r) and 'applied=1' in read(r) for r in ['a','b']):break
 for r in ['a','b']:
  n=frame(r)
  if not native_safe(r) and n>marks.get(r,0)+60:marks[r]=n;key(r,duration=24)
 time.sleep(.04)
else:raise RuntimeError('Cold native game did not load')
settle()
def snapshot(r):
 s=read(r);assert progress(r,0x820)==1 and progress(r,0x254)==1 and progress(r,0x829)==1 and progress(r,0x238)==1 and progress(r,0x23a)==1
 assert 'tm39_count_at_least_one=1 tm39_count_at_least_two=0' in s
 return {'tiles':int(re.search(r'field_tiles=(\d+)',s)[1]),'position':re.search(r'map=([^ ]+) tile=([^ ]+)',read(r,'world-check.txt'))[0],'balance':int(re.search(r'balance=(\d+)',read(r,'wallet-check.txt'))[1]),'masterball_receipt':progress(r,0x250),'janice':progress(r,0x574)}
if args.mode=='prepare':
 for r in ['a','b']:
  command(r,'campaign-fixture',1)
  wait(lambda:'map=5,3 ' in read(r,'world-check.txt') and safe(r),'Could not enter the native Mart',100,('a','b'))
 settle()
 for r in ['a','b']:
  old=(base/r/'test.sav').stat().st_mtime_ns
  command(r,'campaign-save')
  wait(lambda:(base/r/'test.sav').stat().st_mtime_ns>old and 'dirty=0' in read(r,'wallet-check.txt'),'Native checkpoint did not flush',80,('a','b'))
 n=frame('a');wait(lambda:frame('a')>n+120,'Save diagnostics did not settle');capture('prepared-native-map-save')
 (base/'map-save-expected.json').write_text(json.dumps({r:snapshot(r) for r in ['a','b']},indent=2))
 print('PASS: native map/Quest Log checkpoint flushed for both trainers',flush=True)
 (base/'stop.txt').write_text('done\n')
else:
 expected=json.loads((base/'map-save-expected.json').read_text());actual={r:snapshot(r) for r in ['a','b']}
 assert actual==expected,(expected,actual)
 capture('cold-map-save-verified')
 (base/'map-save-verified.json').write_text(json.dumps({'checks':['native cold Continue retains exact local terrain metatile fingerprints','native map and position retained','money, personal Badges, TM and HM receipts retained','claimant-only Master Ball still personal'],'actual':actual},indent=2))
 print('PASS: both cold saves preserve exact terrain, location, wallet and personal receipts',flush=True)
