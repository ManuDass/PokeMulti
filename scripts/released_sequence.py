from pathlib import Path
import subprocess,time,re,sys,json
root=Path(__file__).resolve().parents[1];base=Path((root/'cache/released-active.txt').read_text()).resolve();folder=base/'a';seq=int(time.time()*1000)
def read(name='released-check.txt'):
 try:return (folder/name).read_text()
 except OSError:return ''
def wait(f,why,seconds=70):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if f():return
  time.sleep(.04)
 raise RuntimeError(why+'\n'+read())
def frame():
 m=re.search('frame=([0-9]+)',read());return int(m[1]) if m else 0
def key(action):
 global seq
 seq+=1;old=frame();mask={'A':'0x3fe','B':'0x3fd','up':'0x3bf','down':'0x37f','left':'0x3df','right':'0x3ef','wait':'0x3ff'}[action]
 p=folder/'test-keys.txt';tmp=p.with_suffix('.sequence-tmp');tmp.write_text(f'{seq} {mask} 8\n')
 for _ in range(100):
  try:tmp.replace(p);break
  except PermissionError:time.sleep(.01)
 wait(lambda:frame()>old+60,'Native input stalled')
 with (base/'sequence-actions.jsonl').open('a') as f:f.write(json.dumps({'action':action,'frame':frame(),'state':read()})+'\n')
def keys(actions):
 for a in actions.split():key(a)
print('PC test: '+str(base),flush=True)
keys('up A A A A A down down A wait A wait down down A wait A')
assert 'native=808cdc4' in read(),'Native storage did not open'
print('Native storage open; checking cancel.',flush=True)
keys('down down down down A B');assert 'pending=0 ' in read(),'Canceled release leaked into world'
keys('A down down down down A up A wait');wait(lambda:'pending=1 ' in read(),'First native release failed')
print('Confirmed first release.',flush=True)
for count in [2,3]:
 keys('A A right A down down down down A up A wait');wait(lambda:f'pending={count} ' in read(),'Native release failed');print('Confirmed release '+str(count),flush=True)
keys('A A B B wait B B down down down A wait')
wait(lambda:read().count('phase=3 ')==3 and 'pending=0 ' in read(),'Pokemon did not form a waiting crowd')
text=read();tiles=re.findall(r'phase=3 .*?tile=(\d+),(\d+)',text);assert len(set(tiles))==3,'Waiting crowd overlaps';assert all(abs(int(x)-26)>1 for x,y in tiles),'Doorway blocked'
(base/'native-pc-crowd-verified.json').write_text(json.dumps({'checks':['original PC cancel','three original confirmed releases','native save before publish','indoor departure','three distinct waiting tiles','clear doorway'],'tiles':tiles,'state':text},indent=2));print('PASS native PC and crowd',flush=True)
