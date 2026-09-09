from campaign_native_control import *
wait(lambda:all('applied=1' in read(r) and frame(r)>180 for r in ['a','b']),'Campaign did not initialize')
assert all(progress(r,0x4057,True)==0 and progress(r,0x829)==0 for r in ['a','b'])
assert all('objectives=0' in read(r) for r in ['a','b']),'Unearned campaign milestone'
print('PASS: no Pokedex required; no unearned key items',flush=True)
command('b','campaign-fixture',1)
wait(lambda:'map=5,3 ' in read('b','world-check.txt') and 'locked=1' in read('b'),'Parcel scene did not start')
print('Parcel dialogue:',read('b').splitlines()[0],flush=True)
old=re.search(r'tile=(\d+,\d+)',read('a','world-check.txt'))[1]
key('a','0x3df',18)
wait(lambda:re.search(r'tile=(\d+,\d+)',read('a','world-check.txt'))[1]!=old,'Other player cannot move during parcel dialogue')
wait(lambda:progress('a',0x4057,True)==1 and progress('b',0x4057,True)==1,'Parcel collection was not shared',70,('b',))
settle();capture('parcel-collected')
print('PASS: one native parcel collection; spectator remains free',flush=True)
command('b','campaign-fixture',2)
wait(lambda:'map=4,3 ' in read('b','world-check.txt') and safe('b'),'Oak fixture did not arrive')
key('b','0x3bf',4);mark=frame('b');wait(lambda:frame('b')>mark+60,'Facing Oak stalled')
key('b')
wait(lambda:progress('a',0x4057,True)==2 and progress('b',0x4057,True)==2,'Oak parcel completion did not reach the campaign',110,('b',))
settle(seconds=80)
assert all(progress(r,0x829)==1 for r in ['a','b']),'Both trainers must receive Pokedex access'
assert 'map=3,1 ' in read('a','world-check.txt'),'Spectator must remain outside while Oak is handled'
capture('parcel-delivered')
print('PASS: native Oak delivery grants shared completion and Pokedex access',flush=True)
command('a','campaign-fixture',1)
wait(lambda:'map=5,3 ' in read('a','world-check.txt') and safe('a'),'Second trainer reached a repeated parcel cutscene')
mark=frame('a');wait(lambda:frame('a')>mark+180,'Second visit did not settle')
assert safe('a') and progress('a',0x4057)==2,'Clerk repeated the completed task'
capture('clerk-no-repeat')
print('PASS: second trainer enters Mart without repeating the parcel scene',flush=True)
(base/'parcel-verified.json').write_text(json.dumps({'checks':['story begins after personal starter','hidden Rocket drops are not rewards','native parcel collected once','spectator free during dialogue','native Oak delivery once','Pokedex access shared','second Mart visit does not repeat task']},indent=2))
