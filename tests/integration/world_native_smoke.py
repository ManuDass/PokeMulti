"""Isolated host-world persistence, nearby T trade, cold reconnect and host-loss acceptance."""
from pathlib import Path
from native_fixture import local_field_state
import argparse,hashlib,json,os,re,socket,subprocess,time,uuid,zipfile
ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True);ap.add_argument('--configuration',default='Release-0.25.0');ap.add_argument('--state',type=Path,help='Local field savestate made with this exact ROM');args=ap.parse_args()
root=Path(__file__).resolve().parents[2];base=root/'cache'/('world-native-'+uuid.uuid4().hex[:12]);base.mkdir();rom=Path(args.rom).resolve()
data=zipfile.ZipFile(rom).read(next(n for n in zipfile.ZipFile(rom).namelist() if n.lower().endswith('.gba'))) if rom.suffix.lower()=='.zip' else rom.read_bytes()
worldid=uuid.uuid4().hex;world=base/'worlds'/worldid;world.mkdir(parents=True);(world/'world.cfg').write_text(f'PMWORLD1 "{worldid}" "Test world" "{hashlib.sha256(data).hexdigest()}"\n')
ids={r:uuid.uuid4().hex for r in ['a','b']};runtime={};processes={};logs=[]
for r in ids:(base/r).mkdir();(base/r/'identity.cfg').write_text(ids[r]);(base/r/'identity.key').write_text(uuid.uuid4().hex)
sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
state=local_field_state(root,Path(args.rom).resolve(),args.state)
print('Evidence: '+str(base),flush=True)
def read(r,file='link-check.txt',ui=False):
 try:return ((base/r) if ui else runtime[r]).joinpath(file).read_text(encoding='utf-8')
 except (OSError,KeyError):return ''
def value(r,key,file='link-check.txt',ui=False):
 m=re.search(r'\b'+key+r'=(-?\d+)',read(r,file,ui));return int(m[1]) if m else -1
def wait(fn,why,seconds=90,alive=True):
 deadline=time.monotonic()+seconds
 while time.monotonic()<deadline:
  if fn():return
  if alive and any(p.poll() is not None for p in processes.values()):raise RuntimeError(why+'; process exited '+str({r:p.poll() for r,p in processes.items()}))
  time.sleep(.035)
 raise RuntimeError(why+'\n'+str({r:read(r,'world-save-check.txt')+read(r,'field-battle-check.txt') for r in processes}))
def write(path,text):
 tmp=path.with_suffix('.tmp');tmp.write_text(text)
 for _ in range(100):
  try:tmp.replace(path);return
  except PermissionError:time.sleep(.01)
 raise RuntimeError('Locked test command')
def key(r,mask=0x3fe,duration=4,settle=65):
 start=value(r,'frame');write(runtime[r]/'test-keys.txt',f'{time.monotonic_ns()} {hex(mask)} {duration}\n');wait(lambda:value(r,'frame')>=start+duration+settle,'Native key stalled')
def ui(r,command):
 seq=time.monotonic_ns();write(base/r/'test-ui-input.txt',f'{seq} {command}\n');wait(lambda:f'command={seq} phase=0 ' in read(r,'test-ui-status.txt',True),'UI command stalled')
def safe(r):return value(r,'safe','wallet-check.txt')==1
def start(r,cold=False):
 if r=='a':runtime[r]=world/'players'/ids[r]/'runtime';runtime[r].mkdir(parents=True,exist_ok=True)
 if r in runtime:(runtime[r]/'test-stop.txt').unlink(missing_ok=True)
 oldstamp=(runtime[r]/'link-check.txt').stat().st_mtime_ns if r in runtime and (runtime[r]/'link-check.txt').exists() else 0
 log=(base/r/('cold.log' if cold else 'first.log')).open('w');logs.append(log)
 cmd=[str(root/'build'/args.configuration/'fr_game_harness.exe'),'--rom',str(rom),'--save',str(base/r/'bootstrap.sav'),'--profile-dir',str(base/r),'--identity-dir',str(base/r),'--name','Aster' if r=='a' else 'Leaf','--window','--test-ui','--test-report','--test-manual','--room-port',str(port),'--room-key','world-native-key','--capacity','8','--online','host' if r=='a' else 'join','--frames','100000']
 cmd+=['--world-dir',str(world)] if r=='a' else ['--join-address','127.0.0.1']
 if not cold:cmd+=['--load-state',str(state),'--fixture','field-'+r]
 old=set((base/r/'guest-cache'/worldid).glob('*')) if r=='b' else set()
 processes[r]=subprocess.Popen(cmd,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW)
 if r=='b':
  wait(lambda:bool(set((base/r/'guest-cache'/worldid).glob('*'))-old),'Guest download before boot')
  runtime[r]=next(iter(set((base/r/'guest-cache'/worldid).glob('*'))-old))
 wait(lambda:(runtime[r]/'link-check.txt').exists() and (runtime[r]/'link-check.txt').stat().st_mtime_ns!=oldstamp,'Native startup',120)
 if cold:
  # A presses pass the original title/Continue/recap, loading only the host's flash.
  for _ in range(45):
   if safe(r) and value(r,'frame')>120:break
   key(r,settle=100)
 wait(lambda:safe(r),'Native field readiness',120)
def capture(name):
 from PIL import Image
 for r in processes:
  ui(r,'f12');time.sleep(.1);Image.open(base/r/'game-ui.bmp').save(base/(name+'-'+r+'.png'))
def identity(r):
 return {'party':re.findall(r'^party=.*$',read(r),re.M),'balance':value(r,'balance','wallet-check.txt'),'position':re.search(r'map=\S+ tile=\S+',read(r,'world-check.txt'))[0]}
try:
 start('a');start('b');wait(lambda:all(value(r,'ack','world-save-check.txt')>0 for r in processes),'Automatic host checkpoints')
 capture('joined')
 assert value('a','peers','test-ui-status.txt',True)==2
 # T goes through the actual SDL shortcut with the trainer facing their neighbor.
 ui('a','escape');ui('a','trade');wait(lambda:value('b','ui','field-battle-check.txt')==4,'T did not send trade invitation')
 for _ in range(12):
  if value('b','choice_ready','field-battle-check.txt')==1:break
  key('b')
 capture('trade-invite');key('b',0x37f);key('b')
 for _ in range(20):
  if all(safe(r) for r in processes):break
  for r in processes:
   if not safe(r):key(r,0x3fd)
 ui('a','chat');ui('a','text "Testing the letter t"');ui('a','enter');ui('a','escape')
 wait(lambda:'Testing the letter t' in read('b','test-chat.txt',True),'Chat still works by clicking its input')
 assert value('b','invitation','test-ui-status.txt',True)==-1
 expected={r:identity(r) for r in processes}
 # Native campaign save exercises the original flash saver; host then requests guests.
 before=value('b','request','world-save-check.txt');write(runtime['a']/'test-world-command.txt',f'{time.monotonic_ns()} campaign-save 0 0\n')
 wait(lambda:value('b','request','world-save-check.txt')>before,'Host save did not request guest checkpoint')
 wait(lambda:all((world/'players'/ids[r]/'checkpoint.pmsv').exists() for r in processes),'Master world player files')
 write(runtime['a']/'test-stop.txt','close\n');wait(lambda:all(p.poll() is not None for p in processes.values()),'Graceful host closure did not return guest',25,False)
 assert all(p.returncode==0 for p in processes.values()),{r:p.returncode for r,p in processes.items()}
 assert (base/'b'/'world-return.txt').exists()
 processes.clear();start('a',True);start('b',True);wait(lambda:all(safe(r) for r in processes),'Cold world resume')
 actual={r:identity(r) for r in processes};assert actual==expected,(expected,actual)
 capture('cold-resume')
 # Only this test-owned host is terminated to simulate an abrupt server failure.
 processes['a'].terminate();processes['a'].wait(10)
 wait(lambda:processes['b'].poll() is not None,'Guest kept playing after host crash',20,False);assert processes['b'].returncode==0
 (base/'acceptance.json').write_text(json.dumps({'checks':['T trade invitation with native consent','chat input retains T typing','automatic host-owned private trainer checkpoints','host save requests guest save','graceful host close exits guest','cold reconnect preserves party, money and location','host crash exits guest without solo continuation'],'trainers':actual,'world':str(world)},indent=2))
 print('PASS: native world saves, T trade, cold resume and host-loss exit',flush=True)
finally:
 for p in processes.values():
  if p.poll() is None:p.terminate()
 for p in processes.values():
  try:p.wait(10)
  except subprocess.TimeoutExpired:p.kill()
 for log in logs:log.close()
