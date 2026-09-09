from pathlib import Path
import time,json,re,shutil,sys
root=Path(__file__).resolve().parents[1];base=Path((root/'cache/released-active.txt').read_text());out=base/('watch-'+str(int(time.time())));out.mkdir();seen=set();end=time.monotonic()+int(sys.argv[1] if len(sys.argv)>1 else 70)
with (out/'poses.jsonl').open('w') as log:
 while time.monotonic()<end:
  for role in ['a','b']:
   try:text=(base/role/'released-check.txt').read_text()
   except OSError:continue
   entries=re.findall(r'released=(\w+).*?phase=(\d+).*?map=(\d+),(\d+).*?tile=(\d+),(\d+).*?pixel=(\d+),(\d+)',text)
   log.write(json.dumps({'role':role,'time':time.time(),'entries':entries})+'\n')
   states=tuple((e[0],e[1],e[2],e[3]) for e in entries)
   if entries and (role,states) not in seen:
    seen.add((role,states));label=role+'-'+str(len(seen));(out/(label+'.txt')).write_text(text);print(label,str(states),flush=True)
    for name in ['live.bmp','native.bmp','world-check.txt','composition-check.txt']:
     try:shutil.copyfile(base/role/name,out/(label+'-'+name))
     except OSError:pass
  time.sleep(.12)
print(out,flush=True)
