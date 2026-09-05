#!/usr/bin/env python3
"""Finalize trust data, Git source verification, notices and separate archives."""
import argparse, concurrent.futures, hashlib, json, pathlib, re, shutil, subprocess, tempfile, tarfile
from stage import sha, run, download
P=pathlib.Path
ap=argparse.ArgumentParser();ap.add_argument('--runtime',required=True);ap.add_argument('--support',required=True);ap.add_argument('--shader-runtime',required=True);ap.add_argument('--dxil');ap.add_argument('--minimal-shaders',action='store_true');ap.add_argument('--archives',action='store_true');a=ap.parse_args();r=P(a.runtime);s=P(a.support)
rows=json.loads((s/'packages.json').read_text())
# Runtime-loaded shader libraries are not reliably represented in PE imports.
shader=[]
for n in ['dxcompiler.dll','dxil.dll','d3dcompiler_47.dll']:
    if a.minimal_shaders and n!='dxil.dll':
        (r/n).unlink(missing_ok=True);continue
    p=P(a.dxil) if n=='dxil.dll' and a.dxil else P(a.shader_runtime)/n
    shutil.copy2(p,r/n);shader.append({'path':n,'sha256':sha(p),'source_runtime':str(p)})
(s/'retained-shader-libraries.json').write_text(json.dumps(shader,indent=2))
# The CA package contains empty install-time generated bundles. Generate using
# its policy only, isolated from host/user additions. Fedora trust layout.
ca=next((s/'extracted').glob('*-ca-certificates-*'))/'ucrt64/share/pki/ca-trust-source'
with tempfile.TemporaryDirectory() as tmp:
    subprocess.run(['bwrap','--ro-bind','/','/','--bind',tmp,tmp,'--tmpfs','/etc/pki/ca-trust','--setenv','XDG_DATA_HOME',tmp,'--setenv','XDG_CONFIG_HOME',tmp,'--ro-bind',str(ca),'/usr/share/pki/ca-trust-source','trust','extract','--format=pem-bundle','--filter=ca-anchors','--purpose=server-auth','--overwrite',tmp+'/ca.pem'],check=True)
    pem=P(tmp+'/ca.pem').read_bytes();assert pem.count(b'BEGIN CERTIFICATE')>50
for dest in ['etc/ssl/certs/ca-bundle.crt','etc/ssl/cert.pem','etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem','ca-bundle.crt']:
    p=r/dest;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(pem)
shutil.copytree(ca,r/'share/pki/ca-trust-source',dirs_exist_ok=True)
(s/'ca-verification.json').write_text(json.dumps({'source_policy_sha256':sha(ca/'ca-bundle.trust.crt'),'bundle_sha256':hashlib.sha256(pem).hexdigest(),'server_auth_roots':pem.count(b'BEGIN CERTIFICATE'),'method':'Fedora trust extract in bwrap; only MSYS2 ca-trust source policy'},indent=2))
(r/'CloudStream.cmd').write_text('@echo off\r\nsetlocal\r\nset "SSL_CERT_FILE=%~dp0ca-bundle.crt"\r\nset "CURL_CA_BUNDLE=%~dp0ca-bundle.crt"\r\nset "FONTCONFIG_PATH=%~dp0etc\\fonts"\r\nset "FONTCONFIG_FILE=%~dp0etc\\fonts\\fonts.conf"\r\nstart "" /D "%~dp0" "%~dp0cloudstream.exe" %*\r\n')
# Verify VCS inputs by makepkg's git archive checksum, not a packfile checksum.
def vcs(row):
    meta=s/'metadata'/row['key'];text=(meta/'.SRCINFO').read_text();sources=re.findall(r'^\s*source = (.+)$',text,re.M);hashes=re.findall(r'^\s*sha256sums = (.+)$',text,re.M);results=[]
    if not any('git+' in x for x in sources):return results
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(['bsdtar','-xf',str(s/'sources'/row['source_package']),'-C',tmp],check=True)
        for url,expected in zip(sources,hashes):
            if 'git+' not in url:continue
            prefix,urlpart=(url.split('::',1) if '::' in url else (None,url));loc,ref=urlpart.split('#',1);ref=ref.split('=',1)[1];name=prefix or loc.removeprefix('git+').rstrip('/').split('/')[-1].removesuffix('.git');gitdir=P(tmp)/row['info']['pkgbase'][0]/name
            actual=hashlib.sha256(run('git','--git-dir='+str(gitdir),'archive','--format=tar',ref)).hexdigest()
            commit=run('git','--git-dir='+str(gitdir),'rev-parse',ref+'^{commit}').decode().strip()
            assert expected=='SKIP' or actual==expected,(row['key'],expected,actual)
            results.append({'url':url,'commit':commit,'git_archive_sha256':actual,'declared_sha256':expected})
            # Verbatim root-level legal files from the pinned tree.
            names=run('git','--git-dir='+str(gitdir),'ls-tree','--name-only',ref).decode().splitlines()
            for n in names:
                if re.match(r'(?i)^(copying|license|copyright|notice|authors)([.-]|$)',n):
                    try:b=run('git','--git-dir='+str(gitdir),'show',ref+':'+n)
                    except subprocess.CalledProcessError:continue
                    dst=r/'licenses/MSYS2-sources'/row['info']['pkgbase'][0]/n;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(b)
    (meta/'vcs-verification.json').write_text(json.dumps(results,indent=2));return results
vcsresults={}
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    futures={pool.submit(vcs,x):x for x in rows}
    for f in concurrent.futures.as_completed(futures):
        row=futures[f];res=f.result()
        if res:vcsresults[row['key']]=res
(s/'vcs-verification.json').write_text(json.dumps(vcsresults,indent=2))
# Copy legal texts embedded inside upstream source archives without unpacking
# their whole source trees; retain originals alongside distributor notices.
def legal(row):
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(['bsdtar','-xf',str(s/'sources'/row['source_package']),'-C',tmp],check=True)
        for archive in P(tmp).rglob('*'):
            if not archive.is_file() or not re.search(r'\.(tar\.(gz|xz|bz2|zst)|tgz|zip)$',archive.name):continue
            listing=subprocess.run(['bsdtar','-tf',str(archive)],capture_output=True,text=True)
            if listing.returncode:continue
            for name in listing.stdout.splitlines():
                parts=P(name).parts
                if len(parts)>4 or not re.match(r'(?i)^(copying|license|copyright|notice|authors)([.-]|$)',P(name).name):continue
                b=subprocess.run(['bsdtar','-xOf',str(archive),name],capture_output=True)
                if b.returncode or not b.stdout or len(b.stdout)>1024*1024:continue
                dst=r/'licenses/MSYS2-sources'/row['info']['pkgbase'][0]/('/'.join(parts).replace('/','__'));dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(b.stdout)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(legal,rows))
notice='\n\nMSYS2 media-runtime replacement\n===============================\nThe old shinchiro libmpv and gyan FFmpeg are replaced in THIS candidate.\nThe earlier static-runtime audit is historical, not its binary inventory.\nExact imported UCRT64 package sources and build metadata are supplied in\nCloudStream-Windows-MSYS2-sources.tar and metadata.tar.\nSee licenses/MSYS2 and licenses/MSYS2-sources for verbatim legal texts.\nThe shader DLL trio is retained from the original runtime for runtime loading.\nOther Qt, JVM, Microsoft and Mesa terms/audits remain applicable.\n\n'
notice+='\n'.join(x['package']+' | '+', '.join(x['info'].get('license',[])) for x in rows)+'\n'
p=r/'THIRD-PARTY-NOTICES.txt';old=p.read_text().split('\n\nMSYS2 media-runtime replacement')[0];p.write_text(old+notice)
for p in P(__file__).parent.glob('*'):
    if p.is_file():shutil.copy2(p,s/p.name)
(s/'runtime-manifest.json').write_text(json.dumps([{'path':str(p.relative_to(r)),'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(r.rglob('*')) if p.is_file()],indent=2))
(s/'SHA256SUMS.json').write_text(json.dumps([{'path':str(p.relative_to(s)),'bytes':p.stat().st_size,'sha256':sha(p)} for d in ['sources','metadata'] for p in sorted((s/d).rglob('*')) if p.is_file()],indent=2))
if a.archives:
    for name,paths in [('sources',['sources']),('metadata',['metadata']+[p.name for p in s.iterdir() if p.is_file()])]:
        with tarfile.open(s.parent/('CloudStream-Windows-MSYS2-'+name+'.tar'),'w') as t:
            for p in paths:t.add(s/p,arcname='windows-msys2-source-support/'+p)
    shutil.make_archive(str(s.parent/'CloudStream-Windows-MSYS2-runtime'),'zip',r.parent,r.name)
print(json.dumps({'packages':len(rows),'vcs_sources_verified':len(vcsresults),'ca_roots':pem.count(b'BEGIN CERTIFICATE'),'runtime':str(r)},indent=2))
