from pathlib import Path
import hashlib,json,shutil,tarfile,subprocess
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
pin=root/'build/pin-ramsey-public-2742130e-debug-01';assert not pin.exists();pin.mkdir()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source=set(json.loads((root/'build/pin-parity-public-release-6fe14539-01/PIN.json').read_text())['source_sha256'])
for directory in ['src','include','tests']:
 for p in (root/directory).rglob('*'):
  if p.is_file() and p.suffix in ['.cpp','.hpp','.h','.c','.inc']:source.add(str(p.relative_to(root)))
hashes={p:sha(root/p) for p in sorted(source)}
with tarfile.open(pin/'source.tar.gz','w:gz') as out:
 for name in hashes:out.add(root/name,arcname=name,recursive=False)
assert hashes=={p:sha(root/p) for p in hashes}
for name in ['tactix','tests/test_ramsey_startup','tests/test_ramsey_public_action','tests/test_random_online','tests/test_online_api','tests/test_random_controller_policy']:
 shutil.copy2(root/'build/ramsey-public-dev'/name,pin/Path(name).name)
meta={'scope':'Unaccepted public Ramsey typed startup action; Debug/proof-shadow focused pin, not full Debug acceptance','head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'source_sha256':hashes,'source_archive_sha256':sha(pin/'source.tar.gz'),'binaries':{p.name:sha(p) for p in pin.iterdir() if p.is_file() and p.name!='source.tar.gz'}}
(pin/'PIN.json').write_text(json.dumps(meta,indent=2)+'\n')
old=json.loads((root/'benchmarks/runs/20261003-ramsey-native-startup/manifest.json').read_text())
bindings={p:h for p,h in old['bindings'].items() if p.startswith(old['parent']+'/')}
for p in [run/'run.py',run/'job.sh',run/'freeze.py',pin/'test_ramsey_startup',pin/'PIN.json',pin/'source.tar.gz']:
 bindings[str(p.relative_to(root))]=sha(p)
manifest={'parent':old['parent'],'binary':str((pin/'test_ramsey_startup').relative_to(root)),'bindings':bindings,'scope':meta['scope']}
(run/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(pin, len(hashes),'source/support files')
