from pathlib import Path
import argparse,os,socket,subprocess,time,re,uuid
ap=argparse.ArgumentParser();ap.add_argument('--configuration',default='Release-0.22.1');args=ap.parse_args()
root=Path(__file__).resolve().parents[1];base=root/'cache'/('route-exit-'+uuid.uuid4().hex[:12]);base.mkdir();processes=[]
sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
def read(role,name='story-gate-check.txt'):
 try:return (base/role/name).read_text()
 except OSError:return ''
def val(role,field):
 m=re.search(r'\b'+field+r'=(\d+)',read(role));return int(m[1]) if m else -1
def wait(fn,why,limit=140):
 end=time.monotonic()+limit
 while time.monotonic()<end:
  if fn():return
  if any(p.poll() is not None for p in processes):raise RuntimeError('Client exited: '+str([p.poll() for p in processes])+'\n'+read('a','runtime.log')[-4000:]+'\n'+read('b','runtime.log')[-1000:])
  time.sleep(.04)
 raise RuntimeError(why+'\n'+read('a')+'\n'+read('b'))
def write(role,file,contents):
 path=base/role/file;temp=path.with_suffix('.tmp');temp.write_text(contents)
 for _ in range(100):
  try:temp.replace(path);return
  except PermissionError:time.sleep(.02)
 raise RuntimeError('Input locked')
def command(role,action,x,y):write(role,'test-world-command.txt',f'{time.monotonic_ns()} {action} {x:x} {y:x}\n')
def key(role,mask,duration=60):
 before=val(role,'frame');write(role,'test-keys.txt',f'{time.monotonic_ns()} {hex(mask)} {duration}\n');wait(lambda:val(role,'frame')>before+duration+60,'Key stalled')
print('Evidence: '+str(base),flush=True)
try:
 for role in ['a','b']:
  folder=base/role;folder.mkdir();(folder/'test-keys.txt').write_text('1 0x3ff 6000\n');log=(folder/'runtime.log').open('w')
  cmd=[str(root/'build'/args.configuration/'fr_game_harness.exe'),'--rom',str(root/'Pokemon - Fire Red Version.zip'),'--save',str(folder/'test.sav'),'--profile-dir',str(folder),'--name','Aster' if role=='a' else 'Leaf','--window','--test-ui','--test-report','--test-manual','--test-'+('host' if role=='a' else 'join'),str(port),'--load-state',str(root/'cache/runtime-check/cable-a.state'),'--fixture','campaign-start','--frames','100000']
  processes.append(subprocess.Popen(cmd,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW))
  if role=='a':wait(lambda:val('a','safe')==1,'Host boot')
 wait(lambda:all('applied=1' in read(role,'shared-world.txt') and val(role,'safe')==1 for role in ['a','b']),'Shared field')
 def exit_position(role):
  key(role,0x3ff,1);command(role,'walk',2,18)
  wait(lambda:'map=3,1 ' in read(role) and 'tile=2,18 ' in read(role) and val(role,'safe')==1,'Reach western exit')
 for role in ['a','b']:
  for trip in range(2):
   exit_position(role)
   (base/f'{role}-{trip}-before.txt').write_text(read(role,'shared-world.txt')+'\n'+read(role))
   key(role,0x3df,64);assert 'map=3,41 ' in read(role),'Did not enter Route 22'
   (base/f'{role}-{trip}-route.txt').write_text(read(role,'shared-world.txt')+'\n'+read(role))
   print(role,'west',trip,read(role),flush=True)
   key(role,0x3ef,80);assert 'map=3,1 ' in read(role),'Did not return to Viridian'
   print(role,'east',trip,read(role),flush=True)
 # Cross the same connection together after both maps already have room state.
 exit_position('a');exit_position('b')
 frames={role:val(role,'frame') for role in ['a','b']}
 for role in ['a','b']:write(role,'test-keys.txt',f'{time.monotonic_ns()} 0x3df 64\n')
 wait(lambda:all(val(role,'frame')>frames[role]+150 and 'map=3,41 ' in read(role) for role in ['a','b']),'Simultaneous route transition')
 for role in ['a','b']:
  assert 'Invalid local world state' not in read(role,'runtime.log')
  seq=time.monotonic_ns();write(role,'test-ui-input.txt',f'{seq} f12\n')
  wait(lambda:f'command={seq} phase=0 ' in read(role,'test-ui-status.txt'),'Capture route')
 (base/'acceptance.txt').write_text('PASS: host and guest each crossed west/east twice; simultaneous west crossing; no invalid world snapshots\n'+read('a')+read('b'))
 print('PASS: native Route 22 transitions',flush=True)

finally:
 for role in ['a','b']:
  if (base/role).exists():(base/role/'test-stop.txt').write_text('done')
 for p in processes:
  try:p.wait(timeout=15)
  except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)
