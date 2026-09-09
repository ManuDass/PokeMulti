"""Development-only replay of the actual integrated ImGui widgets and Session calls."""
from pathlib import Path
import argparse,json,os,re,shutil,socket,subprocess,time,uuid
parser=argparse.ArgumentParser();parser.add_argument('--rom',required=True);parser.add_argument('--configuration',default='Release');args=parser.parse_args()
root=Path(__file__).resolve().parents[2];base=root/'cache'/('online-ui-v3-'+uuid.uuid4().hex);base.mkdir(parents=True)
processes=[];sequence=0

def status(role):
    try:return (base/role/'test-ui-status.txt').read_text()
    except OSError:return ''
def wait(predicate,message,seconds=25):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        if predicate():return
        if any(p.poll() is not None for p in processes):raise RuntimeError('Test process exited: '+str([p.poll() for p in processes]))
        time.sleep(.05)
    raise RuntimeError(message+'\n'+status('a')+'\n'+status('b'))
def write(path,text):
    temp=path.with_suffix('.tmp');temp.write_text(text,encoding='utf-8')
    for attempt in range(40):
        try:temp.replace(path);return
        except PermissionError:time.sleep(.025)
    raise RuntimeError('Cannot replace test command '+str(path))
def send(role,op,arguments=''):
    global sequence
    sequence+=1;write(base/role/'test-ui-input.txt',f'{sequence} {op} {arguments}\n')
    wait(lambda:f'command={sequence} phase=0 ' in status(role),'UI command failed: '+op);time.sleep(.1)
def click(role,x,y):send(role,'click',f'{x} {y}')
def fill(role,x,y,value):click(role,x,y);send(role,'text',json.dumps(value))
def capture(role,name):
    send(role,'f12');wait(lambda:(base/role/'game-ui.bmp').exists(),'Missing capture');time.sleep(.2);shutil.copyfile(base/role/'game-ui.bmp',base/name)
try:
    for role in ['a','b']:
        folder=base/role;folder.mkdir();log=open(folder/'runtime.log','w')
        command=[str(root/'build'/args.configuration/'fr_game_harness.exe'),'--rom',str(Path(args.rom).resolve()),'--save',str(folder/'test.sav'),'--profile-dir',str(folder),'--name','RED' if role=='a' else 'LEAF','--window','--test-ui','--test-report','--frames','100000']
        processes.append(subprocess.Popen(command,cwd=root,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW))
    wait(lambda:'size=1140,760' in status('a') and 'size=1140,760' in status('b'),'UI did not open')
    sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close();key='interface-test-123'
    for role in ['a','b']:
        fill(role,920,328,key);fill(role,920,394,str(port));assert f'port={port} key={key}' in status(role);assert 'keyboard=1' in status(role)
    click('a',920,442);wait(lambda:'hosting=1' in status('a'),'Host failed')
    click('b',1000,260);fill('b',920,328,'127.0.0.1');click('b',920,512)
    wait(lambda:'peers=2' in status('a') and 'peers=2' in status('b'),'Join failed');capture('a','connected.bmp')
    click('a',930,394);click('a',930,454)
    wait(lambda:(base/'a/friends.cfg').exists() and 'LEAF' in (base/'a/friends.cfg').read_text(),'Friend was not persisted')
    click('a',949,133);capture('a','friends.bmp')
    click('a',300,300);wait(lambda:'keyboard=0' in status('a'),'Game focus did not resume')
    send('a','f2');wait(lambda:'sidebar=0' in status('a'),'Sidebar did not hide');capture('a','focus.bmp')
    send('a','f2');send('a','resize','940 650');capture('a','compact.bmp');assert 'size=940,650' in status('a')
    send('a','resize','1140 760')
    click('a',1030,134);capture('a','options.bmp');click('a',850,258);click('a',850,308)
    wait(lambda:(base/'a/world.cfg').exists() and (base/'a/world.cfg').read_text().strip()=='0 0','World toggles did not persist')
    click('a',840,134);click('a',890,498)
    wait(lambda:'invitation=0' in status('b'),'Battle invitation missing');capture('b','invitation.bmp')
    click('b',940,277);wait(lambda:'invitation=-1' in status('b'),'Decline failed');assert 'cable=0' in status('a')
    click('a',1020,498);wait(lambda:'invitation=0' in status('b'),'Trade invitation missing');click('b',940,234)
    wait(lambda:'cable=1' in status('a') and 'cable=1' in status('b'),'Accept did not connect cable');capture('a','cable.bmp')
    click('a',940,596);wait(lambda:'cable=0' in status('a') and 'cable=0' in status('b'),'Disconnect cable failed')
    click('b',940,582);wait(lambda:'peers=1' in status('a') and 'hosting=0 peers=0' in status('b'),'Leave failed')
    click('a',945,134);capture('a','friend-offline.bmp');click('a',842,330)
    wait(lambda:(base/'a/friends.cfg').read_text().strip()=='','Remove friend failed')
    print('PASS: Host/Join, text focus, friend add/remove/presence, resizing, sidebar, settings, invite decline/accept, cable disconnect and Leave.',flush=True)
    print('Captures: '+str(base),flush=True)
finally:
    for role in ['a','b']:
        if (base/role).exists():write(base/role/'test-stop.txt','done\n')
    for p in processes:
        try:p.wait(timeout=15)
        except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)
