"""Fetch the locked, SHA-256 verified native UCRT64 toolchain into .tools only.
Requires Python 3.9+, Windows curl.exe and 7-Zip. No admin rights/MSYS required.
"""
import concurrent.futures
import hashlib
import json
import pathlib
import shutil
import subprocess
import tarfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOLS = ROOT / '.tools'

def main():
    seven = shutil.which('7z') or r'C:\Program Files\7-Zip\7z.exe'
    if not pathlib.Path(seven).is_file():
        raise SystemExit('Install 7-Zip or add 7z.exe to PATH.')
    packages = json.loads(pathlib.Path(__file__).with_name('packages.lock.json').read_text())
    cache = TOOLS / 'packages'
    dest = TOOLS / 'msys2'
    cache.mkdir(parents=True, exist_ok=True)
    dest.mkdir(parents=True, exist_ok=True)
    def fetch(p):
        f = cache / p['filename']
        if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest() != p['sha256sum']:
            subprocess.run(['curl.exe', '-fL', '--retry', '3', '--connect-timeout', '20',
                            '--max-time', '300', 'https://repo.msys2.org/mingw/ucrt64/' + f.name,
                            '-o', str(f)], check=True, stdout=subprocess.DEVNULL)
        if hashlib.sha256(f.read_bytes()).hexdigest() != p['sha256sum']:
            raise RuntimeError('SHA-256 mismatch: ' + f.name)
        return f
    links = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        for f in pool.map(fetch, packages):
            print('Extracting', f.name, flush=True)
            proc = subprocess.Popen([seven, 'x', '-so', str(f)], stdout=subprocess.PIPE,
                                    stderr=subprocess.DEVNULL)
            try:
                with tarfile.open(fileobj=proc.stdout, mode='r|') as archive:
                    for m in archive:
                        if not m.name.startswith('ucrt64/'):
                            continue
                        target = (dest / m.name).resolve()
                        if not target.is_relative_to(dest):
                            raise RuntimeError('Invalid package path')
                        if m.isdir():
                            target.mkdir(parents=True, exist_ok=True)
                        elif m.isfile():
                            target.parent.mkdir(parents=True, exist_ok=True)
                            with target.open('wb') as out:
                                shutil.copyfileobj(archive.extractfile(m), out)
                        elif m.islnk() or m.issym():
                            source = (dest / m.linkname if m.islnk() else target.parent / m.linkname).resolve()
                            if not source.is_relative_to(dest):
                                raise RuntimeError('Invalid package link')
                            links.append((source, target))
                # Consume tar padding before waiting; otherwise a full pipe can deadlock.
                proc.stdout.read()
                if proc.wait() != 0:
                    raise RuntimeError('7-Zip decompression failed')
            finally:
                proc.stdout.close()
                if proc.poll() is None:
                    proc.kill()
    for source, target in links:
        if source.is_file():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
        else:
            raise RuntimeError('Unresolved package link: ' + str(source))
    print('Ready:', dest / 'ucrt64')

if __name__ == '__main__':
    main()
