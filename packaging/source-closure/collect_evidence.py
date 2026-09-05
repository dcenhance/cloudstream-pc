#!/usr/bin/env python3
"""Read-only upstream evidence collection; no runtime changes."""
import concurrent.futures, hashlib, json, pathlib, re, subprocess, tarfile, urllib.request
ROOT=pathlib.Path('/data/CloudStream-releases/0.1.0-preview.2/windows-source-support/source-closure')
ROOT.mkdir(parents=True,exist_ok=True)
def fetch(item):
    name,url=item
    p=ROOT/name; p.parent.mkdir(parents=True,exist_ok=True)
    try:
        with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'CloudStream-source-audit'}),timeout=80) as r: data=r.read()
        p.write_bytes(data)
        return {'path':name,'url':url,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
    except Exception as e: return {'path':name,'url':url,'error':str(e)}
def gh(name,endpoint):
    r=subprocess.run(['gh','api',endpoint],capture_output=True)
    (ROOT/name).parent.mkdir(parents=True,exist_ok=True)
    (ROOT/name).write_bytes(r.stdout)
    return {'path':name,'endpoint':endpoint,'exit_code':r.returncode,'stderr':r.stderr.decode()}
if __name__=='__main__':
    jobs=[('gyan-builds.html','https://www.gyan.dev/ffmpeg/builds/'),('msys2-mpv.html','https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-mpv'),('msys2-ffmpeg.html','https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-ffmpeg')]
    results=list(concurrent.futures.ThreadPoolExecutor(4).map(fetch,jobs))
    results += [gh('gyan-release-9.0.1.json','repos/GyanD/codexffmpeg/releases/tags/9.0.1'),gh('gyan-tree.json','repos/GyanD/codexffmpeg/git/trees/master?recursive=1'),gh('mpv-caches.json','repos/shinchiro/mpv-winbuild-cmake/actions/caches?per_page=100')]
    t=tarfile.open(ROOT.parent/'sources/mpv-winbuild-cd1edc11.tar.gz')
    for m in t.getmembers():
        if m.isfile():
            rel=pathlib.Path(m.name).relative_to(pathlib.Path(m.name).parts[0]); p=ROOT/'mpv-recipe'/rel; p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(t.extractfile(m).read())
    (ROOT/'initial-receipts.json').write_text(json.dumps(results,indent=2))
    for n in ['msys2-mpv.html','msys2-ffmpeg.html']:
        s=(ROOT/n).read_text()
        print(n,re.findall(r'https[^\"<> ]+(?:src.tar.zst|pkg.tar.zst)',s))
    print(json.dumps(results,indent=2))
