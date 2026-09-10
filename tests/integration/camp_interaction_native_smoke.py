from pathlib import Path
from collections import deque
import time,sys,re
test=Path(__file__).with_name('world_native_smoke.py')
sys.path.insert(0,str(test.parent));scope={'__file__':str(test),'__name__':'__main__'}
exec(compile(test.read_text(encoding='utf-8').split("\ntry:\n start('a')")[0],str(test),'exec'),scope);globals().update(scope)
def command(r,action,id=0,val=0):
 write(runtime[r]/'test-world-command.txt',f'{time.monotonic_ns()} {action} {id:x} {val:x}\n')
def position():
 return tuple(map(int,re.search(r'tile=(-?\d+),(-?\d+)',read('a','world-check.txt')).groups()))
def ground():
 text=read('a','camp-check.txt');x,y=map(int,re.search(r'camp=0 .*?tile=(\d+),(\d+)',text).groups());rows=list(map(int,re.search(r'^ground=0,([0-9,]+)',text,re.M)[1].split(',')))
 return {(x-4+col,y-4+row) for row,bits in enumerate(rows) for col in range(11) if bits&(1<<col)}
def mons():return [tuple(map(int,m)) for m in re.findall(r'mon=0 .*?pixel=(\d+),(\d+)',read('a','camp-check.txt'))]
def records(r):
 return [tuple(map(int,line.split(','))) for line in read(r,'camp-emotes.csv').splitlines() if len(line.split(','))==7]
def dismiss():
 key('a',0x3fd,settle=65)
 for _ in range(18):
  if safe('a'):return
  key('a',0x3fd,settle=25)
 raise RuntimeError('Camp dialogue did not close')
try:
 start('a');start('b');command('a','fixture',0,6);command('b','fixture',0,7)
 wait(lambda:all('lawn=1' in read(r,'camp-check.txt') for r in ['a','b']),'Camp fixtures did not reach grass')
 ui('a','click 1070 132');ui('a','click 809 310')
 wait(lambda:value('a','follower','overlay-check.txt')==0,'Follower toggle did not turn off')
 ui('a','click 410 360');ui('a','camp');wait(lambda:all('camp=0 ' in read(r,'camp-check.txt') and 'party=6' in read(r,'camp-check.txt') for r in ['a','b']) and 'pending=0' in read('a','camp-check.txt'),'Camp did not synchronize')
 dismiss();seen=set();area=ground()
 for attempt in range(80):
  points=mons();pos=position();px,py=pos[0]*16,pos[1]*16
  available=[i for i in range(len(points)) if i not in seen];i=min(available,key=lambda j:abs(points[j][0]-px)+abs(points[j][1]-py));mx,my=points[i]
  before={(row[1],row[2],row[3],row[4]) for row in records('a')}
  if abs(mx-px)+abs(my-py)<=29:
   command('a','face-camp',i);time.sleep(.09)
   write(runtime['a']/'test-keys.txt',f'{time.monotonic_ns()} 0x3fe 4\n')
   deadline=time.monotonic()+1.1
   while time.monotonic()<deadline and not any((row[1],row[2],row[3],row[4]) not in before for row in records('a')):time.sleep(.02)
   added=[row for row in records('a') if (row[1],row[2],row[3],row[4]) not in before]
   if added:
    event=added[0];member=event[2];kind=event[3];seq=event[4]
    capture('camp-pet-'+str(member))
    wait(lambda:any(row[1:5]==event[1:5] for row in records('b')),'Visitor did not render the same camp reaction')
    seen.add(member);print('Native X camp member',member,'reaction',kind,'seen on both clients',flush=True)
    dismiss();time.sleep(.4)
    if len(seen)>=2:break
    continue
   if not safe('a'):dismiss()
  # Walk one native step along the connected lawn toward that roaming member.
  target=min(area,key=lambda p:abs(p[0]*16-mx)+abs(p[1]*16-my));parents={pos:None};queue=deque([pos])
  while queue and target not in parents:
   p=queue.popleft()
   for dx,dy in [(0,1),(0,-1),(-1,0),(1,0)]:
    q=p[0]+dx,p[1]+dy
    if q in area and q not in parents:parents[q]=p;queue.append(q)
  if target in parents and target!=pos:
   step=target
   while parents[step]!=pos:step=parents[step]
   key('a',{(0,1):0x37f,(0,-1):0x3bf,(-1,0):0x3df,(1,0):0x3ef}[(step[0]-pos[0],step[1]-pos[1])],duration=12,settle=55)
  else:time.sleep(.25)
 assert len(seen)>=2,'Could not interact with two distinct roaming camp members'
 assert 'active=1' in read('a','camp-check.txt') and value('a','follower','overlay-check.txt')==0
 capture('camp-reactions-finished');ui('a','camp');wait(lambda:all('camp=0 ' not in read(r,'camp-check.txt') for r in ['a','b']),'Packing did not clear camp')
 print('PASS: native X interactions with distinct camp party members, follower disabled, synchronized reactions and camp removal.',flush=True)
finally:
 for p in processes.values():
  if p.poll() is None:p.terminate();p.wait(10)
 for log in logs:log.close()
 print('Camp interaction evidence:',base,flush=True)
