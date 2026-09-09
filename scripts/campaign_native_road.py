import campaign_native_control as c
from pathlib import Path
import subprocess,os,socket,time,json,uuid
root=Path(__file__).resolve().parents[1];base=root/'cache'/('campaign-road-'+uuid.uuid4().hex);folder=base/'a';folder.mkdir(parents=True);c.base=base
(folder/'world.cfg').write_text('1 0\n');(folder/'test-keys.txt').write_text('1 0x3ff 6000\n')
s=socket.socket();s.bind(('127.0.0.1',0));port=s.getsockname()[1];s.close()
log=(folder/'runtime.log').open('w')
cmd=[str(root/'build/Release-0.17.0/fr_game_harness.exe'),'--rom',str(root/'Pokemon - Fire Red Version.zip'),'--save',str(folder/'road.sav'),'--profile-dir',str(folder),'--name','Road test','--window','--test-ui','--test-report','--test-manual','--test-host',str(port),'--load-state',str(root/'cache/runtime-check/cable-a.state'),'--fixture','campaign-start','--frames','20000']
p=subprocess.Popen(cmd,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW)
try:
 c.wait(lambda:'applied=1' in c.read('a') and c.safe('a'),'Road campaign setup',80)
 c.command('a','campaign-fixture',8)
 c.wait(lambda:'tile=21,12 ' in c.read('a','world-check.txt') and c.safe('a'),'Road fixture failed',50)
 assert c.progress('a',0x4051)==0
 c.capture('road-blocked')
 c.command('a','story',0x4057,2)
 c.wait(lambda:c.progress('a',0x4051)==1 and c.progress('a',0x4051,True)==1 and 'tile=21,8 ' in c.read('a'),'Native old man did not move off the road',100,('a',))
 c.settle(('a',));c.capture('road-opened-without-map-reentry')
 assert 'tile=21,12 ' in c.read('a','world-check.txt')
 (base/'verified.json').write_text(json.dumps({'check':'Live original old-man transition updates position/graphics after campaign parcel proof without reentering map','fixture_note':'Parcel completion injected for projection test; full original two-client parcel scene tested separately.'},indent=2))
 print('PASS: native road reopened in place: '+str(base),flush=True)
finally:
 (folder/'test-stop.txt').write_text('done\n')
 try:p.wait(timeout=12)
 except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)
