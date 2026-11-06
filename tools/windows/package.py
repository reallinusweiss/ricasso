"""Stage only the native game client, redistributable dependencies and guest SREs.
Original game libraries, APK contents and user saves are deliberately never inputs.

Package layout (end users only see two entries next to the app folder):
    Ricasso Windows x64/
        Ricasso.exe      no-terminal setup wizard + launcher (tools/windows/launcher)
        README.txt        English quick start
        app/              the client exactly as before (swordfare.exe, DLLs, libs/, ...)
"""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]

def main():
    prefix = Path(os.environ.get('SWORDIGO_TOOLCHAIN_PREFIX', ROOT / '.tools/msys2/ucrt64'))
    out = ROOT / 'dist-windows/Ricasso Windows x64'
    suffix = 1
    while out.exists():
        suffix += 1
        out = ROOT / ('dist-windows/Ricasso Windows x64-' + str(suffix))
    out.mkdir(parents=True, exist_ok=True)
    # Refuse stale contents so a prior local game-data copy cannot enter an archive.
    if any(out.iterdir()):
        raise SystemExit(f'Output must be empty. Move the old package aside first: {out}')
    root = out
    out = root / 'app'
    out.mkdir()
    starter = ROOT / 'tools/windows/launcher/bin/Ricasso.exe'
    if not starter.is_file():
        raise SystemExit('Missing starter. Build it first: tools/windows/launcher/build_launcher.ps1 (needs Go 1.22+).')
    def copy(src, relative):
        dst = out / relative
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        return dst
    exe = copy(ROOT / 'binw/swordfare.exe', 'swordfare.exe')
    subprocess.run([str(prefix/'bin/strip.exe'), '--strip-unneeded', str(exe)], check=True)
    queue = [exe, copy(prefix/'share/qt6/plugins/platforms/qwindows.dll', 'platforms/qwindows.dll')]
    available = {p.name.lower():p for p in (prefix/'bin').glob('*.dll')}
    system = Path(os.environ['SystemRoot'])/'System32'
    imports = {}
    done = set()
    while queue:
        binary = queue.pop()
        if binary.name.lower() in done:
            continue
        done.add(binary.name.lower())
        info = subprocess.check_output([str(prefix/'bin/objdump.exe'), '-p', str(binary)], text=True, errors='replace')
        if 'pei-x86-64' not in info:
            raise RuntimeError('Not Windows x64: ' + str(binary))
        deps = re.findall(r'DLL Name:\s*(\S+)', info)
        imports[binary.relative_to(out).as_posix()] = deps
        for dep in deps:
            name = dep.lower()
            if name in available:
                if name not in done and not (out/available[name].name).exists():
                    queue.append(copy(available[name], available[name].name))
            elif not (system/dep).exists() and not name.startswith(('api-ms-', 'ext-ms-')):
                raise RuntimeError('Missing runtime dependency: ' + dep)
    for name in ['libsre12.so', 'libsre13.so']:
        copy(ROOT/'binw/libs'/name, 'libs/'+name)
    for src, dst in [('WINDOWS_CLIENT.md','WINDOWS_CLIENT.md'), ('LICENSE.md','LICENSE.md'),
                     ('WINDOWS_CLIENT.md','README.md'), ('CREDITS.md','CREDITS.md'),
                     ('COPYING.GPL-3.0.txt','COPYING.GPL-3.0.txt'),
                     ('src/sre/LICENSE.md','licenses/project/SRE-LICENSE.md'),
                     ('src/platform/LICENSE.md','licenses/project/Platform-LICENSE.md'),
                     ('tools/windows/import_apkm.py','tools/windows/import_apkm.py'),
                     ('tools/windows/Start-Swordigo.cmd','Start-Swordigo.cmd'),
                     ('tools/windows/packages.lock.json','packages.lock.json')]:
        copy(ROOT/src,dst)
    (out/'qt.conf').write_text('[Paths]\nPlugins=.\n', encoding='utf-8')
    (out/'portable-client.txt').write_text('Local Windows ELF/JIT client; original game data supplied separately.\n')
    for folder in ['engine/v1.4.13/arm64-v8a','assets']:
        (out/folder).mkdir(parents=True,exist_ok=True)
    for src, dst in [(prefix/'share/licenses',out/'licenses/dependencies'),
                     (ROOT/'deps/dynarmic/LICENSE.txt',out/'licenses/Dynarmic.txt')]:
        if src.is_dir():shutil.copytree(src,dst,dirs_exist_ok=True)
        elif src.is_file():dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
    source_notice = ROOT/'SOURCE.md'
    if not source_notice.is_file():
        raise RuntimeError('Missing source and dependency notice: SOURCE.md')
    copy(source_notice, 'SOURCE.md')
    copy(ROOT/'tools/windows/launcher/README.md', 'licenses/project/Starter-README.md')
    copy(ROOT/'tools/windows/launcher/vendor/golang.org/x/sys/LICENSE', 'licenses/vendored/golang.org-x-sys/LICENSE')
    shutil.copy2(starter, root/'Ricasso.exe')
    shutil.copy2(ROOT/'tools/windows/README.txt', root/'README.txt')
    (out/'imports.json').write_text(json.dumps(imports,indent=2)+'\n')
    # Keep exact notices for bundled JIT dependencies in addition to MSYS2 DLLs.
    for folder in [ROOT/'deps/dynarmic/externals', ROOT/'src/imgui', ROOT/'src/stb', ROOT/'src/sre/base']:
        for p in folder.rglob('*'):
            if p.is_file() and (p.name.upper().startswith(('LICENSE', 'COPYING')) or p.name=='copyright'):
                copy(p, 'licenses/vendored/'+p.relative_to(ROOT).as_posix())
    # Hashes stay relative to app/ (import_apkm.py renames this file in its copies);
    # the two top-level files are listed with a ../ prefix.
    hashes={p.relative_to(out).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(out.rglob('*')) if p.is_file()}
    for name in ['Ricasso.exe', 'README.txt']:
        hashes['../'+name]=hashlib.sha256((root/name).read_bytes()).hexdigest()
    (out/'SHA256SUMS.json').write_text(json.dumps(hashes,indent=2)+'\n')
    archive=root.parent/'Ricasso-Windows-x64.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(root.rglob('*')):
            z.write(p,p.relative_to(root.parent))
    print('Package:',archive)
    (ROOT/'.tools').mkdir(exist_ok=True)
    (ROOT/'.tools/latest-package.txt').write_text(str(root), encoding='utf-8')
    print('Host DLLs:',len(done)-1,'; game data excluded')

if __name__ == '__main__':
    main()
