"""Native follower, fade and shiny acceptance in disposable host/guest worlds."""
from pathlib import Path
import os,time,sys,json,re,csv
test=Path(__file__).with_name('world_native_smoke.py')
sys.path.insert(0,str(test.parent));scope={'__file__':str(test),'__name__':'__main__'}
exec(compile(test.read_text(encoding='utf-8').split("\ntry:\n start('a')")[0],str(test),'exec'),scope)
globals().update(scope)
(world/'shiny-rate.cfg').write_text('1\n')
def command(r,action,id=0,val=0):
 before=value(r,'frame');write(runtime[r]/'test-world-command.txt',f'{time.monotonic_ns()} {action} {id:x} {val:x}\n');wait(lambda:value(r,'frame')>before+15,'Command did not advance')
def dismiss(r):
 for _ in range(18):
  if safe(r):return
  key(r,0x3fd,settle=25)
 raise RuntimeError('Notice did not finish')
def rows(r):
 try:return [list(map(int,line.split(','))) for line in read(r,'follower-effects.csv').splitlines() if line.count(',')==12]
 except ValueError:return []
def mapis(r,g,n):return f'map={g},{n} ' in read(r,'world-check.txt') and safe(r)
try:
 start('a');start('b');wait(lambda:len(rows('a'))>15,'Follower did not draw while standing still')
 command('a','party-shiny',77,1);time.sleep(.8)
 command('a','face-follower');key('a',settle=5);wait(lambda:any(line[9] for line in rows('a')),'X emote missing',10);capture('pet-small');dismiss('a')
 ui('a','click 1070 132');capture('options-follow')
 mark=value('a','frame');ui('a','click 809 310');time.sleep(.12);capture('recall');time.sleep(.8)
 assert any(r[0]>mark and r[3]==3 for r in rows('a')),'Settings did not animate recall'
 assert value('a','follower','overlay-check.txt')==0,'Follower setting did not turn off'
 mark=value('a','frame');ui('a','click 809 310');time.sleep(.12);capture('send-out');time.sleep(.8)
 assert any(r[0]>mark and r[3]==1 for r in rows('a')),'Settings did not animate send-out'
 ui('a','escape');command('a','party-size',0,2);command('a','party-shiny',1,0);time.sleep(.8)
 mark=value('a','frame');command('a','party-faint',0);time.sleep(1)
 assert any(r[0]>mark and r[3]==3 for r in rows('a')) and any(r[0]>mark and r[3]==1 for r in rows('a')),'Faint did not recall/send replacement'
 capture('faint-replacement')
 print('PASS: smaller native X emote, actual settings recall/send-out and faint replacement',flush=True)
 # Walk through the actual Pokemon Center exit instead of injecting a warp.
 command('a','fixture',0,11);wait(lambda:mapis('a',5,4),'Center fixture');key('a',0x37f,duration=20,settle=50)
 wait(lambda:mapis('a',3,1),'Center exit');capture('center-exit')
 # Step north from Route 1 into Viridian: native connection changes coordinates.
 command('a','fixture',0,10);wait(lambda:mapis('a',3,19),'Boundary fixture');time.sleep(.5)
 mark=value('a','frame');key('a',0x3bf,duration=20,settle=50);wait(lambda:mapis('a',3,1),'Route boundary');time.sleep(.5);capture('route-boundary')
 after=[r for r in rows('a') if r[0]>mark and r[1:3]==[3,1]]
 assert after and all(r[5] and r[3]==2 for r in after),'Connected route lost follower or repeated send-out'
 fades=[list(map(int,line.split(','))) for line in read('a','fade-overlay.csv').splitlines() if line.count(',')==4]
 assert fades and any(r[3] for r in fades) and all(r[4]==0 for r in fades),'Host overlay leaked into fade/black screen'
 print('PASS: native building exit, continuous connected-map follower, zero overlay changes during fades',flush=True)
 command('a','fixture',0,1);command('b','fixture',0,1)
 wait(lambda:all(mapis(r,3,19) and 'shiny=1' in read(r,'world-check.txt') for r in processes),'New map shiny wild')
 time.sleep(1);capture('shiny-wild-visible')
 ui('a','shiny-check');wait(lambda:'valid=1' in read('a','shiny-check.txt'),'Native forced shiny/non-shiny generation')
 (runtime['a']/'shiny-encounter.txt').unlink(missing_ok=True)
 command('a','chase');wait(lambda:(runtime['a']/'shiny-encounter.txt').exists(),'Visible shiny encounter',80)
 check=read('a','shiny-encounter.txt');assert 'field=1 battle=1' in check,check
 capture('shiny-encounter')
 (base/'follower-acceptance.json').write_text(json.dumps({'checks':['half-size nearest-neighbor emote','X interaction','setting recall/send-out','faint replacement','native Center exit','connected route persistence','no overlays during fade/black','shiny wild sprite and native encounter match'],'fade_frames':len(fades)},indent=2))
 print('PASS: visible shiny Pokemon enters battle with the same shiny identity',flush=True)
finally:
 for p in processes.values():
  if p.poll() is None:p.terminate();p.wait(10)
 for log in logs:log.close()
 print('Final follower evidence:',base,flush=True)
