#!/usr/bin/env python3
"""Additional binary-version evidence and replacement-package inspection."""
from collect_evidence import ROOT,fetch,gh
from collect_sources import collect
import concurrent.futures,hashlib,json,pathlib,re,subprocess,tarfile,io
runtime=ROOT.parent.parent/'windows-runtime'
records=[]
for binary in ['ffmpeg.exe','libmpv-2.dll']:
    data=(runtime/binary).read_bytes()
    matches=sorted(set(x.decode() for x in re.findall(rb'[ -~]{6,}',data) if re.fullmatch(rb' ?r\d{4} [0-9a-f]{7,}|\d+\.\d+\+\d+-[a-f0-9]+',x)))
    records.append({'binary':binary,'sha256':hashlib.sha256(data).hexdigest(),'version_strings':matches})
(ROOT/'binary-codec-versions.json').write_text(json.dumps(records,indent=2))
rows=[]
for name,repo,ref,binaries in [('x264-both','mirror/x264','0480cb0',['ffmpeg.exe','libmpv-2.dll']),('x265-gyan','Multicorewareinc/x265','9ddc216',['ffmpeg.exe']),('x265-mpv','Multicorewareinc/x265','98eec40',['libmpv-2.dll'])]:
    rows.append({'package':name,'repo':repo,'ref':ref,'fields':{'URL_HASH':[]},'basis':'embedded codec version string; upstream snapshot identified, distributor patches/configuration not independently established','binaries':binaries})
with concurrent.futures.ThreadPoolExecutor(3) as p: rows=list(p.map(collect,rows))
(ROOT/'codec-source-evidence.json').write_text(json.dumps(rows,indent=2))
for f in (ROOT/'msys2-candidate').glob('*.tar.zst'):
    raw=subprocess.check_output(['zstd','-dc',str(f)])
    with tarfile.open(fileobj=io.BytesIO(raw)) as t:
        for m in t.getmembers():
            if not m.isfile():continue
            rel=pathlib.PurePosixPath(m.name)
            if '..' in rel.parts or rel.is_absolute():raise ValueError('unsafe archive')
            if rel.name in ['.PKGINFO','.BUILDINFO','PKGBUILD','.SRCINFO'] or rel.name.endswith('.patch') or '/licenses/' in m.name or m.name in ['ucrt64/bin/libmpv-2.dll','ucrt64/bin/ffmpeg.exe']:
                dest=ROOT/'msys2-inspection'/f.name/rel;dest.parent.mkdir(parents=True,exist_ok=True);stream=t.extractfile(m)
                if stream is not None:dest.write_bytes(stream.read())
                if dest.suffix in ['.dll','.exe']:
                    r=subprocess.run(['objdump','-p',str(dest)],capture_output=True,text=True)
                    (dest.parent/(dest.name+'.imports.txt')).write_text('\n'.join(l.strip() for l in r.stdout.splitlines() if 'DLL Name:' in l)+'\n')
                    dest.unlink()
print(json.dumps({'codec_sources':[(r['package'],r.get('full_commit'),r.get('download',{}).get('error')) for r in rows]},indent=2))
