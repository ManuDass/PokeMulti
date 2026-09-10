from pathlib import Path
import sys,time,re,json
test=Path(__file__).with_name('world_native_smoke.py')
sys.path.insert(0,str(test.parent));scope={'__file__':str(test),'__name__':'__main__'}
exec(compile(test.read_text(encoding='utf-8').split("\ntry:\n start('a')")[0],str(test),'exec'),scope);globals().update(scope)
def command(r,action):write(runtime[r]/'test-world-command.txt',f'{time.monotonic_ns()} {action} 0 0\n')
try:
 start('a');start('b');command('a','world-save')
 wait(lambda:all(value(r,'ack','world-save-check.txt')>0 for r in processes),'Initial private saves')
 before=value('b','ack','world-save-check.txt');request=value('b','request','world-save-check.txt');expected={r:identity(r) for r in processes}
 command('b','face-follower');time.sleep(.2);key('b',settle=15)
 wait(lambda:not safe('b'),'Guest follower dialogue did not open')
 command('a','world-save');wait(lambda:value('b','request','world-save-check.txt')>request and value('a','checkpoint_waiting','test-ui-status.txt',True)==1,'Host did not wait for busy guest')
 assert value('b','ack','world-save-check.txt')==before
 capture('save-waits-for-dialogue')
 for _ in range(15):
  key('b',0x3fd,settle=25)
  if safe('b'):break
 wait(lambda:value('b','ack','world-save-check.txt')>before and value('a','checkpoint_waiting','test-ui-status.txt',True)==0,'Busy guest did not save after dialogue')
 assert all((world/'players'/ids[r]/'checkpoint.pmsv').exists() for r in processes)
 print('PASS: host save waits for guest dialogue then commits separate private trainer files',flush=True)
 ui('a','click 1070 132');capture('end-session-options')
 # Actual button coordinates in the default General composition.
 ui('a','click 950 530')
 wait(lambda:value('a','solo','test-ui-status.txt',True)==1 and processes['b'].poll() is not None,'End session button did not keep host solo and return guest',25,False)
 assert processes['a'].poll() is None and processes['b'].returncode==0 and (base/'b'/'world-return.txt').exists()
 processes.pop('b');ui('a','click 850 132');capture('host-keeps-playing-solo')
 assert value('a','scroll','test-ui-status.txt',True)==0
 before=identity('a');key('a',0x3df,16,50);assert identity('a')['position']!=before['position'],'Host cannot walk after ending session'
 old=value('a','ack','world-save-check.txt');command('a','world-save');wait(lambda:value('a','ack','world-save-check.txt')>old,'Host cannot manually save after ending session')
 # Exit world is the single action in the solo Room pane, under its save reminder.
 write(base/'a'/'test-ui-input.txt',f'{time.monotonic_ns()} click 950 322\n')
 wait(lambda:processes['a'].poll() is not None,'Exit world did not close the running game',20,False)
 assert processes['a'].returncode==0
 print('PASS: End session disconnects guest while host stays playable and saveable; Exit world closes game',flush=True)
 (base/'acceptance.json').write_text(json.dumps({'checks':['host saves guest after dialogue','private trainer world files','End session keeps host playing','guest returns to launcher','solo host movement and manual save','Exit world closes game']},indent=2))
finally:
 for p in processes.values():
  if p.poll() is None:p.terminate()
 for p in processes.values():p.wait(10)
 for log in logs:log.close()
