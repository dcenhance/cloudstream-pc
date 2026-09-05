#!/usr/bin/env python3
"""Verify collected files and bind replacement recipes to binary BUILDINFO."""
from collect_evidence import ROOT
import hashlib,io,json,pathlib,re,shutil,subprocess,tarfile
results=[]
for stem,version in [('mpv','0.41.0-6'),('ffmpeg','9.0.1-3')]:
    pkg=f'mingw-w64-ucrt-x86_64-{stem}-{version}-any.pkg.tar.zst'
    src=f'mingw-w64-{stem}-{version}.src.tar.zst'
    buildinfo=(ROOT/'msys2-inspection'/pkg/'.BUILDINFO').read_text()
    recipe=(ROOT/'msys2-inspection'/src/f'mingw-w64-{stem}'/'PKGBUILD').read_bytes()
    expected=re.search(r'pkgbuild_sha256sum = (\w+)',buildinfo).group(1)
    actual=hashlib.sha256(recipe).hexdigest()
    assert expected==actual,(stem,expected,actual)
    with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(['zstd','-dc',str(ROOT/'msys2-candidate'/src)]))) as t:
        inputs={}
        for m in t.getmembers():
            if m.isfile():
                stream=t.extractfile(m)
                if stream is not None:inputs[pathlib.PurePosixPath(m.name).name]=hashlib.sha256(stream.read()).hexdigest()
    hashes=re.search(r"sha256sums=\((.*?)\)",recipe.decode(),re.S).group(1)
    expected_inputs=re.findall(r"'([a-f0-9]{64})'",hashes)
    assert all(h in inputs.values() for h in expected_inputs),(stem,'source input mismatch')
    package_sha=hashlib.sha256((ROOT/'msys2-candidate'/pkg).read_bytes()).hexdigest()
    assert package_sha in (ROOT/f'msys2-{stem}.html').read_text()
    results.append({'package':pkg,'binary_sha256_matches_saved_publisher_page':True,'pkgbuild_sha256_matches_BUILDINFO':actual,'all_non_skipped_PKGBUILD_source_checksums_verified':True,'source_inputs':inputs,'installed_build_packages':len(re.findall(r'^installed = ',buildinfo,re.M))})
for name in ['mpv-source-evidence.json','codec-source-evidence.json']:
    for row in json.loads((ROOT/name).read_text()):
        if 'download' in row and 'sha256' in row['download']:
            d=row['download'];assert hashlib.sha256((ROOT/d['path']).read_bytes()).hexdigest()==d['sha256']
(ROOT/'verification.json').write_text(json.dumps(results,indent=2))
for f in pathlib.Path(__file__).parent.iterdir():
    if f.is_file() and f.suffix in ['.py','.md']:shutil.copy2(f,ROOT/f.name)
manifest=[]
for p in sorted(ROOT.rglob('*')):
    if p.is_file() and p.name!='SHA256SUMS.json':
        b=p.read_bytes();manifest.append({'path':str(p.relative_to(ROOT)),'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()})
(ROOT/'SHA256SUMS.json').write_text(json.dumps(manifest,indent=2))
print(json.dumps({'verified_msys2':results,'inventoried_files':len(manifest),'total_bytes':sum(x['bytes'] for x in manifest),'mpv_source_archives':len(list((ROOT/'mpv-sources').glob('*'))),'notice_files':len([p for p in (ROOT/'mpv-notices').rglob('*') if p.is_file()])},indent=2))
