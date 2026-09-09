"""Original Route 22 rival loss, friend takeover, and completion on private saves."""
from pathlib import Path
import argparse, os, re, socket, subprocess, time, uuid, shutil, threading
from PIL import Image
ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True);ap.add_argument('--configuration',default='Release-0.24.0');ap.add_argument('--departures',action='store_true');args=ap.parse_args()
root=Path(__file__).resolve().parents[2];base=root/'cache'/('story-retry-'+uuid.uuid4().hex[:12]);base.mkdir();processes=[]
s=socket.socket();s.bind(('127.0.0.1',0));port=s.getsockname()[1];s.close()
def read(role,file='story-retry-check.txt'):
 try:return (base/role/file).read_text()
 except OSError:return ''
def val(role,key,file='story-retry-check.txt'):
 m=re.search(r'\b'+key+r'=(-?\d+)',read(role,file));return int(m[1]) if m else -1
def pair(role,key,file='story-retry-check.txt'):
 m=re.search(r'\b'+key+r'=(-?\d+),(-?\d+)',read(role,file));return tuple(map(int,m.groups())) if m else None
def wait(fn,why,seconds=90):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if fn():return
  if any(p.poll() is not None for p in processes):raise RuntimeError('Client exited '+str([p.poll() for p in processes]))
  time.sleep(.04)
 raise RuntimeError(why+'\n'+read('a')+read('b')+read('a','story-gate-check.txt')+read('b','story-gate-check.txt'))
def write(role,file,text):
 path=base/role/file;tmp=path.with_suffix('.tmp');tmp.write_text(text)
 for _ in range(100):
  try:tmp.replace(path);return
  except PermissionError:time.sleep(.02)
 raise RuntimeError('Cannot publish test command')
def key(role,mask=0x3fe,duration=4,settle=30):
 before=val(role,'frame');write(role,'test-keys.txt',f'{time.monotonic_ns()} {hex(mask)} {duration}\n');wait(lambda:val(role,'frame')>before+duration+settle,'Input stalled')
def safe(role):return val(role,'safe','story-gate-check.txt')==1
def capture(label):
 out=base/label;out.mkdir()
 for role in ['a','b']:write(role,'test-ui-input.txt',f'{time.monotonic_ns()} f12\n')
 time.sleep(.6)
 for role in ['a','b']:
  for file in ['story-retry-check.txt','story-gate-check.txt','wallet-check.txt','shared-world.txt','world-check.txt']:
   p=base/role/file
   if p.exists():shutil.copyfile(p,out/(role+'-'+file))
  p=base/role/'game-ui.bmp'
  if p.exists():Image.open(p).save(out/(role+'.png'))
 print(label,read('a').strip(),read('b').strip(),flush=True)
def press_until(role,predicate,why,steps=180):
 for step in range(steps):
  if predicate():return
  key(role,settle=40)
  if step%30==0:print(why,step,read(role).strip(),flush=True)
 raise RuntimeError(why+'\n'+read(role))
print('Evidence: '+str(base),flush=True)
departure_samples=[];watch_done=threading.Event()
def watch_departure():
 for threshold in [100,550,1200]:
  while not watch_done.wait(.015):
   text=read('b','departure-check.txt');m=re.search(r'elapsed=(\d+)',text)
   if not m or int(m[1])<threshold:continue
   departure_samples.append(text);image=base/'b/game-ui.bmp';old=image.stat().st_mtime_ns if image.exists() else 0
   write('b','test-ui-input.txt',f'{time.monotonic_ns()} f12\n')
   deadline=time.monotonic()+2
   while time.monotonic()<deadline and (not image.exists() or image.stat().st_mtime_ns==old):time.sleep(.015)
   if image.exists():
    try:Image.open(image).save(base/f'departure-{threshold}.png')
    except OSError:pass
   break
  if watch_done.is_set():return
 (base/'departure-samples.txt').write_text(''.join(departure_samples))
watcher=threading.Thread(target=watch_departure,daemon=True)
try:
 for role in ['a','b']:
  folder=base/role;folder.mkdir();(folder/'test-keys.txt').write_text('1 0x3ff 6000\n');log=(folder/'runtime.log').open('w')
  cmd=[str(root/'build'/args.configuration/'fr_game_harness.exe'),'--rom',str(Path(args.rom).resolve()),'--save',str(folder/'test.sav'),'--profile-dir',str(folder),'--name','Aster' if role=='a' else 'Leaf','--window','--test-ui','--test-report','--test-manual','--test-'+('host' if role=='a' else 'join'),str(port),'--load-state',str(root/'cache/runtime-check/cable-a.state'),'--fixture','campaign-retry-'+role,'--frames','100000']
  processes.append(subprocess.Popen(cmd,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW))
  if role=='a':wait(lambda:safe('a'),'Host boot',150)
 wait(lambda:all(safe(r) and 'applied=1' in read(r,'shared-world.txt') for r in ['a','b']),'Shared field ready',150)
 assert pair('a','tile','story-gate-check.txt')==(34,5) and pair('b','tile','story-gate-check.txt')==(34,6)
 capture('before');key('a',0x3df,16)
 wait(lambda:val('a','lease')>0,'Route 22 claim')
 key('b',0x3df,50);assert pair('b','tile','story-gate-check.txt')==(34,6) and val('b','blocked','story-gate-check.txt')>0,'Friend crossed active rival event'
 press_until('b',lambda:safe('b'),'Dismiss road notice',25)
 press_until('a',lambda:val('a','battle')==1,'Start rival battle',70)
 wait(lambda:pair('b','gary')==(32,5),'Original seven-tile approach did not stage Gary');capture('first-battle')
 if args.departures:watcher.start()
 press_until('a',lambda:val('a','blackouts')==1,'Lose native battle')
 wait(lambda:val('b','retries')==1 and val('b','lease')==0,'Retry did not reach other client');capture('blackout-released')
 press_until('a',lambda:safe('a') and pair('a','map','story-gate-check.txt')==(5,4),'Native center healing',100)
 assert val('a','hp')==val('a','maxhp') and val('a','hp')>1,'Original native healing failed'
 assert all(val(r,'canonical')==1 for r in ['a','b']),'Loss advanced the shared story'
 assert pair('b','gary')==(32,5),'Gary moved while awaiting retry'
 assert val('b','balance','wallet-check.txt')==3000,'Friend paid for someone else blackout'
 after_loss=val('a','balance','wallet-check.txt');assert 0<=after_loss<3000,'Native blackout did not charge its owner'
 capture('center-and-waiting-rival');key('b',0x3df,16)
 press_until('b',lambda:val('b','battle')==1,'Friend retry',70)
 assert val('b','skipped')>0 and pair('b','gary')==(32,5),'Retry accumulated the original approach movement'
 assert val('b','lease')>0 and val('a','lease')==0,'Friend did not exclusively own retry';capture('friend-retry-same-position')
 press_until('b',lambda:val('b','canonical')==2 and val('b','lease')==0,'Win native rival battle',240)
 wait(lambda:all(val(r,'canonical')==2 and val(r,'retries')==0 for r in ['a','b']),'Shared completion not applied')
 press_until('b',lambda:safe('b'),'Finish victory notice',35)
 assert val('a','balance','wallet-check.txt')==after_loss,'Native rival reward changed the losing friend wallet'
 key('b',0x3df,32);assert pair('b','tile','story-gate-check.txt')[0]<33,'Completed route remained blocked'
 capture('shared-success-route-open')
 if args.departures:
  watcher.join(2);assert len(departure_samples)==3,'Missing spectator departure animation'
  assert int(re.search(r'lift=(\d+)',departure_samples[-1])[1])>240,'Departure did not fly off screen'
 report='PASS native Route 22 blackout, healing, private money, friend takeover from a different row, original staged position, native victory and shared unlock\n'+read('a')+read('b')
 (base/'acceptance.txt').write_text(report);print(report,flush=True)
finally:
 watch_done.set()
 for role in ['a','b']:
  if (base/role).exists():(base/role/'test-stop.txt').write_text('done')
 for p in processes:
  try:p.wait(timeout=15)
  except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)
