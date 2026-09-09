from campaign_native_control import *
def badge(r):return 'story=2080 value=1' in read(r)
def money(r):return int(re.search(r'balance=(\d+)',read(r,'wallet-check.txt'))[1])
def talk(r,fixture,mapname):
 command(r,'campaign-fixture',fixture)
 wait(lambda:('map='+mapname+' ') in read(r,'world-check.txt') and safe(r),'Could not reach NPC',80,(r,))
 key(r,'0x3bf',4);mark=frame(r);wait(lambda:frame(r)>mark+60,'Facing stalled')
 key(r)
def gym(r):
 talk(r,3,'6,2')
 wait(lambda:badge(r),'Native Brock battle did not finish',300,(r,))
 settle(seconds=160)
 assert 'tm39_count_at_least_one=1 tm39_count_at_least_two=0' in read(r),read(r)
assert not badge('a') and not badge('b')
before=[money(r) for r in ['a','b']]
gym('b')
assert not badge('a'),'Spectator was awarded an unearned personal badge'
assert money('a')==before[0] and money('b')>before[1],'Gym winnings must remain personal'
assert 'tm39_count_at_least_one=1 tm39_count_at_least_two=0' in read('a'),'Shared TM was not delivered once'
capture('first-personal-gym')
print('PASS: Leaf earned Boulder Badge and money; Aster received only the shared TM',flush=True)
before=[money(r) for r in ['a','b']]
gym('a')
assert all(badge(r) for r in ['a','b'])
assert money('a')>before[0] and money('b')==before[1]
assert all('tm39_count_at_least_one=1 tm39_count_at_least_two=0' in read(r) for r in ['a','b']),'Duplicate Gym TM'
capture('both-personal-gyms')
print('PASS: Aster earned own badge and money with no duplicate shared TM',flush=True)
talk('b',6,'25,0')
wait(lambda:progress('a',0x238)==1 and progress('b',0x238)==1,'HM Fly did not reach both trainers',160,('a','b'))
settle(seconds=100);capture('shared-fly')
print('PASS: native HM giver delivered quest access to both trainers',flush=True)
(base/'careers-verified.json').write_text(json.dumps({'checks':['personal Gym Badges','individual native Gym winnings','automatic shared TM','native second Gym win prevents duplicate TM','HM Fly reaches both trainers']},indent=2))
