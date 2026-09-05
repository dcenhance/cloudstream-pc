#!/usr/bin/env python3
"""Collect shaderc static build inputs and exact Cargo.lock source crates."""
import argparse,concurrent.futures,hashlib,json,pathlib,re,subprocess,tempfile,tomllib
from stage import run,sha,download
P=pathlib.Path
ap=argparse.ArgumentParser();ap.add_argument('--support',required=True);ap.add_argument('--runtime');a=ap.parse_args();s=P(a.support);rows=json.loads((s/'packages.json').read_text());extra=s/'supplementary';extra.mkdir(exist_ok=True);receipts=[]
shader=next(x for x in rows if x['info']['pkgbase'][0]=='mingw-w64-shaderc');build=(s/'metadata'/shader['key']/'.BUILDINFO').read_text()
selected={}
for row in rows:
    recipe=(s/'metadata'/row['key']/'PKGBUILD').read_text()
    wanted=set(re.findall(r'\$\{MINGW_PACKAGE_PREFIX\}-([a-z0-9-]*headers)',recipe))
    if row==shader:wanted.update(['glslang','spirv-tools','spirv-headers'])
    for short in wanted:
        bi=(s/'metadata'/row['key']/'.BUILDINFO').read_text()
        match=re.search(r'^installed = (mingw-w64-ucrt-x86_64-'+short+r'-.+)-any$',bi,re.M)
        if match:selected.setdefault(match.group(1),[]).append(row['key'])
for pkg,consumers in selected.items():
    name=pkg+'-any.pkg.tar.zst';p=download('https://repo.msys2.org/mingw/ucrt64/'+name,extra/'binary-packages'/name)
    out=extra/'metadata'/pkg;out.mkdir(parents=True,exist_ok=True)
    for n in ['.PKGINFO','.BUILDINFO']: (out/n).write_bytes(run('bsdtar','-xOf',str(p),n))
    info=(out/'.PKGINFO').read_text();base=re.search(r'^pkgbase = (.+)',info,re.M).group(1);ver=re.search(r'^pkgver = (.+)',info,re.M).group(1);srcname=base+'-'+ver+'.src.tar.zst';src=download('https://mirror.msys2.org/mingw/sources/'+srcname,extra/'sources'/srcname)
    recipe=run('bsdtar','-xOf',str(src),base+'/PKGBUILD');(out/'PKGBUILD').write_bytes(recipe);expected=re.search(r'^pkgbuild_sha256sum = (.+)',(out/'.BUILDINFO').read_text(),re.M).group(1);assert hashlib.sha256(recipe).hexdigest()==expected
    receipts.append({'package':name,'binary_sha256':sha(p),'source':srcname,'source_sha256':sha(src),'pkgbuild_sha256':expected,'reason':'Static/header input pinned by consumer BUILDINFO','consumers':consumers})
(extra/'static-inputs.json').write_text(json.dumps(receipts,indent=2))
# Sources fetched by Cargo at prepare/build time are not in MSYS2 src archives.
crates={};gitdeps=[];locks=[]
for row in rows:
    recipe=(s/'metadata'/row['key']/'PKGBUILD').read_text()
    if 'cargo' not in recipe:continue
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(['bsdtar','-xf',str(s/'sources'/row['source_package']),'-C',tmp],check=True)
        for p in P(tmp).rglob('*'):
            if not p.is_file() or not re.search(r'\.(tar\.(gz|xz|bz2|zst)|tgz)$',p.name):continue
            names=run('bsdtar','-tf',str(p)).decode().splitlines()
            for n in names:
                if not n.endswith('/Cargo.lock'):continue
                data=run('bsdtar','-xOf',str(p),n);dest=extra/'cargo-locks'/row['key']/n.replace('/','__');dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data);locks.append(str(dest.relative_to(extra)))
                for pkg in tomllib.loads(data.decode()).get('package',[]):
                    source=pkg.get('source','')
                    if source.startswith('registry+'):
                        key=(pkg['name'],pkg['version']);assert source=='registry+https://github.com/rust-lang/crates.io-index',source
                        if key in crates:assert crates[key]['checksum']==pkg['checksum']
                        crates[key]=pkg
                    elif source.startswith('git+'):gitdeps.append(pkg)
(extra/'cargo-locks.json').write_text(json.dumps({'locks':locks,'git_dependencies':gitdeps},indent=2))
def collect(item):
    (name,version),pkg=item;url=f'https://static.crates.io/crates/{name}/{name}-{version}.crate';p=download(url,extra/'crates'/f'{name}-{version}.crate');assert sha(p)==pkg['checksum'],str(p)
    return {'name':name,'version':version,'url':url,'sha256':pkg['checksum'],'bytes':p.stat().st_size}
with concurrent.futures.ThreadPoolExecutor(max_workers=12) as pool:collected=list(pool.map(collect,crates.items()))
(extra/'crates.json').write_text(json.dumps(collected,indent=2))
if gitdeps:raise RuntimeError('Cargo git dependencies require collection: '+str(gitdeps))
if a.runtime:
    import tarfile
    runtime=P(a.runtime)
    for p in (extra/'crates').glob('*.crate'):
        with tarfile.open(p) as t:
            for m in t:
                if not m.isfile() or not re.match(r'(?i)^(copying|license|copyright|notice|authors)([.-]|$)',P(m.name).name):continue
                dest=runtime/'licenses/MSYS2-Cargo'/p.stem/m.name.replace('/','__');dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(t.extractfile(m).read())
    for p in (extra/'binary-packages').glob('*.pkg.tar.zst'):
        for n in run('bsdtar','-tf',str(p)).decode().splitlines():
            if n.startswith('ucrt64/share/licenses/') and not n.endswith('/'):
                dest=runtime/'licenses/MSYS2-supplementary'/P(n).relative_to('ucrt64/share/licenses');dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(run('bsdtar','-xOf',str(p),n))
print(json.dumps({'static_inputs':len(receipts),'cargo_locks':len(locks),'crates':len(collected),'crate_bytes':sum(x['bytes'] for x in collected)},indent=2))
