"""Two private native clients exercise ROM coordinate roadblocks and busy NPCs."""
from pathlib import Path
import argparse, os, re, socket, subprocess, time, uuid, shutil
ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True);ap.add_argument('--configuration',default='Release-0.22.1');ap.add_argument('--case',choices=['oldman','pewter','arrival'],default='oldman');args=ap.parse_args()
root=Path(__file__).resolve().parents[1];base=root/'cache'/('story-gates-'+args.case+'-'+uuid.uuid4().hex[:12]);base.mkdir();processes=[]
s=socket.socket();s.bind(('127.0.0.1',0));port=s.getsockname()[1];s.close()
def read(role,file='story-gate-check.txt'):
 try:return (base/role/file).read_text()
 except OSError:return ''
def val(role,field,file='story-gate-check.txt'):
 m=re.search(r'\b'+field+r'=(-?\d+)',read(role,file));return int(m[1]) if m else -1
def pos(role):
 m=re.search(r'tile=(-?\d+),(-?\d+)',read(role));return tuple(map(int,m.groups())) if m else None
def wait(fn,why,seconds=90):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if fn():return
  if any(p.poll() is not None for p in processes):raise RuntimeError('Client exited')
  time.sleep(.04)
 raise RuntimeError(why+'\n'+read('a')+'\n'+read('b')+'\n'+read('a','shared-world.txt')+'\n'+read('b','shared-world.txt'))
def write(role,file,text):
 path=base/role/file;tmp=path.with_suffix('.tmp');tmp.write_text(text)
 for _ in range(100):
  try:tmp.replace(path);return
  except PermissionError:time.sleep(.02)
 raise RuntimeError('Cannot publish input')
def send(role,mask=0x3fe,duration=4):write(role,'test-keys.txt',f'{time.monotonic_ns()} {hex(mask)} {duration}\n')
def key(role,mask=0x3fe,duration=4,settle=30):
 before=val(role,'frame');send(role,mask,duration);wait(lambda:val(role,'frame')>before+duration+settle,'Input stalled')
def command(role,action,id=0,value=0):write(role,'test-world-command.txt',f'{time.monotonic_ns()} {action} {id:x} {value:x}\n')
def idle(role):return val(role,'safe')==1 and val(role,'lease')==0 and val(role,'reserved')==0 and val(role,'deferred')==0
def finish(role):
 for _ in range(50):
  if idle(role):
   key(role,0x3ff,4,60)
   if idle(role):return
  key(role,settle=40)
 raise RuntimeError('Scene did not finish '+read(role))
def capture(label):
 out=base/label;out.mkdir()
 for role in ['a','b']:write(role,'test-ui-input.txt',f'{time.monotonic_ns()} f12\n')
 time.sleep(.6)
 for role in ['a','b']:
  for file in ['game-ui.bmp','story-gate-check.txt','shared-world.txt','world-check.txt']:
   p=base/role/file
   if p.exists():shutil.copyfile(p,out/(role+'-'+file))
 print(label,read('a').strip(),read('b').strip(),flush=True)
print('Evidence: '+str(base),flush=True)
try:
 for role in ['a','b']:
  folder=base/role;folder.mkdir();(folder/'test-keys.txt').write_text('1 0x3ff 6000\n');log=(folder/'runtime.log').open('w')
  cmd=[str(root/'build'/args.configuration/'fr_game_harness.exe'),'--rom',str(Path(args.rom).resolve()),'--save',str(folder/'test.sav'),'--profile-dir',str(folder),'--name','Aster' if role=='a' else 'Leaf','--window','--test-ui','--test-report','--test-manual','--test-'+('host' if role=='a' else 'join'),str(port),'--load-state',str(root/'cache/runtime-check/cable-a.state'),'--fixture','campaign-gate-'+('oldman' if args.case=='arrival' else args.case)+'-'+role,'--frames','100000']
  processes.append(subprocess.Popen(cmd,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW))
  if role=='a':wait(lambda:val('a','safe')==1,'Host boot',150)
 wait(lambda:all(idle(r) and 'applied=1' in read(r,'shared-world.txt') for r in ['a','b']),'Shared field ready',150)
 capture('before')
 if args.case in ['oldman','arrival']:
  assert pos('a')==(21,12) and pos('b')==(22,12)
  key('a',0x3bf);key('a')
  wait(lambda:val('a','safe')==0 and val('a','lease')>0,'Old man dialogue did not own scene')
  if args.case=='arrival':
   command('b','gate-arrival');key('b',0x3ff)
   key('b',0x3bf,80)
   assert val('b','deferred')>0 and val('b','lease')==0 and pos('b')==(22,11),'Unreserved arrival did not wait for ownership'
   key('b',0x3bf,60);assert pos('b')==(22,11),'Waiting trainer escaped the active story tile'
   capture('arrival-held');finish('a')
   wait(lambda:val('b','deferred')==0 and val('b','lease')>0,'Waiting scene did not resume after ownership release')
   capture('arrival-resumed');finish('b');assert pos('b')==(22,12),'Resumed ROM barrier did not return trainer'
  else:
   key('b',0x3bf,80);assert pos('b')==(22,12) and val('b','blocked')>0,'Busy NPC allowed bypass'
   capture('busy-road-blocked');finish('b')
   key('b',0x37f,16);assert pos('b')[1]>12,'Unrelated movement was frozen'
   key('b',0x3bf,16);assert pos('b')==(22,12),'Return to road entrance'
   assert val('a','safe')==0,'Owner dialogue unexpectedly advanced'
   finish('a');before=val('b','reservations');key('b',0x3bf,16)
   wait(lambda:val('b','reservations')>before and val('b','safe')==0,'Native road script was skipped')
   capture('native-road-script');finish('b');assert pos('b')==(22,12),'ROM did not walk trainer back from closed road'
  command('a','story',0x4057,2)
  wait(lambda:all(val(r,'oldman')==1 for r in ['a','b']),'Parcel progress did not open road');finish('a');finish('b')
  before=val('b','blocked');key('b',0x3bf,32)
  assert pos('b')[1]<=10 and val('b','blocked')==before,'Shared unlock did not allow crossing'
  capture('shared-road-open')
 else:
  assert pos('a')==(41,21) and pos('b')==(41,22)
  # Both inputs are issued before either client finishes its first step.
  send('a',0x3ef,32);send('b',0x3ef,32)
  wait(lambda:(val('a','lease')>0) != (val('b','lease')>0),'Simultaneous scene was not exclusive')
  owner='a' if val('a','lease')>0 else 'b';other='b' if owner=='a' else 'a'
  wait(lambda:val(other,'blocked')>0,'Second approach was not blocked')
  assert pos(other)[0]==41,'Second trainer crossed gym guide trigger'
  capture('simultaneous-guide-blocked');finish(other)
  # Let the guide physically leave the route while the other player tries it.
  for _ in range(30):
   if pos(owner)[0]<40:break
   key(owner,settle=45)
  assert pos(owner)[0]<40 and val(owner,'lease')>0,'Guide did not leave the route during escort'
  key(other,0x3ef,90);assert pos(other)[0]==41,'Moving guide left an opening'
  capture('moving-guide-blocked');finish(other);finish(owner)
  assert all(val(r,'pewter')==0 for r in ['a','b']),'Talking to guide incorrectly unlocked route'
  command('a','story',0x820,1)
  wait(lambda:all(val(r,'pewter')>=1 for r in ['a','b']),'Shared Brock progress did not open path');finish('a');finish('b')
  key(other,0x3ef,32);assert pos(other)[0]>=43,'Brock unlock did not allow route access'
  capture('shared-pewter-open')
 report='PASS '+args.case+'\n'+read('a')+read('b');(base/'acceptance.txt').write_text(report);print(report,flush=True)
finally:
 for role in ['a','b']:
  if (base/role).exists():(base/role/'test-stop.txt').write_text('done')
 for p in processes:
  try:p.wait(timeout=15)
  except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)
