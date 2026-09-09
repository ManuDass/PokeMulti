"""Check two real game traces; poses must match the sending ROM's sampled sprite."""
from pathlib import Path
import argparse,csv,json,math
parser=argparse.ArgumentParser();parser.add_argument('trace');args=parser.parse_args();base=Path(args.trace);report={}
for viewer,sender in [('a','b'),('b','a')]:
    rows=[list(map(int,r)) for r in csv.reader((base/viewer/'motion-render.csv').open()) if len(r)==13]
    sourceRows=[list(map(int,r)) for r in csv.reader((base/sender/'motion-source.csv').open()) if len(r)==9]
    sources={r[1]:r for r in sourceRows};matched=0
    for r in rows:
        if r[3] in sources:
            matched+=1;assert r[6:9]==sources[r[3]][6:9],('pose changed in transit',r,sources[r[3]])
    jumps=[];steps=[]
    for a,b in zip(rows,rows[1:]):
        dt=b[1]-a[1];distance=abs(a[4]-b[4])+abs(a[5]-b[5])
        if 0<dt<60:
            steps.append(distance)
            if distance>math.ceil(dt/16.74)+2:jumps.append((a,b))
    faces=sorted(set(r[6] for r in rows));assert faces==[1,2,3,4],('missing facing',faces)
    assert len(rows)>500 and matched>500;assert not jumps,('unexpected movement jump',jumps[:3])
    # A single walking field update cannot reverse 16 pixels when crossing a tile.
    badSource=[]
    for a,b in zip(sourceRows,sourceRows[1:]):
        if a[3] and b[3] and 0<b[0]-a[0]<=1:
            d=abs(a[4]-b[4])+abs(a[5]-b[5])
            if d>2:badSource.append((a,b))
    assert not badSource,('incoherent field snapshot',badSource[:3])
    report[viewer]={'rendered_samples':len(rows),'pose_matches':matched,'pose_mismatches':0,'directions':faces,'maximum_step_at_under_60ms':max(steps),'field_snapshot_jumps':0}
print(json.dumps(report,indent=2));(base/'verified.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
