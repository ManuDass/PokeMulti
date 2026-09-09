from pathlib import Path
import subprocess,os,socket,time,json,re,csv,shutil,uuid,argparse
parser=argparse.ArgumentParser();parser.add_argument('--rom',required=True);parser.add_argument('--configuration',default='Release-0.3.2');config=parser.parse_args()
r=Path(__file__).resolve().parents[1];base=r/'cache'/('composition-multiplayer-0.3.2-'+uuid.uuid4().hex);base.mkdir()
print('Evidence: '+str(base),flush=True)
processes=[];sequence=0;reports=[]
def text(role,file):
 try:return (base/role/file).read_text()
 except OSError:return ''
def frame(role):
 m=re.search(r'frame=(\d+)',text(role,'link-check.txt'));return int(m[1]) if m else 0
def position(role):
 m=re.search(r'tile=(-?\d+),(-?\d+)',text(role,'world-check.txt'));return tuple(map(int,m.groups())) if m else None
def wait(test,why,seconds=45):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if test():return
  if any(p.poll() is not None for p in processes):raise RuntimeError('Process exited '+str([p.poll() for p in processes]))
  time.sleep(.08)
 raise RuntimeError(why+' '+str([(role,frame(role),position(role),text(role,'composition-check.txt')) for role in ['a','b']]))
def command(role,key,duration):
 global sequence
 sequence+=1;before=frame(role);path=base/role/'test-keys.txt';tmp=path.with_suffix('.tmp');tmp.write_text(f'{sequence} {key} {duration}\n')
 for attempt in range(50):
  try:tmp.replace(path);break
  except PermissionError:time.sleep(.02)
 wait(lambda:frame(role)>=before+duration+120,'Controller command stalled')
def capture(name):
 folder=base/name;folder.mkdir(exist_ok=True)
 for role in ['a','b']:
  wait(lambda:(base/role/'composition-pixels.csv').exists(),'No pixel trace')
  for file in ['game-ui.bmp','live.bmp','native.bmp','composition-pixels.csv','composition-check.txt','world-check.txt','link-check.txt']:
   for retry in range(50):
    try:
     data=(base/role/file).read_bytes()
     if not data or (file.endswith('.bmp') and len(data)<115254):time.sleep(.04);continue
     (folder/(role+'-'+file)).write_bytes(data);break
    except OSError:time.sleep(.04)
   else:raise RuntimeError('Could not capture '+file)
  rows=list(csv.DictReader((folder/(role+'-composition-pixels.csv')).open()))
  blocked={'ui':0,'terrain':0,'native':0,'window':0};front=0
  for row in rows:
   hk,nk=int(row['host_key']),int(row['native_key']);layer=int(row['native_layer'])
   before=tuple(int(row['before_'+c]) for c in ['r','g','b']);after=tuple(int(row['after_'+c]) for c in ['r','g','b'])
   if row['obj_enabled']=='0':blocked['window']+=1;assert before==after,(name,role,'window',row)
   elif nk<=hk:
    key='ui' if layer==0 else 'native' if layer==4 else 'terrain';blocked[key]+=1
    assert before==after,(name,role,'covered pixel overwritten',row)
   elif layer==4 and hk<nk and before!=after:front+=1
  result={'case':name,'role':role,'position':position(role),'blocked':blocked,'host_in_front_of_native':front,'diagnostic':text(role,'composition-check.txt').strip()}
  reports.append(result);print(result,flush=True)
def move(role,x,y):
 for _ in range(30):
  current=position(role)
  if current==(x,y):return
  cx,cy=current
  key='0x3ef' if cx<x else '0x3df' if cx>x else '0x37f' if cy<y else '0x3bf'
  command(role,key,8)
 raise RuntimeError('Could not reach target '+str((role,x,y,position(role))))
try:
 sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
 for role,state in [('a','cable-a.state'),('b','ready-b.state')]:
  p=base/role;p.mkdir(exist_ok=True);(p/'world.cfg').write_text('1 0\n');(p/'test-keys.txt').write_text('0 0x3ff 6000\n')
  stop=p/'test-stop.txt'
  if stop.exists():stop.unlink()
  args=[str(r/'build'/config.configuration/'fr_game_harness.exe'),'--rom',str(Path(config.rom).resolve()),'--save',str(p/'test.sav'),'--profile-dir',str(p),'--name','RED' if role=='a' else 'LEAF','--window','--test-ui','--test-report','--test-'+('host' if role=='a' else 'join'),str(port),'--load-state',str(r/'cache/runtime-check'/state),'--frames','100000']
  log=(p/'run.log').open('w');processes.append(subprocess.Popen(args,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW))
 wait(lambda:all(frame(role)>5100 and 'peers=2' in text(role,'link-check.txt') for role in ['a','b']),'Two clients did not connect')
 capture('connected')
 command('a','0x3f7',4);capture('start-menu')
 assert any(p['case']=='start-menu' and p['role']=='a' and p['blocked']['ui']>0 for p in reports),'Remote trainer did not intersect menu'
 command('a','0x3fd',4)
 move('a',8,7);move('b',8,6);capture('b-behind-a')
 move('a',8,5);capture('a-behind-b')
 move('a',8,6);capture('same-ground-tie')
 move('b',10,4);capture('counter-and-npc')
 command('b','0x3fe',4);capture('npc-dialogue')
 assert any(p['case']=='npc-dialogue' and p['role']=='b' and p['blocked']['ui']>0 for p in reports),'Remote trainer did not intersect dialogue'
 assert any(p['blocked']['native']>0 for p in reports if 'behind' in p['case']),'No actual native-object overlap exercised'
 assert any(p['host_in_front_of_native']>0 for p in reports if 'behind' in p['case']),'No front-of-native overlap exercised'
 assert any(int(re.search(r'host_intersections=(\d+)',p['diagnostic'])[1])>0 for p in reports),'No follower/trainer overlap exercised'
 (base/'verified.json').write_text(json.dumps(reports,indent=2)+'\n')
 print('PASS: real multiplayer crossings, followers, stable equal-depth overlap and protected Start menu/dialogue pixels.',flush=True)
finally:
 for role in ['a','b']:
  p=base/role
  if p.exists():(p/'test-stop.txt').write_text('done\n')
 for process in processes:
  try:process.wait(timeout=10)
  except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=10)
