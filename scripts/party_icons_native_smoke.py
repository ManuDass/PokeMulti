"""Disposable native party-count and overhead artwork acceptance clients."""
from pathlib import Path
import argparse, csv, os, re, socket, subprocess, time, uuid, shutil
ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True);ap.add_argument('--configuration',default='Release-0.22.1');args=ap.parse_args()
root=Path(__file__).resolve().parents[1];base=root/'cache'/('party-icons-'+uuid.uuid4().hex[:12]);base.mkdir();processes=[]
s=socket.socket();s.bind(('127.0.0.1',0));port=s.getsockname()[1];s.close()
def read(role,file='annotations-check.txt'):
 try:return (base/role/file).read_text()
 except OSError:return ''
def value(role,field,file='story-gate-check.txt'):
 m=re.search(r'\b'+field+r'=(\d+)',read(role,file));return int(m[1]) if m else -1
def wait(fn,why,seconds=100):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if fn():return
  if any(p.poll() is not None for p in processes):raise RuntimeError('Client exited')
  time.sleep(.04)
 raise RuntimeError(why+'\n'+read('a')+'\n'+read('b'))
def write(role,file,text):
 path=base/role/file;tmp=path.with_suffix('.tmp');tmp.write_text(text)
 for _ in range(100):
  try:tmp.replace(path);return
  except PermissionError:time.sleep(.02)
 raise RuntimeError('Locked input')
def command(role,action,number=0):write(role,'test-world-command.txt',f'{time.monotonic_ns()} {action} 0 {number:x}\n')
def key(role,mask=0x3ff,duration=4):
 before=value(role,'frame');write(role,'test-keys.txt',f'{time.monotonic_ns()} {hex(mask)} {duration}\n')
 wait(lambda:value(role,'frame')>before+duration+90,'Input stalled')
def rows(role):return [dict((k,int(v)) for k,v in re.findall(r'(\w+)=(-?\d+)',line)) for line in read(role).splitlines() if line.startswith('party=')]
def expected(a,b):return all({v['party']:v['count'] for v in rows(role)}=={0:a,1:b} for role in ['a','b'])
def overlap(a,b):return max(0,min(a[0]+a[2],b[0]+b[2])-max(a[0],b[0]))*max(0,min(a[1]+a[3],b[1]+b[3])-max(a[1],b[1]))
def clear_labels(role):
 boxes=[]
 for v in rows(role):
  boxes.append((v['name_x'],v['name_y'],v['name_width'],v['name_height']))
  if v['count']:
   assert v['visible']==1 and v['width']==v['count']*8-1 and v['height']==7,'Source icon dimensions changed'
   assert v['y']+v['height']+2==v['name_y'],'Party row not directly above username'
   boxes.append((v['x'],v['y'],v['width'],v['height']))
  else:assert v['visible']==0,'Empty party has placeholder icons'
 assert all(overlap(a,b)==0 for i,a in enumerate(boxes) for b in boxes[i+1:]),'Overlapping username/icon rows'
def capture(label):
 out=base/label;out.mkdir()
 for role in ['a','b']:
  seq=time.monotonic_ns();write(role,'test-ui-input.txt',f'{seq} f12\n')
  wait(lambda:f'command={seq} phase=0 ' in read(role,'test-ui-status.txt'),'Screenshot command')
 time.sleep(.25)
 for role in ['a','b']:
  for file in ['game-ui.bmp','live.bmp','native.bmp','annotations-check.txt','composition-pixels.csv','story-gate-check.txt']:
   p=base/role/file
   if p.exists():shutil.copyfile(p,out/(role+'-'+file))
 print(label,read('a').strip(),read('b').strip(),flush=True)
print('Evidence: '+str(base),flush=True)
try:
 for role in ['a','b']:
  folder=base/role;folder.mkdir();(folder/'world.cfg').write_text('1 0\n');(folder/'test-keys.txt').write_text('1 0x3ff 6000\n');log=(folder/'runtime.log').open('w')
  cmd=[str(root/'build'/args.configuration/'fr_game_harness.exe'),'--rom',str(Path(args.rom).resolve()),'--save',str(folder/'test.sav'),'--profile-dir',str(folder),'--name','Aster' if role=='a' else 'Leaf','--window','--test-ui','--test-report','--test-manual','--test-'+('host' if role=='a' else 'join'),str(port),'--load-state',str(root/'cache/runtime-check/cable-a.state'),'--fixture','field-'+role,'--frames','100000']
  processes.append(subprocess.Popen(cmd,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW))
  if role=='a':wait(lambda:value('a','safe')==1,'Host boot',150)
 wait(lambda:expected(1,1) and all(value(role,'safe')==1 for role in ['a','b']),'Both client labels',150)
 command('a','party-size',1);command('b','party-size',6)
 wait(lambda:expected(1,6),'One and six did not synchronize');key('a');key('b')
 for role in ['a','b']:clear_labels(role)
 capture('one-and-six')
 command('a','party-size',6);wait(lambda:expected(6,6),'Full party change did not synchronize')
 for role in ['a','b']:clear_labels(role)
 capture('nearby-full-parties')
 command('b','party-eggs',0x2a)
 wait(lambda:all(any(v['party']==1 and v['eggs']==0x2a for v in rows(role)) for role in ['a','b']),'Mixed Egg slots did not synchronize')
 capture('mixed-eggs')
 command('b','party-eggs',0x22)
 wait(lambda:all(any(v['party']==1 and v['eggs']==0x22 for v in rows(role)) for role in ['a','b']),'Egg-to-Pokemon change did not synchronize')
 capture('egg-slot-updated')
 key('b',0x3df,64);capture('spaced-mixed-eggs')
 command('b','party-eggs',0)
 wait(lambda:all(any(v['party']==1 and v['eggs']==0 for v in rows(role)) for role in ['a','b']),'Egg markers did not clear')
 command('b','chat');wait(lambda:all('bubble=1 ' in read(role) for role in ['a','b']),'Chat bubble missing')
 for role in ['a','b']:
  clear_labels(role);assert all('actor_overlap=0' in line for line in read(role).splitlines() if line.startswith('bubble=')),'Chat overlaps a party row'
 capture('chat-with-party-icons')
 key('a',0x3f7);capture('native-menu')
 for role in ['a','b']:
  file=base/'native-menu'/(role+'-composition-pixels.csv')
  if file.exists():
   for row in csv.DictReader(file.open()):
    if row['obj_enabled']=='0' or int(row['native_key'])<=int(row['host_key']):
     assert all(row['before_'+c]==row['after_'+c] for c in 'rgb'),'Party overlay changed a protected native pixel'
 key('a',0x3fd);wait(lambda:value('a','safe')==1,'Menu exit')
 command('b','party-size',3);wait(lambda:expected(6,3),'Party reduction did not synchronize')
 command('a','party-size',0);wait(lambda:expected(0,3),'Empty party did not clear icons')
 for role in ['a','b']:clear_labels(role)
 capture('zero-and-three')
 (base/'acceptance.txt').write_text('PASS: 1/6, 6/6, 6/3, 0/3 live party counts and ordered Egg slots; Egg-to-Pokemon refresh; unaltered 7px artwork; nearby labels; chat; native menu composition\n'+read('a')+read('b'))
 print('PASS: party icons',flush=True)
finally:
 for role in ['a','b']:
  if (base/role).exists():(base/role/'test-stop.txt').write_text('done')
 for p in processes:
  try:p.wait(timeout=15)
  except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)
