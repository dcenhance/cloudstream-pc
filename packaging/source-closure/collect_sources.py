#!/usr/bin/env python3
"""Collect only source revisions supported by archived build evidence."""
from collect_evidence import ROOT, fetch, gh
import concurrent.futures,hashlib,json,pathlib,re,tarfile
P=ROOT/'mpv-recipe/packages'
changes={'opus':'8f39f97','harfbuzz':'2093bb59','libarchive':'2f81aa8','highway':'17abbca','libsdl2':'b90ac950','spirv-tools':'0db1457','glslang':'3b7ce57','ngtcp2':'b5de53a'}
active_fixed={'angle-headers','avisynth-headers','libpsl','libsixel','openssl','vapoursynth','xz','libiconv','libopenmpt','lzo','opus-dnn','xvidcore'}
rows=[]
for f in sorted(P.glob('*.cmake')):
    s=f.read_text(); fields={k:re.findall(r'^\s*'+k+r'\s+([^\n]+)',s,re.M) for k in ['GIT_REPOSITORY','GIT_TAG','GIT_RESET','URL','URL_HASH']}
    row={'package':f.stem,'recipe':str(f.relative_to(ROOT)),'fields':fields,'status':'recipe-only; exact compiled source not locked'}
    if f.stem in changes:
        row['ref']=changes[f.stem];row['basis']='x86_64 job fetch output + recipe tracking branch + rebuild in same job'
    elif f.stem in active_fixed:
        refs=[v.split()[0] for k in ['GIT_RESET','GIT_TAG'] for v in fields[k] if re.match('[0-9a-f]{40}',v)]
        if refs: row['ref']=refs[0];row['basis']='build-head recipe explicit commit'
        elif fields['URL']:
            row['url']=fields['URL'][0].strip('"');row['basis']='build-head recipe version archive and digest'
    if 'ref' in row:
        repo=fields['GIT_REPOSITORY'][0].removesuffix('.git').removeprefix('https://github.com/')
        row['repo']=repo
    rows.append(row)
def collect(row):
    if 'ref' in row:
        receipt=gh('commit-metadata/'+row['package']+'.json','repos/'+row['repo']+'/commits/'+row['ref'])
        if receipt['exit_code']:
            row['error']=receipt;return row
        metadata=json.loads((ROOT/receipt['path']).read_text());row['full_commit']=metadata['sha']
        row['url']='https://codeload.github.com/'+row['repo']+'/tar.gz/'+metadata['sha']
    if 'url' not in row:return row
    ext='.tar.gz'
    if row['url'].endswith('.tar.bz2'):ext='.tar.bz2'
    receipt=fetch(('mpv-sources/'+row['package']+ext,row['url']));row['download']=receipt
    if 'error' in receipt:return row
    data=(ROOT/receipt['path']).read_bytes()
    if row['fields']['URL_HASH']:
        alg,digest=row['fields']['URL_HASH'][0].split('=',1)
        row['recipe_digest_verified']=hashlib.new(alg.lower(),data).hexdigest().lower()==digest.lower()
        if not row['recipe_digest_verified']:raise ValueError(row['package']+' digest mismatch')
    with tarfile.open(ROOT/receipt['path']) as t:
        names=[]
        for m in t.getmembers():
            if m.isfile() and re.match(r'(?i)^(copying|copyright|license|notice|authors)([.\-_].*)?$',pathlib.PurePosixPath(m.name).name):
                rel=pathlib.PurePosixPath(m.name)
                if '..' in rel.parts or rel.is_absolute():raise ValueError('unsafe archive')
                p=ROOT/'mpv-notices'/row['package']/rel;p.parent.mkdir(parents=True,exist_ok=True)
                stream=t.extractfile(m)
                if stream is not None:p.write_bytes(stream.read());names.append(str(p.relative_to(ROOT)))
        row['notice_files']=names
    row['status']='source collected; this component only (submodules/transitive inputs still require review)'
    return row
if __name__=='__main__':
    with concurrent.futures.ThreadPoolExecutor(5) as pool:rows=list(pool.map(collect,rows))
    (ROOT/'mpv-source-evidence.json').write_text(json.dumps(rows,indent=2))
    extras=[]
    for f in ['msys2-mpv.html','msys2-ffmpeg.html']:
        for url in sorted(set(re.findall(r'https[^\"<> ]+(?:src.tar.zst|pkg.tar.zst)',(ROOT/f).read_text()))):extras.append(('msys2-candidate/'+url.rsplit('/',1)[-1],url))
    with concurrent.futures.ThreadPoolExecutor(4) as pool: receipts=list(pool.map(fetch,extras))
    (ROOT/'msys2-downloads.json').write_text(json.dumps(receipts,indent=2))
    gh('gyan-tree-main.json','repos/GyanD/codexffmpeg/git/trees/main?recursive=1')
    print(json.dumps({'components':len(rows),'collected':[r['package'] for r in rows if r['status'].startswith('source collected')],'errors':[(r['package'],r.get('error',r.get('download',{}).get('error'))) for r in rows if r.get('error') or r.get('download',{}).get('error')],'msys2':receipts},indent=2))
