"""Import a user-provided Swordigo APKM into a separate local client directory.
Never places original files in the redistributable dist-windows archive.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import shutil
import zipfile

ROOT=Path(__file__).resolve().parents[2]
HASHES={
    '1.4.12':'f847814d1b6f81268567ed5ec2473fea4d4ee3b75d2c6fec7057227225e989f8',
    '1.4.13':'c2f567a34ec7bed9cf6b0af0ce88e9c925c11e5c2a2f2fbd8604461cf93aae7d',
}
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apkm',type=Path)
    args=parser.parse_args()
    with zipfile.ZipFile(args.apkm) as bundle:
        info=json.loads(bundle.read('info.json'))
        version=info.get('release_version')
        if info.get('pname')!='com.touchfoo.swordigo' or version not in HASHES:
            raise SystemExit('Unsupported package/version; supported: Swordigo 1.4.12/1.4.13.')
        files={}
        library=None
        for entry in bundle.infolist():
            if not entry.filename.endswith('.apk'):continue
            if entry.file_size>256*1024*1024:raise ValueError('APK too large')
            with zipfile.ZipFile(io.BytesIO(bundle.read(entry))) as apk:
                for item in apk.infolist():
                    name=item.filename
                    if item.is_dir():continue
                    if name=='lib/arm64-v8a/libswordigo.so':
                        value=apk.read(item)
                        if library is not None and library!=value:raise ValueError('Conflicting ARM64 libraries')
                        library=value
                    is_music=(name.startswith('res/raw/music_') and
                              PurePosixPath(name).suffix.lower() in {'.mp3','.ogg','.wav'})
                    if not name.startswith('assets/') and not is_music:continue
                    path=PurePosixPath(name)
                    if path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name:
                        raise ValueError('Invalid asset path')
                    if item.file_size>128*1024*1024:raise ValueError('Asset too large')
                    value=apk.read(item)
                    if name in files and files[name]!=value:raise ValueError('Conflicting asset: '+name)
                    files[name]=value
        if library is None or hashlib.sha256(library).hexdigest()!=HASHES[version]:
            raise SystemExit('Missing or unexpected ARM64 library hash; no import performed.')
        if not any(n in files for n in ['assets/resources/gamedata.gdata','assets/gamedata.gdata']):
            raise SystemExit('Game data marker missing.')
        if sum(map(len,files.values()))>512*1024*1024:raise ValueError('Asset tree too large')
    pointer=ROOT/'.tools/latest-package.txt'
    source=Path(pointer.read_text(encoding='utf-8')) if pointer.is_file() else ROOT
    if (source/'app'/'swordfare.exe').is_file():source=source/'app'  # package root with Ricasso.exe
    if not (source/'swordfare.exe').is_file() or not (source/'libs/libsre13.so').is_file():
        raise SystemExit('Build/package the source checkout first, or run this importer from the extracted Windows client ZIP.')
    output=ROOT/'play-windows'/('Swordigo '+version)
    if output.exists():raise SystemExit('Local installation already exists: '+str(output))
    shutil.copytree(source,output)
    hashes={}
    for name,value in files.items():
        # ZIPs can contain both X.scene and X.scene/<resources>. Windows cannot.
        parts=list(PurePosixPath(name).parts)
        for i in range(1,len(parts)):
            prefix='/'.join(parts[:i])
            if prefix in files:parts[i-1]+='_res'
        path=output.joinpath(*parts)
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(value)
        hashes[path.relative_to(output).as_posix()]=hashlib.sha256(value).hexdigest()
    libpath=output/'engine'/('v'+version)/'arm64-v8a/libswordigo.so'
    libpath.parent.mkdir(parents=True,exist_ok=True)
    libpath.write_bytes(library)
    hashes[libpath.relative_to(output).as_posix()]=HASHES[version]
    starter_path=ROOT/'tools/windows/Start-Swordigo.cmd'
    if not starter_path.is_file():starter_path=ROOT/'Start-Swordigo.cmd'
    starter=starter_path.read_text(encoding='utf-8')
    starter=starter.replace('v1.4.12','v'+version).replace('v1.4.13','v'+version)
    (output/'Start-Swordigo.cmd').write_text(starter,encoding='utf-8')
    record={'version':version,'apkm_sha256':hashlib.sha256(args.apkm.read_bytes()).hexdigest(),
            'asset_count':sum(n.startswith('assets/') for n in files),
            'music_count':sum(n.startswith('res/raw/') for n in files),
            'game_library_sha256':HASHES[version],
            'files':hashes,'note':'User-provided originals; local use only; excluded from distributable.'}
    (output/'game-files.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
    # The template manifest covers only files shipped by the clean package.
    (output/'SHA256SUMS.json').rename(output/'template-SHA256SUMS.json')
    (ROOT/'.tools').mkdir(exist_ok=True)
    (ROOT/'.tools/local-game.txt').write_text(str(output),encoding='utf-8')
    print('Local installation:',output)
    print('Version:',version,'assets:',len(files),'ARM64 SHA256:',HASHES[version])

if __name__=='__main__':main()
