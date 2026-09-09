"""Local UI/controller acceptance using recorded BLE input; no personal save writes."""
from pathlib import Path
import argparse,os,re,socket,subprocess,time,uuid,shutil
ap=argparse.ArgumentParser();ap.add_argument('--configuration',default='Release-0.23.1');ap.add_argument('--view-only',action='store_true');args=ap.parse_args()
root=Path(__file__).resolve().parents[2]
base=root/'cache'/('pokeball-ui-'+uuid.uuid4().hex[:12]);base.mkdir()
sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
log=(base/'runtime.log').open('w');process=None
def read(file='test-ui-status.txt'):
 try:return (base/file).read_text()
 except OSError:return ''
def val(field,file='test-ui-status.txt'):
 m=re.search(r'\b'+field+r'=(-?\d+)',read(file));return int(m[1]) if m else -1
def wait(fn,why,seconds=40):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if fn():return
  if process.poll() is not None:raise RuntimeError('Client exited: '+read('runtime.log')[-3000:])
  time.sleep(.015)
 raise RuntimeError(why+'\n'+read()+'\n'+read('story-gate-check.txt'))
def write(file,text):
 p=base/file;tmp=p.with_suffix('.tmp');tmp.write_text(text)
 for _ in range(50):
  try:tmp.replace(p);return
  except PermissionError:time.sleep(.01)
 raise RuntimeError('Locked test input')
def ui(op):
 seq=time.monotonic_ns();write('test-ui-input.txt',f'{seq} {op}\n')
 wait(lambda:f'command={seq} phase=0 ' in read(),'UI '+op)
def capture(name):
 ui('f12');shutil.copyfile(base/'game-ui.bmp',base/(name+'.bmp'));(base/(name+'.txt')).write_text(read())
def report(text):ui('ball-report "'+text+'"')
def tile():
 m=re.search(r'tile=(-?\d+),(-?\d+)',read('story-gate-check.txt'));return (int(m[1]),int(m[2])) if m else None
print('Evidence: '+str(base),flush=True)
try:
 (base/'world.cfg').write_text('1 0\n')
 cmd=[str(root/'build'/args.configuration/'fr_game_harness.exe'),'--rom',str(root/'Pokemon - Fire Red Version.zip'),'--save',str(base/'test.sav'),'--profile-dir',str(base),'--name','Aster','--window','--test-ui','--test-report','--test-manual','--test-host',str(port),'--load-state',str(root/'cache/runtime-check/cable-a.state'),'--fixture','campaign-start','--frames','100000']
 process=subprocess.Popen(cmd,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW)
 wait(lambda:val('safe','story-gate-check.txt')==1,'Boot',140)
 ui('controller-page');capture('controller-default')
 assert val('ball_model')==2462,'Original mesh missing'
 ui('resize 940 650');capture('controller-small')
 ui('resize 1140 760')
 if not args.view_only:
  # Start from the panel, with game input captured; movement must stay blocked.
  before=tile();report('00 00 00 07 6c');report('01 00 00 0c 6c');time.sleep(.25)
  assert tile()==before and val('ball_keys')==1023,'Options leaked controller input'
  # Same completion path as the real Connect button: no click on the game.
  ui('ball-connect');assert val('keyboard')==1 and val('ball_resume')==1
  report('02 00 00 07 6c');wait(lambda:val('keyboard')==0 and val('ball_armed')==1,'Connect failed to return game controls')
  capture('controller-connected-ready')
  report('03 00 00 0c 6c');wait(lambda:tile()!=before,'Native character did not move')
  report('04 00 00 07 6c');time.sleep(.3);capture('controller-moved')
  assert tile()[0]>before[0],'Stick moved in wrong direction'
  # Native Start menu through the real shell input callback.
  report('05 03 00 07 6c');time.sleep(.25);report('06 00 00 07 6c');capture('controller-start-menu')
  # B cancels the native menu; a new A hold cannot leak across losing focus.
  report('07 01 00 07 6c');time.sleep(.18);report('08 00 00 07 6c');time.sleep(.35)
  ui('chat');report('09 00 00 0c 6c');assert val('ball_keys')==1023,'Chat leaked stick movement'
  report('0a 00 00 07 6c');ui('escape');report('0b 00 00 07 6c');time.sleep(.1)
  report('0c 00 00 0c 6c');time.sleep(1.1);assert val('ball_keys')==1023,'Stale report held movement'
  report('0d 00 00 07 6c');report('0e 00 00 0c 6c');ui('ball-drop');assert val('ball_keys')==1023,'Disconnect held input'
  capture('controller-disconnected')
  # A later chat interaction cancels the pending return to the game.
  ui('ball-connect');ui('click 200 680');report('0f 00 00 07 6c');report('10 00 00 0c 6c')
  assert val('keyboard')==1 and val('chat_focus')==1 and val('ball_keys')==1023,'Connection stole chat focus'
  report('11 00 00 07 6c');ui('escape')
  # Completion while another window is active must not steal focus or move.
  ui('ball-connect');ui('ball-blur');report('12 00 00 07 6c');report('13 00 00 0c 6c')
  assert val('keyboard')==1 and val('ball_keys')==1023,'Connection stole another window focus'
  ui('ball-focus');ui('escape');report('14 00 00 07 6c');ui('controller-page');report('15 00 00 07 6c')
  capture('controller-paused')
  ui('resize 940 650');report('16 00 00 07 6c');capture('controller-small-paused')
  assert val('scroll')==0,'Paused controller view unnecessarily scrolls'
  ui('resize 1140 760');report('17 00 00 07 6c')
  ui('click '+str(val('ball_resume_x'))+' '+str(val('ball_resume_y')));report('18 00 00 07 6c')
  wait(lambda:val('keyboard')==0 and val('ball_armed')==1,'Resume button did not hand control back')
  capture('controller-resumed')
  wait(lambda:(base/'controller-status.txt').exists(),'Controller diagnostics missing')
  (base/'acceptance.txt').write_text('PASS: original mesh; Connect returns game focus without clicking viewport; native movement; Options/chat capture; Start chord; stale/disconnected input released; later chat/window actions retain focus\n'+read())
 print('PASS: controller UI'+(' preview' if args.view_only else ' and native controls'),flush=True)
finally:
 if process:
  (base/'test-stop.txt').write_text('done')
  try:process.wait(timeout=15)
  except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=5)
 log.close()
