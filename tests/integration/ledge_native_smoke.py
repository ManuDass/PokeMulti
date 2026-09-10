from pathlib import Path
import sys,time,re,json
test=Path(__file__).with_name('world_native_smoke.py')
sys.path.insert(0,str(test.parent));scope={'__file__':str(test),'__name__':'__main__'}
source=test.read_text(encoding='utf-8').split("\ntry:\n start('a')")[0].replace("'field-'+r","'camp-route1' if r=='a' else 'camp-route1-b'")
exec(compile(source,str(test),'exec'),scope);globals().update(scope)
try:
 start('a');start('b');time.sleep(1);capture('before-ledge')
 print(read('a','world-check.txt'),flush=True)
 start_frame=value('a','frame');write(runtime['a']/'test-keys.txt',f'{time.monotonic_ns()} 0x37f 48\n')
 wait(lambda:any(len(v.split(','))==9 and int(v.split(',')[0])>start_frame and int(v.split(',')[7])<0 for v in read('a','follower-jumps.csv').splitlines()),'Follower jump height missing')
 capture('jump-arc')
 wait(lambda:value('a','frame')>=start_frame+160,'Ledge landing stalled');capture('after-ledge')
 print(read('a','world-check.txt'),flush=True)
 rows=[list(map(int,l.split(','))) for l in read('a','follower-jumps.csv').splitlines() if len(l.split(','))==9]
 airborne=[v for v in rows if v[4]<0 or v[7]<0]
 assert len(airborne)>20 and all(v[5]==128 and v[8]==1 for v in airborne),airborne
 assert all(abs(airborne[i][6]-airborne[i-1][6])<=airborne[i][0]-airborne[i-1][0] for i in range(1,len(airborne)))
 assert all(v[3]==3 for v in airborne),'Ledge changed the network scene plane'
 landing=next(v[2] for i,v in enumerate(rows[1:],1) if rows[i-1][4]<0 and v[4]==0)
 assert rows[-1][7]==0 and rows[-1][6]==landing,(rows[-1],landing)
 print('PASS: native ledge keeps cardinal path, faces down, uses jump arc and finishes at the native landing coordinate when trainer stops',flush=True)
finally:
 for p in processes.values():
  if p.poll() is None:p.terminate()
 for p in processes.values():p.wait(10)
 for log in logs:log.close()
