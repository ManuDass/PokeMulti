from pathlib import Path
import subprocess,os,socket,time,json,uuid,sys
root=Path(__file__).resolve().parents[1];resume=Path(sys.argv[1]) if len(sys.argv)>1 else None;base=resume or root/'cache'/('released-native-'+uuid.uuid4().hex);base.mkdir(exist_ok=True);
for path in [base/'stop.txt',base/'a/test-stop.txt',base/'b/test-stop.txt']:
 if path.exists():path.unlink()
(root/'cache/released-active.txt').write_text(str(base));processes=[]
print('Evidence: '+str(base),flush=True)
sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
try:
 for role in ['a','b']:
  folder=base/role;folder.mkdir(exist_ok=True);(folder/'world.cfg').write_text('1 1\n');(folder/'test-keys.txt').write_text('1 0x3ff 60000\n');log=(folder/'runtime.log').open('w')
  cmd=[str(root/'build/Release-0.12.0/fr_game_harness.exe'),'--rom',str(root/'Pokemon - Fire Red Version.zip'),'--save',str(folder/'test.sav'),'--profile-dir',str(folder),'--name','Aster' if role=='a' else 'Leaf','--window','--test-ui','--test-report','--test-manual','--test-'+('host' if role=='a' else 'join'),str(port),'--load-state',str(root/'cache/runtime-check/cable-a.state'),'--fixture','released-'+role,'--frames','200000']
  if resume and (role=='a' or (folder/'test.sav').exists()):
   idx=cmd.index('--load-state');del cmd[idx:idx+4]
  processes.append(subprocess.Popen(cmd,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW))
  if role=='a':
   for _ in range(2000):
    if (folder/'world-check.txt').exists():break
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
