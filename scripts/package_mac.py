#!/usr/bin/env python3
"""Stage only CMake installation files; create an ad-hoc-signed Mac preview."""
import hashlib,json,re,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
arch=sys.argv[1]
assert arch in ('arm64','x86_64')
version=re.search(r'project\(PokeMulti VERSION ([0-9.]+)',(root/'CMakeLists.txt').read_text()).group(1)
output=root/'dist';stage=output/('mac-'+arch);app=stage/'PokeMulti.app'
subprocess.run(['cmake','--install',str(root/'build/mac'),'--prefix',str(stage)],check=True)
manifest=[]
for path in sorted(app.rglob('*')):
    if not path.is_file():continue
    relative=path.relative_to(app).as_posix()
    if re.search(r'(?i)\.(gba|gb|gbc|rom|sav|pmsv|state|key)$',relative) or any(p in ('worlds','native-cache','guest-cache','userdata') for p in path.parts):raise RuntimeError('Private data in package: '+relative)
    data=path.read_bytes()
    if len(data)>192 and data[172:176] in (b'BPRE',b'BPGE') and data[178]==0x96:raise RuntimeError('ROM content detected')
    manifest.append(dict(path=relative,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
for executable in ('pokemulti','pokemulti_game'):
    file=app/'Contents/MacOS'/executable
    deps=subprocess.check_output(['otool','-L',str(file)],text=True)
    for line in deps.splitlines()[1:]:
        dep=line.strip().split(' (')[0]
        if not dep.startswith(('/System/Library/','/usr/lib/')):raise RuntimeError('Unbundled dependency: '+dep)
    if executable=='pokemulti_game':subprocess.run(['codesign','--force','--sign','-','--identifier','com.manudass.pokemulti.game',str(file)],check=True)
subprocess.run(['codesign','--force','--sign','-',str(app)],check=True)
subprocess.run(['codesign','--verify','--deep','--strict',str(app)],check=True)
subprocess.run([str(app/'Contents/MacOS/pokemulti'),'--smoke-test','--data-dir',str(root/'build/mac/bundle-smoke')],check=True,timeout=30)
# A real .app, with all libraries supplied by macOS, requires no Xcode or Python.
name='PokeMulti-'+version+'-macOS-'+arch
zipfile=output/(name+'.zip')
subprocess.run(['ditto','-c','-k','--keepParent',str(app),str(zipfile)],check=True)
if not (stage/'Applications').is_symlink():(stage/'Applications').symlink_to('/Applications',target_is_directory=True)
(stage/'READ ME.txt').write_text('Drag PokeMulti to Applications, then open it.\n\nThis is an unsigned Mac preview. If macOS blocks it, use System Settings > Privacy & Security > Open Anyway for this app. Do not disable Gatekeeper.\n\nNo ROM is included. Select your own English FireRed or LeafGreen US 1.0 or 1.1 .gba or ZIP when the launcher opens.\n\nYour worlds are stored in ~/Library/Application Support/PokeMulti.\n\nThis preview uses the interpreter and supports keyboard/SDL controllers. Poké Ball Plus Bluetooth and automatic Mac updates are not implemented yet.\n',encoding='utf-8')
subprocess.run(['hdiutil','create','-volname','PokeMulti','-srcfolder',str(stage),'-ov','-format','UDZO',str(output/(name+'.dmg'))],check=True)
print('Packaged',len(manifest),'audited files for',arch)
