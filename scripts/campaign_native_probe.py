"""Isolated native campaign acceptance clients; never opens the user's profile/save."""
from pathlib import Path
import subprocess,os,socket,time,json,uuid,argparse
ap=argparse.ArgumentParser();ap.add_argument('--reuse');ap.add_argument('--rewards',type=int,default=3);args=ap.parse_args()
root=Path(__file__).resolve().parents[1]
base=Path(args.reuse).resolve() if args.reuse else root/'cache'/('campaign-native-'+uuid.uuid4().hex)
assert base.is_relative_to((root/'cache').resolve()) and base.name.startswith('campaign-native-')
base.mkdir(exist_ok=True);(root/'cache/campaign-native-active.txt').write_text(str(base));processes=[]
if (base/'stop.txt').exists():(base/'stop.txt').unlink()
print('Evidence: '+str(base),flush=True)
sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
try:
 for role in ['a','b']:
  folder=base/role;folder.mkdir(exist_ok=True)
  for name in ['test-stop.txt','test-world-command.txt','test-ui-input.txt','world-check.txt','shared-world.txt','wallet-check.txt','link-check.txt','test-ui-status.txt']:
   if (folder/name).exists():(folder/name).unlink()
  (folder/'world.cfg').write_text('1 1\n');(folder/'test-keys.txt').write_text('1 0x3ff 6000\n')
  log=(folder/('cold.log' if args.reuse else 'runtime.log')).open('w')
  cmd=[str(root/'build/Release-0.22.1/fr_game_harness.exe'),'--rom',str(root/'Pokemon - Fire Red Version.zip'),'--save',str(folder/'test.sav'),'--profile-dir',str(folder),'--name','Aster' if role=='a' else 'Leaf','--window','--test-ui','--test-report','--test-manual','--test-rewards',str(args.rewards),'--test-'+('host' if role=='a' else 'join'),str(port),'--save-state',str(folder/'final.state'),'--frames','100000']
  if not args.reuse:cmd+=['--load-state',str(root/'cache/runtime-check/cable-a.state'),'--fixture','campaign-start']
  processes.append(subprocess.Popen(cmd,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW))
  if role=='a':
   for _ in range(2000):
    if (folder/('link-check.txt' if args.reuse else 'world-check.txt')).exists():break
    if processes[0].poll() is not None:raise RuntimeError('Native client exited')
    time.sleep(.05)
 (base/'pids.json').write_text(json.dumps([p.pid for p in processes]))
 while not (base/'stop.txt').exists():
  if any(p.poll() is not None for p in processes):raise RuntimeError('Native client exited')
  time.sleep(.1)
finally:
 for role in ['a','b']:
  if (base/role).exists():(base/role/'test-stop.txt').write_text('done\n')
 for p in processes:
  try:p.wait(timeout=12)
  except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)
