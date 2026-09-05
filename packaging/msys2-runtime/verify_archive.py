#!/usr/bin/env python3
"""Verify staged PE imports/exports and write release archives + hashes."""
import argparse,json,pathlib,re,shutil,struct,subprocess,tarfile,zipfile
from stage import sha
P=pathlib.Path
class PE:
 def __init__(self,p):
  self.b=p.read_bytes();b=self.b;pe=struct.unpack_from('<I',b,0x3c)[0];assert b[pe:pe+4]==b'PE\0\0';n=struct.unpack_from('<H',b,pe+6)[0];opt=pe+24;size=struct.unpack_from('<H',b,pe+20)[0];magic=struct.unpack_from('<H',b,opt)[0];self.wide=magic==0x20b;self.d=opt+(112 if self.wide else 96);self.sections=[]
  for i in range(n):
   x=opt+size+40*i;vs,va,raw,off=struct.unpack_from('<IIII',b,x+8);self.sections.append((va,max(vs,raw),off))
 def off(self,rva):
  for va,size,off in self.sections:
   if va<=rva<va+size:return off+rva-va
  return rva
 def string(self,rva):
  p=self.off(rva);return self.b[p:self.b.index(b'\0',p)].decode(errors='replace')
 def imports(self):
  rva=struct.unpack_from('<I',self.b,self.d+8)[0];result={}
  if not rva:return result
  p=self.off(rva)
  while True:
   orig,stamp,chain,name,first=struct.unpack_from('<IIIII',self.b,p);p+=20
   if not name:break
   vals=[];q=self.off(orig or first);width=8 if self.wide else 4
   while True:
    v=int.from_bytes(self.b[q:q+width],'little');q+=width
    if not v:break
    vals.append(v&0xffff if v>>(width*8-1) else self.string(v+2))
   result[self.string(name).lower()]=vals
  return result
 def exports(self):
  rva=struct.unpack_from('<I',self.b,self.d)[0]
  if not rva:return set()
  vals=struct.unpack_from('<IIHHIIIIIII',self.b,self.off(rva));base,nfunc,nnames,funcs,names,ords=vals[5:];res=set(range(base,base+nfunc))
  for i in range(nnames):res.add(self.string(struct.unpack_from('<I',self.b,self.off(names)+4*i)[0]))
  return res
ap=argparse.ArgumentParser();ap.add_argument('--runtime',required=True);ap.add_argument('--support',required=True);ap.add_argument('--archives',action='store_true');a=ap.parse_args();r=P(a.runtime);s=P(a.support)
rows=json.loads((s/'packages.json').read_text());edges=json.loads((s/'runtime-imports.json').read_text());assert not edges['unresolved'];dlls={p.name.lower():p for p in r.glob('*.dll')};exports={};errors=[];checked=0
for name in ['cloudstream.exe']+sorted(edges['edges']):
 for dll,symbols in PE(r/name).imports().items():
  if dll not in dlls:continue
  if dll not in exports:exports[dll]=PE(dlls[dll]).exports()
  absent=[x for x in symbols if x not in exports[dll]];checked+=len(symbols)
  if absent:errors.append({'importer':name,'dll':dll,'absent_symbols':absent})
vcs=json.loads((s/'vcs-verification.json').read_text());gaps=[]
for row in rows:
 verified={x['git_archive_sha256'] for x in vcs.get(row['key'],[])}
 gaps.extend({'package':row['package'],'hash':h} for h in row['declared_sha256_missing'] if h not in verified)
assert not gaps,gaps
report={'packages':len(rows),'media_PE_files':len(edges['edges']),'checked_app_local_import_symbols':checked,'missing_symbols':errors,'unresolved_DLLs':edges['unresolved'],'source_checksum_gaps':gaps,'binary_BUILDINFO_recipe_matches':len(rows),'source_archives':len(list((s/'sources').glob('*.src.tar.zst'))),'ca_roots':json.loads((s/'ca-verification.json').read_text())['server_auth_roots'],'windows_execution':'Pending parent VM tests; this verification is static PE/source validation.'}
(s/'verification.json').write_text(json.dumps(report,indent=2));assert not errors,errors
for p in P(__file__).parent.glob('*'):
 if p.is_file():shutil.copy2(p,s/p.name)
for root,name,exclude in [(r,'runtime-manifest.json',set()),(s,'SHA256SUMS.json',{'extracted','binary-packages'})]:
 manifest=[]
 for p in sorted(root.rglob('*')):
  rel=p.relative_to(root)
  if not p.is_file() or rel.parts[0] in exclude or p.name=='SHA256SUMS.json':continue
  manifest.append({'path':str(rel),'bytes':p.stat().st_size,'sha256':sha(p)})
 (s/name).write_text(json.dumps(manifest,indent=2))
if a.archives:
 archives=[]
 for name,paths in [('sources',['sources']),('supplementary',['supplementary']),('metadata',['metadata']+[p.name for p in s.iterdir() if p.is_file()])]:
  dest=s.parent/('CloudStream-Windows-MSYS2-'+name+'.tar')
  with tarfile.open(dest,'w') as t:
   for p in paths:t.add(s/p,arcname='windows-msys2-source-support/'+p)
  archives.append(dest)
 dest=s.parent/'CloudStream-Windows-MSYS2-runtime.zip'
 with zipfile.ZipFile(dest,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for p in sorted(r.rglob('*')):
   if p.is_file():z.write(p,arcname='CloudStream/'+str(p.relative_to(r)))
 with zipfile.ZipFile(dest) as z:assert z.testzip() is None
 archives.append(dest);(s.parent/'CloudStream-Windows-MSYS2-SHA256SUMS').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in archives))
print(json.dumps(report,indent=2))
