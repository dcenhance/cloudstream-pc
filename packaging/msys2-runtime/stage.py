#!/usr/bin/env python3
"""Stage an import-driven MSYS2 UCRT64 media runtime and exact recipe sources.
Requires Python 3, curl, bsdtar, zstd, objdump. Does not execute PKGBUILDs.
"""
import argparse, concurrent.futures, hashlib, json, os, pathlib, re, shutil, subprocess, tarfile, tempfile
P=pathlib.Path

def run(*args): return subprocess.check_output(args)
def sha(p):
    h=hashlib.sha256()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()
def download(url,p):
    p.parent.mkdir(parents=True,exist_ok=True)
    if not p.exists():
        subprocess.run(['curl','-L','--fail','--retry','4','--silent','--show-error','-o',str(p)+'.partial',url],check=True)
        os.replace(str(p)+'.partial',p)
    return p

def fields(text):
    result={}
    for chunk in text.split('\n\n'):
        lines=chunk.strip().splitlines()
        if lines:result[lines[0].strip('%')]=lines[1:]
    return result

def imports(p):
    return re.findall(r'DLL Name: (\S+)',run('objdump','-p',str(p)).decode(errors='replace'))

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--original',required=True);ap.add_argument('--runtime',required=True);ap.add_argument('--support',required=True);ap.add_argument('--seed');ap.add_argument('--repo',default='https://repo.msys2.org/mingw/ucrt64');ap.add_argument('--sources',default='https://mirror.msys2.org/mingw/sources');a=ap.parse_args()
    root=P(a.support);root.mkdir(parents=True,exist_ok=True);runtime=P(a.runtime)
    if runtime.resolve()==P(a.original).resolve():raise ValueError('Runtime target must differ from original')
    if not runtime.exists():shutil.copytree(a.original,runtime)
    cache=root/'binary-packages';cache.mkdir(exist_ok=True)
    if a.seed:
        for p in P(a.seed).glob('*.tar.zst'):
            dest=(root/'sources' if '.src.' in p.name else cache)/p.name;dest.parent.mkdir(exist_ok=True)
            if not dest.exists():shutil.copy2(p,dest)
    repos={};owners={}
    for name in ['ucrt64.db','ucrt64.files']:
        p=download(a.repo+'/'+name,root/name)
        with tempfile.TemporaryFile() as f:
            subprocess.run(['bsdtar','-cf','-','--format=ustar','@'+str(p)],stdout=f,check=True);f.seek(0)
            with tarfile.open(fileobj=f) as t:
                for m in t:
                    if not m.isfile():continue
                    d=fields(t.extractfile(m).read().decode());pkg=m.name.split('/')[0]
                    if m.name.endswith('/desc'):repos[pkg]=d
                    if m.name.endswith('/files'):
                        for path in d.get('FILES',[]):
                            if path.startswith('ucrt64/bin/') and path.lower().endswith('.dll'):owners[P(path).name.lower()]=(pkg,path)
    pkgs={};copied={};edges={};system=set();missing=set()
    # Known Windows system/API-set DLLs, never guessed from an unresolved import.
    system_names=set('advapi32 avicap32 gdiplus avrt bcrypt bcryptprimitives cabinet cfgmgr32 combase comctl32 comdlg32 crypt32 cryptbase cryptsp d2d1 d3d11 d3d12 d3d9 dcomp debughelp dbghelp dnsapi dsound dwmapi dwrite dxgi dxva2 fwpuclnt gdi32 glu32 hid imm32 iphlpapi kernel32 kernelbase mf mfplat mfreadwrite mfuuid mpr msacm32 msimg32 msvcrt ncrypt netapi32 normaliz ntdll ole32 oleacc oleaut32 opengl32 powrprof propsys psapi quartz rpcrt4 secur32 setupapi shell32 shlwapi shcore user32 userenv usp10 ucrtbase uxtheme version vfw32 winhttp wininet winmm winscard winspool wintrust wldap32 ws2_32 wsock32 wtsapi32'.split())
    def getpkg(key):
        if key in pkgs:return pkgs[key]
        d=repos[key];name=d['FILENAME'][0];archive=download(a.repo+'/'+name,cache/name)
        assert sha(archive)==d['SHA256SUM'][0],name
        out=root/'extracted'/key;out.mkdir(parents=True,exist_ok=True)
        if not (out/'.PKGINFO').exists():subprocess.run(['bsdtar','-xf',str(archive),'-C',str(out)],check=True)
        info={}
        for line in (out/'.PKGINFO').read_text().splitlines():
            if ' = ' in line:
                k,v=line.split(' = ',1);info.setdefault(k,[]).append(v)
        row={'key':key,'package':name,'binary_url':a.repo+'/'+name,'binary_sha256':sha(archive),'info':info,'files':[]}
        pkgs[key]=(out,row);print('PACKAGE',name,flush=True)
        return out,row
    def stage(key,path):
        name=P(path).name
        if name.lower() in copied:return
        out,row=getpkg(key);shutil.copy2(out/path,runtime/name);row['files'].append(name);copied[name.lower()]=key
        for dep in imports(runtime/name):
            edges.setdefault(name,[]).append(dep);low=dep.lower()
            if low.startswith(('api-ms-win-','ext-ms-win-')) or low.removesuffix('.dll') in system_names:system.add(dep);continue
            if low in owners:stage(*owners[low])
            else:missing.add(dep)
    for stem,path in [('mpv','ucrt64/bin/libmpv-2.dll'),('ffmpeg','ucrt64/bin/ffmpeg.exe')]:
        key=next(k for k,d in repos.items() if d['NAME'][0]=='mingw-w64-ucrt-x86_64-'+stem);stage(key,path)
    # Runtime data is needed in addition to import closure (TLS and font lookup).
    for key,d in repos.items():
        if d['NAME'][0]=='mingw-w64-ucrt-x86_64-ca-certificates':getpkg(key)
    for key,(out,row) in pkgs.items():
        for sub in ['ucrt64/etc/fonts','ucrt64/etc/ssl','ucrt64/share/fontconfig','ucrt64/share/ca-certificates','ucrt64/share/p11-kit']:
            if (out/sub).exists():shutil.copytree(out/sub,runtime/P(sub).relative_to('ucrt64'),dirs_exist_ok=True)
        lic=out/'ucrt64/share/licenses'
        if lic.exists():shutil.copytree(lic,runtime/'licenses/MSYS2',dirs_exist_ok=True)
    (root/'runtime-imports.json').write_text(json.dumps({'edges':edges,'system_imports':sorted(system),'unresolved':sorted(missing)},indent=2))
    rows=[v[1] for v in pkgs.values()];(root/'packages.json').write_text(json.dumps(rows,indent=2))
    if missing:raise RuntimeError('Unresolved imports: '+str(missing))
    def source(row):
        key=row['key'];out=pkgs[key][0];info=row['info'];base=info['pkgbase'][0];ver=info['pkgver'][0].split(':')[-1];name=base+'-'+ver+'.src.tar.zst'
        p=download(a.sources+'/'+name,root/'sources'/name)
        evidence=root/'metadata'/key;evidence.mkdir(parents=True,exist_ok=True)
        for f in ['.PKGINFO','.BUILDINFO','.MTREE']:
            if (out/f).exists():shutil.copy2(out/f,evidence/f)
        members=run('bsdtar','-tf',str(p)).decode().splitlines();recipepath=next(x for x in members if x.endswith('/PKGBUILD'))
        recipe=run('bsdtar','-xOf',str(p),recipepath);(evidence/'PKGBUILD').write_bytes(recipe)
        expected=re.search(r'^pkgbuild_sha256sum = (\w+)',(out/'.BUILDINFO').read_text(),re.M).group(1)
        assert hashlib.sha256(recipe).hexdigest()==expected,(name,'BUILDINFO recipe mismatch')
        srcinfo=next((x for x in members if x.endswith('/.SRCINFO')),None)
        if srcinfo:(evidence/'.SRCINFO').write_bytes(run('bsdtar','-xOf',str(p),srcinfo))
        # Hash every regular source archive member, streaming, including patches.
        inputs=[]
        proc=subprocess.Popen(['zstd','-dc',str(p)],stdout=subprocess.PIPE)
        with tarfile.open(fileobj=proc.stdout,mode='r|') as t:
            for m in t:
                if not m.isfile():continue
                h=hashlib.sha256();f=t.extractfile(m)
                for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
                inputs.append({'path':m.name,'bytes':m.size,'sha256':h.hexdigest()})
        assert proc.wait()==0
        expected_hashes=re.findall(r'\b[a-f0-9]{64}\b',recipe.decode(errors='replace'))
        # SRCINFO is canonical for hash declarations; SKIP remains explicit.
        declared=[];skipped=[]
        if srcinfo:
            for line in (evidence/'.SRCINFO').read_text().splitlines():
                if re.match(r'\s*sha256sums(?:_\w+)? = ',line):
                    value=line.split(' = ',1)[1];(skipped if value=='SKIP' else declared).append(value)
        actual={x['sha256'] for x in inputs};absent=[h for h in declared if h not in actual]
        row.update(source_package=name,source_url=a.sources+'/'+name,source_sha256=sha(p),pkgbuild_sha256=expected,source_member_hashes=inputs,declared_sha256_missing=absent,skipped_sha256_count=len(skipped))
        (evidence/'source-verification.json').write_text(json.dumps(row,indent=2));print('SOURCE',name,'hash gaps',len(absent),flush=True)
        return row
    errors=[]
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        futures={pool.submit(source,row):row for row in rows}
        for f in concurrent.futures.as_completed(futures):
            try:f.result()
            except Exception as e:errors.append({'package':futures[f]['package'],'error':str(e)})
    (root/'packages.json').write_text(json.dumps(rows,indent=2));(root/'errors.json').write_text(json.dumps(errors,indent=2))
    manifest=[{'path':str(p.relative_to(runtime)),'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(runtime.rglob('*')) if p.is_file()]
    (root/'runtime-manifest.json').write_text(json.dumps(manifest,indent=2))
    print(json.dumps({'packages':len(rows),'media_files':len(copied),'unresolved':sorted(missing),'source_errors':errors},indent=2))
    if errors:raise SystemExit(1)
if __name__=='__main__':main()
