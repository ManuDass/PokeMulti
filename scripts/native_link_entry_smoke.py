"""Regression: original two-player Cable Club entry remains available with shared story."""
from pathlib import Path
import subprocess,os,time,socket,json,re,uuid,shutil,argparse
ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True);ap.add_argument('--configuration',default='Release-0.6.0');a=ap.parse_args()
r=Path(__file__).resolve().parents[1];base=r/'cache'/('native-link-0.6.0-'+uuid.uuid4().hex);base.mkdir();runs=[];print('Evidence: '+str(base),flush=True)
def read(role):
 try:return (base/role/'link-check.txt').read_text(encoding='utf-8')
 except OSError:return ''
try:
 sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
 for role in ['a','b']:
  p=base/role;p.mkdir();(p/'test-keys.txt').write_text('1 0x3ff 0\n');log=(p/'runtime.log').open('w')
  cmd=[str(r/'build'/a.configuration/'fr_game_harness.exe'),'--rom',str(Path(a.rom).resolve()),'--save',str(p/'test.sav'),'--profile-dir',str(p),'--name','Link '+role,'--load-state',str(r/'cache/runtime-check'/('prelink-'+role+'.state')),'--window','--test-report','--test-'+('host' if role=='a' else 'join'),str(port),'--frames','30000']
  runs.append(subprocess.Popen(cmd,cwd=r,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW))
 end=time.monotonic()+150;marks={'a':0,'b':0};sequence=2
 while time.monotonic()<end:
  states=[read(role) for role in ['a','b']]
  if all('object=1 xy=5,8' in s and 'object=2 xy=6,8' in s for s in states):
   for role in ['a','b']:
    for name in ['link-check.txt','live.bmp','native.bmp']:shutil.copyfile(base/role/name,base/(role+'-'+name))
   (base/'verified.json').write_text(json.dumps({'clients':2,'native_trade_center_entered':True,'native_link_owns_scene':True},indent=2));print('PASS: both native clients entered the original Trade Center',flush=True);break
  if any(p.poll() is not None for p in runs):raise RuntimeError('Native client exited unexpectedly')
  for role,s in zip(['a','b'],states):
   frame=re.search(r'frame=(\d+)',s)
   if frame and int(frame[1])>=marks[role]+60:
    sequence+=1;marks[role]=int(frame[1]);target=base/role/'test-keys.txt';tmp=target.with_suffix('.tmp');tmp.write_text(f'{sequence} 0x3fe 6\n')
    try:tmp.replace(target)
    except PermissionError:pass
  time.sleep(.04)
 else:raise RuntimeError('Cable Club entry timed out\n'+read('a')+'\n'+read('b'))
finally:
 for role in ['a','b']:
  if (base/role).exists():(base/role/'test-stop.txt').write_text('done\n')
 for p in runs:
  try:p.wait(timeout=12)
  except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)
