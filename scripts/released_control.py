from pathlib import Path
import sys,time,re,json
root=Path(__file__).resolve().parents[1];base=Path((root/'cache/released-active.txt').read_text());role=sys.argv[1];folder=base/role
sequence=int(time.time()*1000)
def write(name,text):
 p=folder/name;tmp=p.with_suffix('.input-tmp');tmp.write_text(text)
 for _ in range(100):
  try:tmp.replace(p);return
  except PermissionError:time.sleep(.01)
 raise RuntimeError('Input publish failed')
def frame():
 try:
  s=(folder/'released-check.txt').read_text();m=re.search('frame=([0-9]+)',s);return int(m[1]) if m else 0
 except OSError:return 0
for action in sys.argv[2:]:
 sequence+=1;before=frame();end=time.monotonic()+35
 if action=='shot':
  write('test-ui-input.txt',f'{sequence} f12\n')
  while time.monotonic()<end:
   try:
    if f'command={sequence} ' in (folder/'test-ui-status.txt').read_text():break
   except OSError:pass
   time.sleep(.03)
 elif action.startswith('walk:'):
  x,y=map(int,action[5:].split(','));write('test-world-command.txt',f'{sequence} walk {x:x} {y:x}\n')
 else:
  mask={'A':'0x3fe','B':'0x3fd','up':'0x3bf','down':'0x37f','left':'0x3df','right':'0x3ef','start':'0x3f7','wait':'0x3ff'}[action]
  write('test-keys.txt',f'{sequence} {mask} 8\n')
  while time.monotonic()<end:
   if frame()>before+60:break
   time.sleep(.03)
 log=base/'actions.jsonl'
 with log.open('a') as f:f.write(json.dumps({'role':role,'action':action,'frame':before})+'\n')
 print(role,action,'frame',frame(),flush=True)
