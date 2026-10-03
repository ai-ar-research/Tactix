from pathlib import Path
import hashlib,json,shutil,os,subprocess
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
base=root/'build/ramsey-public-dev';prior=root/'build/pin-ramsey-policy-fb174175-debug-01'
pin=root/'build/pin-ramsey-full-fb174175-debug-01';assert not pin.exists();pin.mkdir()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
sources=json.loads((prior/'PIN.json').read_text())['source_sha256']
assert all(sha(root/p)==h for p,h in sources.items()),'compile source changed'
shutil.copy2(prior/'source.tar.gz',pin/'source.tar.gz')
bindings={};binaries={}
for p in base.rglob('*'):
 if not p.is_file():continue
 rel=p.relative_to(base)
 if p.name=='CTestTestfile.cmake':
  dest=pin/rel;dest.parent.mkdir(parents=True,exist_ok=True)
  dest.write_text(p.read_text().replace(str(base),str(pin)))
  bindings[str(dest.relative_to(root))]=sha(dest)
 elif os.access(p,os.X_OK) and p.read_bytes()[:4]==b'\x7fELF' and 'CMakeFiles' not in p.parts:
  dest=pin/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
  binaries[str(dest.relative_to(root))]=sha(dest)
# Runtime scripts/support retain their launch-time hash. Compile-time source is
# bound by the frozen archive rather than by a later mutable root checkout.
for name,h in sources.items():
 if name.startswith(('tests/','tools/')) and Path(name).suffix not in ['.cpp','.hpp','.h','.c','.inc']:
  bindings[name]=h
bindings[str((pin/'source.tar.gz').relative_to(root))]=sha(pin/'source.tar.gz')
bindings.update(binaries)
meta={'scope':'Full Debug suite on an immutable compiled-source archive; repository documentation checks remain launch-time checks, not future-source acceptance','source_sha256':sources,'sha256':bindings,'binaries':len(binaries),'pin':str(pin.relative_to(root))}
(pin/'PIN.json').write_text(json.dumps(meta,indent=2)+'\n')
(run/'full-debug-manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
print('Pinned',len(binaries),'executables and',len(sources),'source/support files')
