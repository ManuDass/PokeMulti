from campaign_native_control import *
wait(lambda:progress('a',0x4057,True)==2 and progress('b',0x4057,True)==2,'Oak parcel completion did not reach the campaign',160,('b',))
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
