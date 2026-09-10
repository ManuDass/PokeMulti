from pathlib import Path
import sys,time,re,json
test=Path(__file__).with_name('world_native_smoke.py')
sys.path.insert(0,str(test.parent));scope={'__file__':str(test),'__name__':'__main__'}
exec(compile(test.read_text(encoding='utf-8').split("\ntry:\n start('a')")[0],str(test),'exec'),scope);globals().update(scope)
def command(r,action,id=0,val=0):write(runtime[r]/'test-world-command.txt',f'{time.monotonic_ns()} {action} {id:x} {val:x}\n')
def mapis(r,g,n):return f'map={g},{n} ' in read(r,'world-check.txt') and safe(r)
try:
 start('a');start('b');command('a','fixture',0,11);command('b','fixture',0,9)
 wait(lambda:mapis('a',5,4) and mapis('b',3,1),'Center fixtures')
 time.sleep(.7);capture('inside');before=value('a','frame')
 key('a',0x37f,20,50);wait(lambda:mapis('a',3,1),'Native center exit');capture('outside')
 time.sleep(.5)
 poses={int(v[0]):list(map(int,v)) for l in read('a','follower-jumps.csv').splitlines() if len(v:=l.split(','))==9}
 scene={int(v[0]):list(map(int,v)) for l in read('a','follower-effects.csv').splitlines() if len(v:=l.split(','))==13}
 rows=[p for f,p in sorted(poses.items()) if f>before and f in scene and scene[f][1:3]==[3,1]]
 print('Exit poses (frame,trainer X/Y,elevation,height,follower X/Y,height,facing):',rows[:55],flush=True)
 (base/'door-poses.json').write_text(json.dumps(rows))
 assert len(rows)>30,'Missing native exit samples'
 door_x=rows[0][1];door_y=rows[0][2]//16*16
 assert all(p[5]==door_x and p[6]==max(door_y,p[2]-16) and p[8]==1 for p in rows),'Follower did not trail straight through doorway'
 assert all(abs(rows[i][6]-rows[i-1][6])<=rows[i][0]-rows[i-1][0] for i in range(1,len(rows))),'Follower exit snapped'
 fades=[list(map(int,l.split(','))) for l in read('a','fade-overlay.csv').splitlines() if l.count(',')==4]
 assert fades and all(v[4]==0 for v in fades),'Overlay leaked during door fade'
 print('PASS: native doorway anchor, forward-facing cardinal exit, no snapping and no overlays during fade',flush=True)
 print('Door evidence:',base,flush=True)
finally:
 for p in processes.values():
  if p.poll() is None:p.terminate()
 for p in processes.values():p.wait(10)
 for log in logs:log.close()
