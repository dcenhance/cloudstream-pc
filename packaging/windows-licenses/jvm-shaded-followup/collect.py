from pathlib import Path
import io, zipfile, tarfile, hashlib, json, urllib.request, concurrent.futures
OUT=Path(__file__).parent
SUP=Path('/data/CloudStream-releases/0.1.0-preview.2/windows-source-support')
SRC=SUP/'sources/jvm-followup'
archive=next(SRC.glob('dex2jar-*.tar.gz'))
t=tarfile.open(archive)
root=t.getnames()[0].split('/')[0]
records=[]
def save(name,data,origin):
 p=OUT/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
 records.append(dict(file=name,origin=origin,sha256=hashlib.sha256(data).hexdigest(),size=len(data)))
for name in ['LICENSE.txt','NOTICE.txt','dex-tools/open-source-license.txt','d2j-external/build.gradle']:
 save('dex2jar/'+name,t.extractfile(root+'/'+name).read(),archive.name+':'+name)
runtime=Path('/data/CloudStream-releases/0.1.0-preview.2/windows-msys2-runtime/provider-host/lib/d2j-external-2.4.38.jar')
r=zipfile.ZipFile(runtime)
comparison=[]
for m in t:
 if '/libs/' in m.name and m.name.endswith('.jar'):
  data=t.extractfile(m).read();z=zipfile.ZipFile(io.BytesIO(data));names=[n for n in z.namelist() if n.endswith('.class')]
  comparison.append(dict(jar=m.name.split('/')[-1],sha256=hashlib.sha256(data).hexdigest(),classes=len(names),identical=sum(n in r.namelist() and z.read(n)==r.read(n) for n in names),missing=[n for n in names if n not in r.namelist()],different=[n for n in names if n in r.namelist() and z.read(n)!=r.read(n)]))
  save('vendored/'+m.name.split('/')[-1]+'.manifest',z.read('META-INF/MANIFEST.MF'),m.name)
urls={
 'asm-9.10.1-sources.jar':'https://repo.maven.apache.org/maven2/org/ow2/asm/asm/9.10.1/asm-9.10.1-sources.jar',
 'android-dalvik-NOTICE.txt':'https://raw.githubusercontent.com/aosp-mirror/platform_dalvik/android-11.0.0_r1/NOTICE',
 'android-dx-Main.java':'https://raw.githubusercontent.com/aosp-mirror/platform_dalvik/android-11.0.0_r1/dx/src/com/android/dx/command/Main.java',
 'SPIRV-Tools-vulkan-sdk-1.4.357.0-LICENSE.txt':'https://raw.githubusercontent.com/KhronosGroup/SPIRV-Tools/vulkan-sdk-1.4.357.0/LICENSE',
 'SPIRV-Tools-vulkan-sdk-1.4.357.0-NOTICE.txt':'https://raw.githubusercontent.com/KhronosGroup/SPIRV-Tools/vulkan-sdk-1.4.357.0/NOTICE',
 'SPIRV-Headers-vulkan-sdk-1.4.357.0-LICENSE.txt':'https://raw.githubusercontent.com/KhronosGroup/SPIRV-Headers/vulkan-sdk-1.4.357.0/LICENSE',
 'LLVM-22.1.8-Support-COPYRIGHT.regex.txt':'https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-22.1.8/llvm/lib/Support/COPYRIGHT.regex',
 'LLVM-22.1.8-Support-xxhash.h':'https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-22.1.8/llvm/include/llvm/Support/xxhash.h',
}
def fetch(item):
 name,url=item
 try:
  data=urllib.request.urlopen(url,timeout=15).read()
  if name.endswith('.jar'):
   (SRC/name).write_bytes(data);z=zipfile.ZipFile(io.BytesIO(data))
   n='org/objectweb/asm/ClassReader.java';text=z.read(n).decode();block=text[:text.index('package org.objectweb.asm;')]
   save('ASM-9.10.1-copyright-and-BSD.txt',block.encode(),url+'!/'+n)
  elif name.endswith('.java') or name.endswith('.h'):
   text=data.decode();end=text.find('*/');save(name+'.header.txt',text[:end+2].encode(),url)
  else: save(name,data,url)
  return name,'OK'
 except Exception as e:return name,str(e)
results=list(concurrent.futures.ThreadPoolExecutor(max_workers=8).map(fetch,urls.items()))
sourcejar=SUP/'sources/jvm/d2j-external-2.4.38-sources.jar'
report=dict(tag='2.4.38',commit='ecdd1b5074e8f8ad1ade959b2a65b4babfe1b3d2',archive=str(archive),archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),runtime_sha256=hashlib.sha256(runtime.read_bytes()).hexdigest(),runtime_classes=sum(n.endswith('.class') for n in r.namelist()),vendored_comparison=comparison,empty_maven_sourcejar_members=zipfile.ZipFile(sourcejar).namelist(),fetch_results=results,notices=records)
(OUT/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
