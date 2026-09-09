from campaign_native_control import *
marks={};deadline=time.monotonic()+160
while time.monotonic()<deadline:
 if all('safe=1 ' in read(r,'wallet-check.txt') and 'applied=1' in read(r) for r in ['a','b']):break
 for r in ['a','b']:
  n=frame(r)
  if 'safe=1 ' not in read(r,'wallet-check.txt') and n>marks.get(r,0)+60:marks[r]=n;key(r,duration=24)
 time.sleep(.04)
else:raise RuntimeError('Cold native games did not load')
settle();assert progress('a',0x574)==0 and progress('a',0x574,True)==1
old=(base/'a/test.sav').stat().st_mtime_ns
command('a','campaign-fixture',5)
wait(lambda:'map=3,21 ' in read('a','world-check.txt') and safe('a'),'Could not reach route trainer',80)
key('a','0x3bf',4);n=frame('a');wait(lambda:frame('a')>n+60,'Facing stalled');key('a')
wait(lambda:progress('a',0x574)==1,'Original personal trainer battle did not finish',240,('a','b'))
settle(seconds=100)
wait(lambda:(base/'a/test.sav').stat().st_mtime_ns>old and 'dirty=0' in read('a','wallet-check.txt'),'Personal victory did not automatically checkpoint',80)
def snapshot(r):
 s=read(r)
 return {'tiles':int(re.search(r'field_tiles=(\d+)',s)[1]),'position':re.search(r'map=([^ ]+) tile=([^ ]+)',read(r,'world-check.txt'))[0],'balance':int(re.search(r'balance=(\d+)',read(r,'wallet-check.txt'))[1]),'masterball_receipt':progress(r,0x250),'janice':progress(r,0x574)}
expected={r:snapshot(r) for r in ['a','b']}
assert expected['a']['balance']==4544 and expected['b']['balance']==4544 and expected['a']['janice']==1
capture('automatic-personal-victory-save')
(base/'map-save-expected.json').write_text(json.dumps(expected,indent=2))
(base/'personal-save-verified.json').write_text(json.dumps({'check':'Aster original Janice victory automatically checkpoints native money and personal flag although world proof was already complete','no_manual_save_command':True},indent=2))
print('PASS: personal victory automatically saves even with existing campaign clearance',flush=True)
(base/'stop.txt').write_text('done\n')
