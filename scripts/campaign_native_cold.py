from campaign_native_control import *
def native_safe(r):return 'safe=1 ' in read(r,'wallet-check.txt')
marks={};deadline=time.monotonic()+150
while time.monotonic()<deadline:
 if all(native_safe(r) and 'applied=1' in read(r) for r in ['a','b']):break
 for r in ['a','b']:
  n=frame(r)
  if not native_safe(r) and n>marks.get(r,0)+60:marks[r]=n;key(r,duration=24)
 time.sleep(.04)
else:raise RuntimeError('Cold native saves did not load')
settle()
for r in ['a','b']:
 assert ('campaign='+(base/'a/campaigns/active.cfg').read_text().strip()) in read(r)
 assert progress(r,0x4057)==2 and progress(r,0x829)==1 and progress(r,0x238)==1
 assert progress(r,0x820)==1 and progress(r,0x254)==1
 assert 'tm39_count_at_least_one=1 tm39_count_at_least_two=0' in read(r)
 assert 'balance=4400 ' in read(r,'wallet-check.txt') and 'policy=0' in read(r,'wallet-check.txt')
capture('cold-campaign-resumed')
print('PASS: original native saves cold-loaded, campaign resumed, rewards once, personal money intact; sharing off',flush=True)
(base/'cold-verified.json').write_text(json.dumps({'checks':['same persistent host campaign','both native saves cold-loaded without savestate or fixtures','Pokedex and parcel retained','separate Boulder Badges and P4400 wallets','one TM39 each, HM02 retained','host changed optional reward policy to zero']},indent=2))
def arrive(r,fixture,mapname):
 command(r,'campaign-fixture',fixture)
 wait(lambda:('map='+mapname+' ') in read(r,'world-check.txt') and safe(r),'Could not reach fixture NPC',100,('a','b'))
 settle()
def talk(r):
 key(r,'0x3bf',4);n=frame(r);wait(lambda:frame(r)>n+60,'Facing stalled');key(r)
 wait(lambda:not safe(r),'Native NPC dialogue did not begin')
arrive('b',4,'11,7');talk('b')
wait(lambda:progress('a',0x23a)==1 and progress('b',0x23a)==1,'Essential Strength did not reach both trainers with sharing off',180,('a','b'))
settle(seconds=100);capture('essential-strength-sharing-off')
print('PASS: Warden handled once; HM Strength delivered to both with all optional reward sharing off',flush=True)
assert all(progress(r,0x250)==0 and 'masterball_count_at_least_one=0' in read(r) for r in ['a','b'])
arrive('b',7,'1,57');talk('b')
wait(lambda:progress('b',0x250)==1 and progress('a',0x250,True)==1,'Original Master Ball gift did not complete',180,('a','b'))
settle(seconds=100)
assert progress('a',0x250)==0 and 'masterball_count_at_least_one=0' in read('a')
assert 'masterball_count_at_least_one=1 masterball_count_at_least_two=0' in read('b')
arrive('a',7,'1,57');talk('a');settle(seconds=140)
assert progress('a',0x250)==0 and 'masterball_count_at_least_one=0' in read('a'),'Claimant-only gift duplicated by native NPC'
assert 'masterball_count_at_least_one=1 masterball_count_at_least_two=0' in read('b')
capture('claimant-only-masterball')
print('PASS: claimant-only Master Ball remained with Leaf after Aster spoke to the original giver',flush=True)
(base/'policy-verified.json').write_text(json.dumps({'checks':['essential HM Strength delivered with all optional sharing off','Master Ball collected once using original giver script','second trainer native script honors campaign claim without marking private receipt'],'fixture_note':'Native setup supplies Gold Teeth and positions trainers; Silph rescue proof is setup. Original gift conversations run normally.'},indent=2))
