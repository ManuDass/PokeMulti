"""Two isolated native clients exercise X challenges without visiting Cable Club.
Uses only the caller's local ROM and local development state; never production saves.
"""
from pathlib import Path
import argparse, os, re, socket, subprocess, time, uuid
from PIL import Image
ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True);ap.add_argument('--configuration',default='Release-0.23.2');ap.add_argument('--indoor',action='store_true');ap.add_argument('--case',choices=['win','free','refund','decline','cancel'],default='win');args=ap.parse_args()
root=Path(__file__).resolve().parents[1];base=root/'cache'/('field-native-'+uuid.uuid4().hex[:12]);base.mkdir();processes=[]
s=socket.socket();s.bind(('127.0.0.1',0));port=s.getsockname()[1];s.close()
def read(role,file='field-battle-check.txt'):
 try:return (base/role/file).read_text()
 except OSError:return ''
def val(role,field,file='field-battle-check.txt'):
 m=re.search(r'\b'+field+r'=(-?\d+)',read(role,file));return int(m[1]) if m else -1
def wait(fn,why,seconds=90):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if fn():return
  if any(p.poll() is not None for p in processes):raise RuntimeError('Client exited: '+str([p.poll() for p in processes]))
  time.sleep(.05)
 raise RuntimeError(why+'\n'+read('a')+'\n'+read('b'))
def write(path,text):
 tmp=path.with_suffix('.tmp');tmp.write_text(text)
 for _ in range(100):
  try:tmp.replace(path);return
  except PermissionError:time.sleep(.02)
 raise RuntimeError('Locked test command')
def key(role,mask=0x3fe,duration=4,settle=25):
 seq=time.monotonic_ns();frame=val(role,'frame');write(base/role/'test-keys.txt',f'{seq} {hex(mask)} {duration}\n');wait(lambda:val(role,'frame')>=frame+duration+settle,'Native key stalled')
def ui(role,op):
 seq=time.monotonic_ns();write(base/role/'test-ui-input.txt',f'{seq} {op}\n');wait(lambda:f'command={seq} phase=0 ' in read(role,'test-ui-status.txt'),'UI command stalled')
def capture(role,name):
 p=base/role/'game-ui.bmp';old=p.stat().st_mtime_ns if p.exists() else 0;ui(role,'f12');wait(lambda:p.exists() and p.stat().st_mtime_ns!=old,'Screenshot missing');time.sleep(.1);Image.open(p).save(base/(name+'.png'))
def menu(role,prompt):
 # Diagnostics are sampled every 30 frames. Let the printer/menu settle before
 # another A press, so a newly opened menu is never selected by a stale sample.
 wait(lambda:val(role,'ui')==prompt,'Missing prompt '+str(prompt))
 for _ in range(20):
  if val(role,'choice_ready')==1:return
  start=val(role,'frame');wait(lambda:val(role,'frame')>=start+90,'Text printer stalled')
  if val(role,'ui')!=prompt:raise RuntimeError('Prompt advanced unexpectedly '+read(role))
  if val(role,'choice_ready')==1:return
  key(role,settle=60)
 raise RuntimeError('Missing native menu '+str(prompt)+' '+read(role))
def encoded(text):
 out=[]
 for c in text:
  out.append(0xbb+ord(c)-65 if 'A'<=c<='Z' else 0xd5+ord(c)-97 if 'a'<=c<='z' else {' ':0,'!':0xab,'\n':0xfe}[c])
 return bytes(out).hex()

def check_dialogue(role):
 rows=read(role,'battle-text.txt').splitlines();assert rows,'Missing native text trace'
 messages=[]
 for row in rows:
  assert ' player0=0:' in row and ' player1=1:' in row, 'Missing opposite native battler IDs: '+row
  raw=bytes.fromhex(re.search(r' text=([0-9a-f]+)',row)[1]);assert raw[-1]==0xff and len(raw)<=300,'Unterminated battle text'
  messages.append(raw.hex())
 peer='TEST B' if role=='a' else 'TEST A'
 assert any(encoded(peer+'\nwants to battle!') in message for message in messages),'Wrong opponent name in introduction'
 assert any(encoded(peer+' sent out\n') in message for message in messages),'Wrong opponent name in send-out dialogue'

def healthy_idle():return all(val(r,'safe')==1 and val(r,'party_ready')==1 for r in ['a','b'])
def wallet(role,amount):return val(role,'balance','wallet-check.txt')==amount and val(role,'held','wallet-check.txt')==0 and val(role,'dirty','wallet-check.txt')==0
print('Evidence: '+str(base),flush=True)
try:
 for role in ['a','b']:
  folder=base/role;folder.mkdir();(folder/'test-keys.txt').write_text('1 0x3ff 6000\n');log=(folder/'runtime.log').open('w')
  cmd=[str(root/'build'/args.configuration/'fr_game_harness.exe'),'--rom',str(Path(args.rom).resolve()),'--save',str(folder/'test.sav'),'--profile-dir',str(folder),'--name','Aster' if role=='a' else 'Leaf','--window','--test-ui','--test-report','--test-manual','--test-'+('host' if role=='a' else 'join'),str(port),'--load-state',str(root/'cache/runtime-check/cable-a.state'),'--fixture',('field-center-' if args.indoor else 'field-')+role,'--frames','100000']
  processes.append(subprocess.Popen(cmd,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW))
  if role=='a':wait(lambda:(folder/'test-ui-status.txt').exists(),'Host boot',120)
 wait(healthy_idle,'Ready field',120);before={r:val(r,'party_hash') for r in ['a','b']};positions={r:re.search(r'map=\S+ tile=\S+',read(r,'world-check.txt'))[0] for r in ['a','b']}
 (base/'before.txt').write_text(read('a')+read('b'))
 key('a');menu('a',1);capture('a','challenge')
 if args.case!='free':
  key('a',0x37f);key('a');wait(lambda:val('a','picker')>=0,'Wager amount')
  key('a',0x3ef);key('a',0x3ef);key('a',0x3bf);assert val('a','amount')==101
  capture('a','amount');key('a')
 else:key('a')
 menu('a',3);capture('a','terms');key('a');menu('b',4);capture('b','incoming')
 if args.case=='cancel':
  for _ in range(25):
   if val('a','safe')==1 and val('a','notice')==0:break
   key('a')
  key('a');menu('a',5);capture('a','cancel');key('a')
 elif args.case=='decline':key('b',0x37f);key('b')
 else:key('b')
 if args.case in ['decline','cancel']:
  for _ in range(35):
   if healthy_idle() and all(val(r,'memory')==0 and val(r,'notice')==0 for r in ['a','b']):break
   for r in ['a','b']:
    if val(r,'safe')!=1:key(r)
  assert healthy_idle() and all(wallet(r,3000) and val(r,'phase')==0 and val(r,'memory')==0 and val(r,'party_hash')==before[r] for r in ['a','b'])
 else:
  for _ in range(60):
   if all(val(r,'entered')==1 for r in ['a','b']):break
   for r in ['a','b']:
    if val(r,'phase')==0:key(r)
   time.sleep(.2)
  wait(lambda:all(val(r,'entered')==1 for r in ['a','b']),'Native battle start',90);capture('a','connected')
  wait(lambda: all(read(r,'battle-text.txt') for r in ['a','b']), 'Battle intro text')
  for r in ['a','b']:
   key(r,0x3ff,settle=160);capture(r,'intro-'+r)
  if args.case=='refund':ui('a','disconnect')
  expected=[3101,2899] if args.case=='win' else [3000,3000]
  for step in range(180):
   if all(val(r,'phase')==0 and wallet(r,v) for r,v in zip(['a','b'],expected)):break
   for r in ['a','b']:key(r,settle=20)
   if step%20==0:print('Battle step',step,flush=True)
  else:raise RuntimeError('Battle did not return with expected money')
  for r in ['a','b']:
   assert val(r,'party_hash')==before[r], 'Party changed after return'
   assert positions[r] in read(r,'world-check.txt'), 'Trainer moved after return'
   if args.case!='refund':check_dialogue(r)
   if args.case in ['win','refund']:
    expected_message=('You won P101!' if r=='a' else 'You lost your P101 wager.') if args.case=='win' else 'Your P101 wager was refunded.'
    wait(lambda:expected_message in read(r,'wallet-check.txt'),'Missing committed wager result')
    wait(lambda:val(r,'notice')!=0,'Missing native result message')
    key(r,0x3ff,settle=120)
   elif args.case=='free':assert 'You won P' not in read(r,'wallet-check.txt')
   capture(r,'returned-'+r)
 report='PASS '+args.case+(' indoors' if args.indoor else ' outdoors')+'\n'+read('a')+read('b')+read('a','wallet-check.txt')+read('b','wallet-check.txt')
 (base/'acceptance.txt').write_text(report);print(report,flush=True)
finally:
 for role in ['a','b']:
  if (base/role).exists():(base/role/'test-stop.txt').write_text('done')
 for p in processes:
  try:p.wait(timeout=15)
  except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)
