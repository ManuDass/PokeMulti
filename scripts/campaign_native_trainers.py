from campaign_native_control import *
def money(r):return int(re.search(r'balance=(\d+)',read(r,'wallet-check.txt'))[1])
def arrive(r):
 command(r,'campaign-fixture',5)
 wait(lambda:'map=3,21 ' in read(r,'world-check.txt') and safe(r),'Could not reach Route 3',100,('a','b'))
 settle()
def talk(r):
 key(r,'0x3bf',4);n=frame(r);wait(lambda:frame(r)>n+60,'Facing stalled');key(r)
assert all(progress(r,0x574)==0 for r in ['a','b'])
before=[money(r) for r in ['a','b']]
arrive('b');talk('b')
wait(lambda:progress('b',0x574)==1 and progress('a',0x574,True)==1,'First original trainer battle did not complete',240,('a','b'))
settle(seconds=100)
assert progress('a',0x574)==0 and money('a')==before[0] and money('b')>before[1]
capture('ordinary-trainer-first-victory')
print('PASS: original Route 3 trainer victory recorded by campaign; spectator kept personal flag and money',flush=True)
arrive('a');key('a','0x37f',4);n=frame('a');wait(lambda:frame('a')>n+240,'Walking through trainer sight did not settle')
assert safe('a') and progress('a',0x574)==0,'Campaign-defeated trainer ambushed the other player'
assert 'tile=19,11 ' in read('a','world-check.txt'),read('a','world-check.txt')
capture('ordinary-trainer-no-repeat-ambush')
print('PASS: second trainer can walk through the defeated NPC sight without a repeat ambush',flush=True)
key('a','0x3bf',24);n=frame('a');wait(lambda:frame('a')>n+90,'Approaching trainer stalled')
assert 'tile=19,10 ' in read('a','world-check.txt')
before=[money(r) for r in ['a','b']];key('a')
wait(lambda:progress('a',0x574)==1,'Voluntary personal trainer challenge did not finish',240,('a','b'))
settle(seconds=100)
assert money('a')>before[0] and money('b')==before[1]
capture('ordinary-trainer-personal-challenge')
print('PASS: voluntary original battle awards the second trainer their own victory and money',flush=True)
(base/'trainers-verified.json').write_text(json.dumps({'checks':['original Janice battle commits shared clearance','other player has no native victory or prize yet','walking through shared defeated trainer sight does not ambush','talking initiates second original trainer battle','personal winnings and victories']},indent=2))
