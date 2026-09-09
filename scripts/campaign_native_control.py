from pathlib import Path
import time,re,json,sys
root=Path(__file__).resolve().parents[1]
base=Path((root/'cache/campaign-native-active.txt').read_text().strip())
def read(role,name='shared-world.txt'):
 try:return (base/role/name).read_text(encoding='utf-8')
 except OSError:return ''
def frame(role):
 m=re.search(r'frame=(\d+)',read(role,'link-check.txt'));return int(m[1]) if m else 0
def progress(role,id,world=False):
 m=re.search(r'progress='+str(id)+r' local=(\d+) world=(\d+)',read(role));return int(m[2 if world else 1]) if m else -1
def write(role,file,text):
 path=base/role/file;tmp=path.with_suffix('.tmp');tmp.write_text(text,encoding='utf-8')
 for _ in range(100):
  try:tmp.replace(path);return
  except PermissionError:time.sleep(.02)
 raise RuntimeError('Could not publish native command')
def command(role,action,id=0,value=0):
 write(role,'test-world-command.txt',f'{time.time_ns()} {action} {id:x} {value:x}\n')
def key(role,mask='0x3fe',duration=24):write(role,'test-keys.txt',f'{time.time_ns()} {mask} {duration}\n')
def ui(role,action):write(role,'test-ui-input.txt',f'{time.time_ns()} {action}\n')
def safe(role):return 'status=2 locked=0' in read(role)
def wait(fn,why,seconds=45,advance=()):
 end=time.monotonic()+seconds;marks={}
 while time.monotonic()<end:
  if fn():return
  for role in advance:
   now=frame(role)
   if not safe(role) and now>=marks.get(role,0)+18:marks[role]=now;key(role)
  time.sleep(.04)
 raise RuntimeError(why+'\n'+read('a')+'\n'+read('b'))
def settle(roles=('a','b'),seconds=60):
 last={r:frame(r) for r in roles};stable=0
 def done():
  nonlocal stable
  if all(safe(r) and frame(r)>last[r]+90 for r in roles):
   if not stable:stable=time.monotonic()
   return time.monotonic()-stable>1
  stable=0;return False
 wait(done,'Dialogues did not finish',seconds,roles)
def capture(label):
 import shutil
 out=base/label;out.mkdir(exist_ok=True)
 roles=[r for r in ['a','b'] if (base/r).exists()]
 for role in roles:
  ui(role,'f12')
 time.sleep(.6)
 for role in roles:
  for name in ['game-ui.bmp','shared-world.txt','world-check.txt','wallet-check.txt','link-check.txt','test-ui-status.txt']:
   p=base/role/name
   if p.exists():shutil.copyfile(p,out/(role+'-'+name))
 print('Capture:',out,flush=True)
if __name__=='__main__':
 role=sys.argv[1]
 for action in sys.argv[2:]:
  if action=='status':print(read(role))
  elif action=='settle':settle()
  elif action=='capture':capture('capture-'+str(time.time_ns()))
  elif action=='a':key(role)
  elif action=='up':key(role,'0x3bf')
  elif action=='left':key(role,'0x3df')
  elif action=='right':key(role,'0x3ef')
  elif action=='down':key(role,'0x37f')
  elif action.startswith('fixture:'):command(role,'campaign-fixture',int(action.split(':')[1]))
  elif action=='camp':command(role,'camp')
