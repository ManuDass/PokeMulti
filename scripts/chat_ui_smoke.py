"""Replay real chat widgets in two isolated native clients; retain screenshots."""
from pathlib import Path
import argparse,json,os,re,shutil,socket,subprocess,time,uuid,csv
parser=argparse.ArgumentParser();parser.add_argument('--rom',required=True);parser.add_argument('--configuration',default='Release-0.6.0');args=parser.parse_args()
root=Path(__file__).resolve().parents[1];base=root/'cache'/('chat-ui-0.6.0-'+uuid.uuid4().hex);base.mkdir(parents=True)
print('Evidence: '+str(base),flush=True)
processes=[];sequence=0

def read(role,file='test-ui-status.txt'):
 try:return (base/role/file).read_text(encoding='utf-8')
 except OSError:return ''
def wait(predicate,message,seconds=30):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if predicate():return
  if any(p.poll() is not None for p in processes):raise RuntimeError('Process exited: '+str([p.poll() for p in processes]))
  time.sleep(.05)
 raise RuntimeError(message+'\n'+read('a')+'\n'+read('b'))
def write(path,text):
 temp=path.with_suffix('.tmp');temp.write_text(text,encoding='utf-8')
 for _ in range(40):
  try:temp.replace(path);return
  except PermissionError:time.sleep(.025)
 raise RuntimeError('Cannot replace '+str(path))
def send(role,op,arguments=''):
 global sequence
 sequence+=1;write(base/role/'test-ui-input.txt',f'{sequence} {op} {arguments}\n')
 wait(lambda:f'command={sequence} phase=0 ' in read(role),'UI command failed: '+op);time.sleep(.08)
def capture(label):
 for role in ['a','b']:
  send(role,'f12');time.sleep(.15)
  for file in ['game-ui.bmp','live.bmp','native.bmp','composition-pixels.csv','test-ui-status.txt','test-chat.txt','annotations-check.txt']:
   for _ in range(40):
    try:shutil.copyfile(base/role/file,base/(label+'-'+role+'-'+file));break
    except OSError:time.sleep(.04)
 def protected(role):
  rows=list(csv.DictReader((base/(label+'-'+role+'-composition-pixels.csv')).open()))
  blocked=0
  for row in rows:
   if row['obj_enabled']=='0' or int(row['native_key'])<=int(row['host_key']):
    assert all(row['before_'+c]==row['after_'+c] for c in 'rgb'),(label,role,row);blocked+=1
  return blocked
 return {role:protected(role) for role in ['a','b']}
def say(role,text):
 send(role,'click','300 300');send(role,'chat');wait(lambda:'keyboard=1' in read(role),'T did not focus chat')
 send(role,'text',json.dumps(text,ensure_ascii=False));send(role,'enter')
 wait(lambda:text in read('a','test-chat.txt') and text in read('b','test-chat.txt'),'Message did not echo to both players')
try:
 sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
 for role,state,name in [('a','cable-a.state','Aster'),('b','ready-b.state','Leaf')]:
  folder=base/role;folder.mkdir();(folder/'world.cfg').write_text('1 0\n');(folder/'test-keys.txt').write_text('0 0x3ff 100000\n');log=open(folder/'runtime.log','w')
  command=[str(root/'build'/args.configuration/'fr_game_harness.exe'),'--rom',str(Path(args.rom).resolve()),'--save',str(folder/'test.sav'),'--profile-dir',str(folder),'--name',name,'--load-state',str(root/'cache/runtime-check'/state),'--window','--test-ui','--test-report','--test-'+('host' if role=='a' else 'join'),str(port),'--frames','100000']
  processes.append(subprocess.Popen(command,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW))
 wait(lambda:all('peers=2' in read(role) and (base/role/'composition-pixels.csv').exists() for role in ['a','b']),'Two clients did not connect')
 capture('names')
 say('a','Hi Leaf!');say('b','Ready to explore Kanto?');capture('conversation');assert all('actor_overlap=0' in read(r,'annotations-check.txt') for r in ['a','b'])
 assert len(read('a','test-chat.txt').splitlines())==2 and len(read('b','test-chat.txt').splitlines())==2,'Duplicate chat echo'
 say('b','Let us explore Route 1 together, then visit the Pokemon Center to trade. I will bring my Pikachu!');capture('wrapped')
 send('a','escape');wait(lambda:'keyboard=0' in read('a'),'Esc did not restore game controls')
 # Exercise the actual Start menu against a currently visible bubble.
 sequence+=1;started=time.time();write(base/'a/test-keys.txt',f'{sequence} 0x3f7 4\n');wait(lambda:(base/'a/composition-pixels.csv').stat().st_mtime>started+1,'Menu composition capture stalled')
 blocked=capture('start-menu');assert blocked['a']>0,'No menu overlap exercised'
 sequence+=1;write(base/'a/test-keys.txt',f'{sequence} 0x3fd 4\n');time.sleep(.4)
 send('a','resize','940 650');capture('compact');send('a','resize','1140 760')
 send('a','f2');capture('wide');send('a','f2')
 wait(lambda:all('bubble=' not in read(r,'annotations-check.txt') for r in ['a','b']),'Speech did not expire',12);capture('expired')
 assert 'chat=3' in read('a') and 'chat=3' in read('b'),'History disappeared with bubbles'
 (base/'verified.json').write_text(json.dumps({'clients':2,'messages':3,'duplicate_echoes':0,'protected_menu_pixels':blocked,'checks':['T focus','Enter sends','UTF-8 transcript','Esc game focus','short and wrapped bubble captures','compact and wide frame layouts','history survives bubble expiry']},indent=2))
 print('PASS: room chat echo, input focus, captures, wrapping, history and protected composition.',flush=True)
finally:
 for role in ['a','b']:
  if (base/role).exists():write(base/role/'test-stop.txt','done\n')
 for p in processes:
  try:p.wait(timeout=12)
  except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)
