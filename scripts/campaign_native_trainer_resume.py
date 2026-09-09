from campaign_native_control import *
def money(r):return int(re.search(r'balance=(\d+)',read(r,'wallet-check.txt'))[1])
key('a','0x3bf',24);n=frame('a');wait(lambda:frame('a')>n+90,'Approaching trainer stalled')
assert 'tile=19,10 ' in read('a','world-check.txt')
before=[money(r) for r in ['a','b']];key('a')
wait(lambda:progress('a',0x574)==1,'Voluntary personal trainer challenge did not finish',240,('a','b'))
settle(seconds=100)
assert money('a')>before[0] and money('b')==before[1]
capture('ordinary-trainer-personal-challenge')
print('PASS: voluntary original battle awards the second trainer their own victory and money',flush=True)
(base/'trainers-verified.json').write_text(json.dumps({'checks':['original Janice battle commits shared clearance','other player has no native victory or prize yet','walking through shared defeated trainer sight does not ambush','talking initiates second original trainer battle','personal winnings and victories']},indent=2))
