"""Two native clients: shared story, private rewards, NPC ownership and free movement."""
from pathlib import Path
import subprocess,os,socket,time,json,re,shutil,uuid,argparse
parser=argparse.ArgumentParser();parser.add_argument('--rom',required=True);parser.add_argument('--configuration',default='Release-0.7.0');parser.add_argument('--wild',action='store_true');parser.add_argument('--pre-pokedex',action='store_true');parser.add_argument('--rival',action='store_true');a=parser.parse_args()
root=Path(__file__).resolve().parents[2];base=root/'cache'/('shared-native-0.7.0-'+uuid.uuid4().hex);base.mkdir();processes=[];sequence=10
print('Evidence: '+str(base),flush=True)
if a.pre_pokedex and not a.wild:parser.error('--pre-pokedex requires --wild')
def read(role,name='shared-world.txt'):
 try:return (base/role/name).read_text(encoding='utf-8')
 except OSError:return ''
def write(path,content):
 temp=path.with_suffix('.tmp');temp.write_text(content,encoding='utf-8')
 for _ in range(80):
  try:temp.replace(path);return
  except PermissionError:time.sleep(.02)
 raise RuntimeError('Cannot publish test command')
def wait(fn,why,seconds=60):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if fn():return
  if any(p.poll() is not None for p in processes):raise RuntimeError('Native process exited '+str([p.poll() for p in processes]))
  time.sleep(.04)
 raise RuntimeError(why+'\n'+read('a')+'\n'+read('b'))
def frame(role):
 m=re.search(r'frame=(\d+)',read(role));return int(m[1]) if m else 0
def position(role):
 m=re.search(r'tile=(-?\d+),(-?\d+)',read(role,'world-check.txt'));return tuple(map(int,m.groups())) if m else None
def key(role,mask,duration=8):
 global sequence
 sequence+=1;before=frame(role);write(base/role/'test-keys.txt',f'{sequence} {mask} {duration}\n');wait(lambda:frame(role)>=before+duration+60,'Key replay stalled')
def move(role,x,y):
 for _ in range(40):
  pos=position(role)
  if pos==(x,y):return
  cx,cy=pos;key(role,'0x3ef' if cx<x else '0x3df' if cx>x else '0x37f' if cy<y else '0x3bf')
 raise RuntimeError('Could not move '+role+' to '+str((x,y))+str(position(role)))
def semantic(role,kind,id,value):
 global sequence
 sequence+=1;write(base/role/'test-world-command.txt',f'{sequence} {kind} {id:x} {value:x}\n')
def capture(label):
 out=base/label;out.mkdir(exist_ok=True)
 for role in ['a','b']:
  for file in ['game-ui.bmp','live.bmp','native.bmp','shared-world.txt','world-check.txt','link-check.txt']:
   for _ in range(50):
    try:shutil.copyfile(base/role/file,out/(role+'-'+file));break
    except OSError:time.sleep(.02)
try:
 sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
 for role,state in [('a','fixture-wild.state' if a.wild else 'cable-a.state'),('b','fixture-wild.state' if a.wild else 'ready-b.state')]:
  folder=base/role;folder.mkdir();(folder/'world.cfg').write_text('1 '+('1' if a.wild else '0')+'\n');(folder/'test-keys.txt').write_text('1 0x3ff 6000\n');log=(folder/'runtime.log').open('w')
  if a.pre_pokedex:(folder/'test-world-command.txt').write_text('2 story 829 0\n',encoding='utf-8')
  cmd=[str(root/'build'/a.configuration/'fr_game_harness.exe'),'--rom',str(Path(a.rom).resolve()),'--save',str(folder/'test.sav'),'--profile-dir',str(folder),'--name','Aster' if role=='a' else 'Leaf','--window','--test-ui','--test-report','--test-'+('host' if role=='a' else 'join'),str(port),'--load-state',str(root/'cache/runtime-check'/state),'--frames','100000']
  processes.append(subprocess.Popen(cmd,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW))
 if a.pre_pokedex:
  wait(lambda:all('connected=1' in read(r) and 'starter=0' in read(r) and frame(r)>=240 and (base/r/'game-ui.bmp').exists() for r in ['a','b']),'Early-game setup failed')
  assert all('ready=0' in read(r) and 'applied=0' in read(r) for r in ['a','b']),'Story must wait for each player to obtain the Pokedex'
  capture('before-pokedex')
 else:wait(lambda:all('applied=1' in read(r) and (base/r/'game-ui.bmp').exists() for r in ['a','b']),'Shared baseline failed')
 if a.rival:
  semantic('a','fixture',0,2);semantic('b','fixture',0,2)
  wait(lambda:all('map=3,3 ' in read(r,'world-check.txt') and position(r)==(23,8) for r in ['a','b']),'Rival test setup failed')
  move('a',23,10);move('b',23,6)
  wait(lambda:'lease_owner=1 kind=1' in read('a') and 'npc=8 owner=1 tile=23,5' in read('a'),'Story rival did not approach the triggering player',90)
  assert 'locked=1' in read('b') and 'locked=0' in read('a'),'Story dialogue locked the wrong player'
  assert any('xy=23,5' in line for line in read('a','link-check.txt').splitlines()),'Spectator did not spawn the story NPC'
  before=position('a');key('a','0x3df',24);assert position('a')!=before,'Spectator could not continue walking';capture('rival-approaches-guest')
 elif a.wild:
  wait(lambda:'wild=' in read('a') and 'wild=' in read('b'),'Wild spawns were not shared',15 if a.pre_pokedex else 60)
  def populations_match():
   populations=[dict(re.findall(r'^wild=(\d+) species=(\d+)',read(r),re.M)) for r in ['a','b']]
   return bool(populations[0]) and populations[0]==populations[1]
  wait(populations_match,'Clients did not agree on wild identities and species')
  move('b',13,14);capture('shared-wild')
  semantic('b','chase',0,0)
  wait(lambda:'lease_owner=1 kind=3' in read('a') and 'main=80565b5' not in read('b','link-check.txt'),'Guest did not enter native wild battle',120)
  wait(lambda:'main=8011101 ' in read('b','link-check.txt'),'Native battle main callback was not reached')
  battle_frame=frame('b');wait(lambda:frame('b')>=battle_frame+300,'Battle introduction stalled')
  assert 'locked=0' in read('a');capture('guest-wild-battle')
  key('a','0x3df',16);assert position('a')!=(12,14),'Host froze during guest battle'
 else:
  semantic('a','story',0x820,1);wait(lambda:all('story=2080 value=1' in read(r) for r in ['a','b']),'Badge did not synchronize')
  semantic('a','story',0x254,1);wait(lambda:'story=596 value=1' in read('a'),'Local reward did not set');assert 'story=596 value=0' in read('b'),'TM claim leaked to other player'
  semantic('b','story',0x4052,2);wait(lambda:all('story=16466 value=2' in read(r) for r in ['a','b']),'Guest story did not reach host');capture('shared-story-private-reward')
  semantic('b','rewards',0,0);wait(lambda:'story=596 value=1' in read('b') and 'tm39_count_at_least_one=1' in read('b'),'Native Bag reward delivery failed');assert 'tm39_count_at_least_two=0' in read('b'),'Reward duplicated';semantic('b','rewards',0,0);key('b','0x3ff',4);assert 'tm39_count_at_least_two=0' in read('b'),'Second claim duplicated reward'
  move('b',10,4);key('b','0x3bf',4);key('b','0x3fe',4)
  wait(lambda:'locked=1' in read('b'),'Guest did not enter native service dialogue')
  before=position('a');key('a','0x3ef',24);assert position('a')!=before and 'locked=0' in read('a'),'Host was blocked by guest dialogue';capture('guest-npc-host-moving')
  move('a',10,4);key('a','0x3bf',4);key('a','0x3fe',4)
  wait(lambda:all('locked=1' in read(r) and 'lease=0' in read(r) for r in ['a','b']),'Cable service must admit both trainers simultaneously');capture('concurrent-cable-service')
 write(base/'verified.json',json.dumps({'native_clients':2,'wild_case':a.wild,'before_pokedex':a.pre_pokedex,'rival_case':a.rival,'checks':['same room world state','story encounters have one owner; cable services are personal','other trainer remains controllable']+(['scripted rival approaches only its triggering player','spectator spawns native rival'] if a.rival else [] if a.wild else ['bidirectional story state','independent TM claim'])},indent=2));print('PASS: native shared world acceptance',flush=True)
finally:
 for role in ['a','b']:
  if (base/role).exists():write(base/role/'test-stop.txt','done\n')
 for p in processes:
  try:p.wait(timeout=12)
  except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)
