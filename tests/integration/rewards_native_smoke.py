"""Original ROM execution: exclusive/per-trainer gifts and saved, agreed battle money."""
from pathlib import Path
import subprocess,os,socket,time,json,re,shutil,uuid,argparse
ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True);ap.add_argument('--configuration',default='Release-0.7.0');ap.add_argument('--case',choices=['unique','shared','refund','battle','cold'],required=True);ap.add_argument('--warm-cache');ap.add_argument('--reuse');a=ap.parse_args()
root=Path(__file__).resolve().parents[2];base=Path(a.reuse) if a.reuse else root/'cache'/('rewards-native-'+a.case+'-'+uuid.uuid4().hex);base.mkdir(exist_ok=True);runs=[];seq=10
print('Evidence: '+str(base),flush=True)
def read(role,name='wallet-check.txt'):
 try:return (base/role/name).read_text(encoding='utf-8')
 except OSError:return ''
def val(role,field,name='wallet-check.txt'):
 m=re.search(r'\b'+field+r'=(-?\d+)',read(role,name));return int(m[1]) if m else -1
def write(path,content):
 temp=path.with_suffix('.tmp');temp.write_text(content,encoding='utf-8')
 for _ in range(100):
  try:temp.replace(path);return
  except PermissionError:time.sleep(.02)
 raise RuntimeError('Could not send test input')
def key(role,mask='0x3fe',duration=6):
 global seq
 seq+=1;write(base/role/'test-keys.txt',f'{seq} {mask} {duration}\n')
def command(role,action,id=0,value=0):
 global seq
 seq+=1;write(base/role/'test-world-command.txt',f'{seq} {action} {id:x} {value:x}\n')
def wait(fn,why,seconds=60,advance=None):
 end=time.monotonic()+seconds;marks={}
 while time.monotonic()<end:
  if fn():return
  if any(p.poll() is not None for p in runs):raise RuntimeError('Process exited '+str([p.poll() for p in runs])+'\n'+read('a')+'\n'+read('b'))
  if advance:
   for role in ['a','b']:
    frame=val(role,'frame')
    if frame>=marks.get(role,0)+60:marks[role]=frame;advance(role,frame)
  time.sleep(.03)
 raise RuntimeError(why+'\n'+read('a')+'\n'+read('b')+'\n'+read('a','link-check.txt')+'\n'+read('b','link-check.txt'))
def capture(label):
 out=base/label;out.mkdir(exist_ok=True)
 for role in ['a','b']:
  for name in ['game-ui.bmp','live.bmp','native.bmp','wallet-check.txt','link-check.txt','shared-world.txt','test-ui-status.txt']:
   for _ in range(30):
    try:shutil.copyfile(base/role/name,out/(role+'-'+name));break
    except OSError:time.sleep(.02)
def tile(role):
 m=re.search(r'tile=(-?\d+),(-?\d+)',read(role,'world-check.txt'));return tuple(map(int,m.groups())) if m else None
def move(role,x,y):
 for _ in range(40):
  p=tile(role)
  if p==(x,y):return
  before=val(role,'frame');cx,cy=p;key(role,'0x3ef' if cx<x else '0x3df' if cx>x else '0x37f' if cy<y else '0x3bf',6);wait(lambda:val(role,'frame')>=before+90,'Movement stalled')
 raise RuntimeError('Movement failed '+str(tile(role)))
try:
 sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
 for role in ['a','b']:
  folder=base/role;folder.mkdir(exist_ok=True)
  if a.warm_cache and not (folder/'native-cache').exists():shutil.copytree(Path(a.warm_cache)/role/'native-cache',folder/'native-cache')
  if (folder/'test-stop.txt').exists():(folder/'test-stop.txt').unlink()
  if a.case=='cold':
   for report in ['wallet-check.txt','link-check.txt','test-ui-status.txt','world-check.txt']:
    if (folder/report).exists():(folder/report).unlink()
  (folder/'world.cfg').write_text('1 0\n');write(folder/'test-keys.txt','1 0x3ff 6000\n');log=(folder/('cold.log' if a.case=='cold' else 'runtime.log')).open('w')
  cmd=[str(root/'build'/a.configuration/'fr_game_harness.exe'),'--rom',str(Path(a.rom).resolve()),'--save',str(folder/'test.sav'),'--profile-dir',str(folder),'--name','Aster' if role=='a' else 'Leaf','--window','--test-ui','--test-report','--frames','100000']
  if a.case!='cold':
   cmd+=['--test-'+('host' if role=='a' else 'join'),str(port),'--load-state',str(root/'cache/runtime-check'/('cable-a.state' if role=='a' else 'ready-b.state')),'--fixture','unique-eevee' if a.case in ['unique','shared'] else 'wager-'+role,'--save-state',str(folder/'final.state')]
   if a.case in ['unique','shared']:cmd+=['--test-manual','--test-rewards','7' if a.case=='shared' else '3']
   else:cmd+=['--test-battle','--test-stake','100']
  runs.append(subprocess.Popen(cmd,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW))
 if a.case=='cold':
  wait(lambda:all(val(r,'safe')==1 for r in ['a','b']),'Saved game did not resume',120,lambda r,n:key(r))
  first=min(val(r,'frame') for r in ['a','b']);wait(lambda:all(val(r,'frame')>=first+240 for r in ['a','b']),'Cold overworld did not remain active')
  assert all(tile(r) is not None and any(int(f,16)&0x10000 for f in re.findall(r'flags=([0-9a-f]+)',read(r,'link-check.txt'))) for r in ['a','b']), 'Cold save did not restore a normal player avatar'
  capture('cold-load');balances=[val(r,'balance') for r in ['a','b']];assert balances in [[3000,3000],[3100,2900]],balances
  assert all(val(r,'receipt')==2 for r in ['a','b']),'Missing saved completed receipt'
  print('PASS: cold-loaded native saves retain independent wallets '+str(balances),flush=True)
 elif a.case in ['unique','shared']:
  wait(lambda:all(val(r,'safe')==1 and tile(r)==(7,4) and 'applied=1' in read(r,'shared-world.txt') for r in ['a','b']),'Gift room setup failed')
  capture('before-gift')
  for r in ['a','b']:key(r,'0x3bf',4)
  mark=val('a','frame');wait(lambda:val('a','frame')>=mark+90,'Facing failed')
  for r in ['a','b']:key(r)
  expected=2 if a.case=='shared' else 1
  wait(lambda:sum('species=133 ' in read(r,'link-check.txt') for r in ['a','b'])==expected,'Eevee gift not delivered',60,lambda r,n:key(r))
  # B declines nickname; finish the script before checking synchronized claim.
  wait(lambda:all(val(r,'safe')==1 for r in ['a','b']),'Gift dialogue did not finish',60,lambda r,n:key(r,'0x3fd'))
  assert all('story=611 value=1' in read(r,'shared-world.txt') for r in ['a','b'])
  assert sum('species=133 ' in read(r,'link-check.txt') for r in ['a','b'])==expected
  capture('after-gift');print('PASS: Eevee recipients = '+str(expected),flush=True)
 else:
  wait(lambda:all(val(r,'phase')==2 and val(r,'balance')==2900 and val(r,'held')==100 for r in ['a','b']),'Native saved deposits failed',120)
  assert all((base/r/'test.sav').stat().st_size==131072 for r in ['a','b']);capture('saved-deposits');print('PASS: each native wallet deposited P100 (3000 -> 2900)',flush=True)
  if a.case=='refund':
   command('a','wager-cancel');wait(lambda:all(val(r,'phase')==0 and val(r,'balance')==3000 and val(r,'receipt')==2 for r in ['a','b']),'Native refund failed',120);capture('refunded');print('PASS: both deposits refunded and autosaved once',flush=True)
  else:
   for r in ['a','b']:move(r,10,4);key(r,'0x3bf',4)
   marks={};stage='menu';menu_seen=None;mark=0;deadline=time.monotonic()+600
   while time.monotonic()<deadline:
    states={r:read(r,'link-check.txt') for r in ['a','b']};frame=min(val(r,'frame') for r in ['a','b'])
    if stage=='menu' and all('task=809cc99' in s for s in states.values()):
     if menu_seen is None:menu_seen=frame
     if frame>=menu_seen+60:
      for r in ['a','b']:key(r,'0x37f',4)
      stage='choose';mark=frame;print('Selecting Colosseum',flush=True)
    elif stage=='choose' and frame>=mark+60:
     for r in ['a','b']:key(r)
     stage='prompts';mark=frame
    elif stage=='prompts' and all('xy=6,8' in s and 'xy=7,8' in s for s in states.values()):
     stage='battle';print('Both trainers entered original Colosseum',flush=True)
    elif stage in ['menu','prompts','battle','exit']:
     for r in ['a','b']:
      n=val(r,'frame');state=states[r];phase=val(r,'phase')
      if n<marks.get(r,0)+60:continue
      if stage=='menu' and 'task=809cc99' in state:continue
      marks[r]=n
      linked_field='main=80565b5' in state and 'remote=1 ' in state
      if linked_field and ((stage=='battle' and phase==2) or stage=='exit'):
       actor=1 if r=='a' else 2;m=re.search(fr'object={actor} xy=([0-9]+),([0-9]+)',state)
       if not m:continue
       x,y=map(int,m.groups())
       if stage=='battle':
        target=3 if r=='a' else 10
        mask='0x3bf' if y>7 or x==target else '0x3df' if x>target else '0x3ef'
       else:
        target=6 if r=='a' else 7
        mask='0x3fe' if 'locked=1 ' in read(r,'shared-world.txt') else '0x37f' if y<7 or x==target and y<=8 else '0x3df' if x>target else '0x3ef' if x<target else '0x3fe'
       # One step per observed position; fixed long holds can miss fades or overshoot.
       key(r,mask,4)
      elif stage!='exit' or 'locked=1 ' in read(r,'shared-world.txt'):key(r)
    if stage=='battle' and all(val(r,'phase')==5 and 'main=80565b5' in states[r] and 'remote=1 ' in states[r] for r in ['a','b']):
     capture('matched-result-before-exit');stage='exit';marks={}
     print('Matching win/loss received; leaving Cable Club before payout save',flush=True)
    if all(val(r,'phase')==0 and val(r,'receipt')==2 for r in ['a','b']):break
    if any(p.poll() is not None for p in runs):raise RuntimeError('Native battle process exited')
    time.sleep(.03)
   else:raise RuntimeError('Native battle settlement timeout\n'+read('a')+'\n'+read('b')+'\n'+read('a','link-check.txt')+'\n'+read('b','link-check.txt'))
   assert [val(r,'balance') for r in ['a','b']]==[3100,2900],'Wrong independent payout balances';capture('paid');print('PASS: original linked battle settled to P3100 / P2900',flush=True)
 (base/('cold-verified.json' if a.case=='cold' else 'verified.json')).write_text(json.dumps({'case':a.case,'native_clients':2,'verified':True},indent=2))
finally:
 for r in ['a','b']:
  if (base/r).exists():write(base/r/'test-stop.txt','done\n')
 for p in runs:
  try:p.wait(timeout=12)
  except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)
