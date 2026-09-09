"""Two isolated native clients: Camp hotkey, grass rules, party poses, packing and legacy display settings."""
from pathlib import Path
import subprocess,os,socket,time,json,re,shutil,uuid,argparse
parser=argparse.ArgumentParser();parser.add_argument('--rom',required=True);parser.add_argument('--configuration',default='Release-0.24.2');parser.add_argument('--route1',action='store_true');a=parser.parse_args()
root=Path(__file__).resolve().parents[2];base=root/'cache'/('camp-native-'+uuid.uuid4().hex);base.mkdir();processes=[];sequence=10
print('Evidence: '+str(base),flush=True)
def read(role,name='camp-check.txt'):
 try:return (base/role/name).read_text(encoding='utf-8')
 except OSError:return ''
def write(path,content):
 tmp=path.with_suffix('.tmp');tmp.write_text(content,encoding='utf-8')
 for _ in range(100):
  try:tmp.replace(path);return
  except PermissionError:time.sleep(.02)
 raise RuntimeError('Cannot publish test command')
def wait(fn,why,seconds=70):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if fn():return
  if any(p.poll() is not None for p in processes):raise RuntimeError('Client exited '+str([p.poll() for p in processes]))
  time.sleep(.04)
 raise RuntimeError(why+'\n'+read('a')+'\n'+read('b'))
def frame(role):
 m=re.search('frame=([0-9]+)',read(role));return int(m[1]) if m else 0
def position(role):
 m=re.search('tile=(-?[0-9]+),(-?[0-9]+)',read(role,'world-check.txt'));return tuple(map(int,m.groups())) if m else None
def area(role):
 m=re.search(r'camp=0 .*?tile=([0-9]+),([0-9]+)',read(role));rows=re.search(r'^ground=0,([0-9,]+)',read(role),re.M)
 if not m or not rows:return set()
 x,y=map(int,m.groups());mask=list(map(int,rows[1].split(',')))
 return {(x-4+col,y-4+row) for row,bits in enumerate(mask) for col in range(11) if bits&(1<<col)}
def walk_to(role,target,ground):
 from collections import deque
 for _ in range(50):
  current=position(role)
  if current==target:return
  parent={current:None};queue=deque([current])
  while queue and target not in parent:
   pos=queue.popleft()
   for dx,dy in [(0,1),(0,-1),(-1,0),(1,0)]:
    q=(pos[0]+dx,pos[1]+dy)
    if q in ground and q not in parent:parent[q]=pos;queue.append(q)
  assert target in parent,'No walking path inside campsite'
  step=target
  while parent[step]!=current:step=parent[step]
  dx,dy=step[0]-current[0],step[1]-current[1]
  key(role,{(0,1):'0x37f',(0,-1):'0x3bf',(-1,0):'0x3df',(1,0):'0x3ef'}[(dx,dy)],12)
  assert position(role) in ground and (role!='a' or 'active=1' in read(role)),'Camper escaped or tent packed during walking'
 raise RuntimeError('Could not reach campsite boundary')
def semantic(role,action,id=0,value=0):
 global sequence
 sequence+=1;write(base/role/'test-world-command.txt',f'{sequence} {action} {id:x} {value:x}\n')
def key(role,mask,duration=12):
 global sequence
 sequence+=1;before=frame(role);write(base/role/'test-keys.txt',f'{sequence} {mask} {duration}\n');wait(lambda:frame(role)>before+duration+60,'Input stalled')
def dismiss_notice(role):
 for _ in range(8):
  key(role,'0x3fd',4)
  if 'locked=0 ' in read(role,'shared-world.txt'):return
 raise RuntimeError('Native notice did not close')

def ui(role,action):
 global sequence
 sequence+=1;write(base/role/'test-ui-input.txt',f'{sequence} {action}\n');wait(lambda:f'command={sequence} ' in read(role,'test-ui-status.txt'),'UI command stalled')
def capture(label):
 dest=base/label;dest.mkdir(exist_ok=True)
 for role in ['a','b']:
  ui(role,'click 410 360')
  old=(base/role/'game-ui.bmp').stat().st_mtime_ns if (base/role/'game-ui.bmp').exists() else 0
  ui(role,'f12');wait(lambda:(base/role/'game-ui.bmp').exists() and (base/role/'game-ui.bmp').stat().st_mtime_ns!=old,'Screenshot did not finish')
  for name in ['game-ui.bmp','live.bmp','native.bmp','world-check.txt','camp-check.txt','composition-check.txt','composition-pixels.csv','test-ui-status.txt']:
   for _ in range(100):
    try:shutil.copyfile(base/role/name,dest/(role+'-'+name));break
    except OSError:time.sleep(.02)
try:
 sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
 for role in ['a','b']:
  folder=base/role;folder.mkdir();(folder/'world.cfg').write_text('1 1\n');(folder/'voxel.cfg').write_text('enabled 1\ncurve 3\ntiltShift 3\ncamera 4\nzoom 2\n');(folder/'test-keys.txt').write_text('1 0x3ff 6000\n');log=(folder/'runtime.log').open('w')
  cmd=[str(root/'build'/a.configuration/'fr_game_harness.exe'),'--rom',str(Path(a.rom).resolve()),'--save',str(folder/'test.sav'),'--profile-dir',str(folder),'--name','Aster' if role=='a' else 'Leaf','--window','--test-ui','--test-report','--test-manual','--test-'+('host' if role=='a' else 'join'),str(port),'--load-state',str(root/'cache/runtime-check/cable-a.state'),'--fixture',('camp-route1'+('' if role=='a' else '-b')) if a.route1 else 'camp-'+role,'--frames','100000']
  processes.append(subprocess.Popen(cmd,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW))
  if role=='a':wait(lambda:'lawn=1' in read('a'),'Host lawn setup failed',100)
 wait(lambda:all('applied=1' in read(r,'shared-world.txt') and 'lawn=1' in read(r) for r in ['a','b']),'Two native campers failed setup',100)
 for role in ['a','b']:
  ui(role,'resize 940 650');ui(role,'f6')
 capture('legacy-settings-2d')
 ui('a','click 410 360');ui('a','camp')
 wait(lambda:all('camp=0 ' in read(r) and 'party=6' in read(r) for r in ['a','b']) and 'pending=0' in read('a'),'Camp did not appear on both clients')
 for role in ['a','b']:dismiss_notice(role)
 ground=area('a');assert ground==area('b') and position('a') in ground,'Walking areas differ or exclude camper'
 before=position('a');walk_to('a',max(ground,key=lambda p:(p[0],-abs(p[1]-before[1]))),ground);assert position('a')!=before and position('a') in ground and 'active=1' in read('a'),'Camper cannot walk within camp'
 capture('camper-walking')
 camp=re.search(r'camp=0 .*?tile=([0-9]+),([0-9]+)',read('a'));cx,cy=map(int,camp.groups())
 before_b=position('b');walk_to('b',(cx+4,cy+1),ground);key('b','0x3df',60)
 assert position('b')==(cx+4,cy+1),'Visitor walked through the tent'
 assert position('b')!=before_b,'Visitor froze while host camped'
 capture('camp-in-both-clients')
 poses=set();moods=set();start=frame('a')
 while frame('a')<start+1200:
  for role in ['a','b']:
   poses.update(re.findall('pixel=([0-9]+,[0-9]+)',read(role)));moods.update(re.findall('mood=([0-9]+)',read(role)))
  time.sleep(.06)
 assert len(poses)>12 and len(moods)>1,'Camp party did not wander and play'
 capture('party-playing')
 # A visitor crossing the outline cannot pack the owner's camp.
 boundary=max(ground,key=lambda p:(p[0],-abs(p[1]-(cy+1))))
 walk_to('b',boundary,ground)
 if not a.route1:
  key('b','0x3ef',12)
  assert position('b') not in ground and 'active=1' in read('a'),'Visitor leaving packed the owner camp'
  key('b','0x3ef',12) # Leave the owner's next step clear.
 else:
  key('b','0x3df',12) # Keep the native terrain boundary free for the owner.
 walk_to('a',boundary,ground)
 assert ground==area('a')==area('b'),'Walking changed the party outline'
 capture('camper-at-boundary')
 if a.route1:
  # The right edge of this compact clearing is a real native ledge.
  key('a','0x3ef',60)
  assert position('a')==boundary and 'active=1' in read('a'),'Bumping native terrain packed the camp'
  ui('a','camp');wait(lambda:all('camp=0 ' not in read(r) for r in ['a','b']) and 'active=0' in read('a'),'Manual pack did not synchronize')
  dismiss_notice('a');key('a','0x3df',12);assert position('a')!=boundary,'Manual packing left camper frozen'
 else:
  key('a','0x3ef',12)
  wait(lambda:all('camp=0 ' not in read(r) for r in ['a','b']) and 'active=0' in read('a'),'Walking outside did not pack on both clients')
  assert position('a') not in ground,'The old perimeter still confined the trainer'
  assert 'locked=0 ' in read('a','shared-world.txt'),'Automatic packing opened blocking dialogue'
  outside=position('a');key('a','0x3df',12)
  assert position('a')!=outside,'Walking stopped after automatic packing'
 capture('packed-by-walking-out' if not a.route1 else 'manual-pack-after-native-wall')
 semantic('b','fixture',0,0);wait(lambda:'map=5,5 ' in read('b','world-check.txt'),'Indoor fixture did not load');semantic('b','camp');start=frame('b');wait(lambda:frame('b')>start+60,'Indoor rejection stalled');assert 'active=0' in read('b') and 'lawn=0' in read('b'),'Indoor camp allowed'
 capture('packed-and-indoor-rejected')
 for role in ['a','b']:
  assert (base/role/'voxel.cfg').read_text()=='enabled 1\ncurve 3\ntiltShift 3\ncamera 4\nzoom 2\n', 'Retired settings were read and rewritten'
  assert not (base/role/'voxel-frames.csv').exists() and not (base/role/'voxel-scene.txt').exists(), 'Retired renderer ran'
 result={'checks':['Native 2D despite legacy enabled settings and F6; G camp shortcut at 940 x 650','same tent and six party members on two native clients',('native terrain still blocks the compact clearing; manual pack works' if a.route1 else 'trainer can cross perimeter; owner exit packs silently on both clients; visitor exit leaves owner camp intact'),'bounded wandering and playful poses','party remains bounded while camp is active; native obstacles and tent collision retained','indoor rejection'],'evidence':str(base),'walking_area':sorted(ground),'boundary':boundary}
 (base/'verified.json').write_text(json.dumps(result,indent=2));print('PASS: native camp smoke',flush=True)
finally:
 for role in ['a','b']:
  if (base/role).exists():write(base/role/'test-stop.txt','done\n')
 for p in processes:
  try:p.wait(timeout=12)
  except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)

