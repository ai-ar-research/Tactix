from pathlib import Path
import hashlib,json,shutil,tarfile
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
base=root/'build/pin-ramsey-public-2742130e-debug-01';pin=root/'build/pin-ramsey-policy-fb174175-debug-01'
assert not pin.exists();pin.mkdir()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
sources=json.loads((base/'PIN.json').read_text())['source_sha256']
assert all(sha(root/p)==h for p,h in sources.items())
sources['tests/test_ramsey_policy_paths.cpp']=sha(root/'tests/test_ramsey_policy_paths.cpp')
shutil.copy2(root/'build/ramsey-public-dev/tests/test_ramsey_policy_paths',pin/'probe')
with tarfile.open(pin/'source.tar.gz','w:gz') as out:
 for name in sorted(sources):out.add(root/name,arcname=name,recursive=False)
assert all(sha(root/p)==h for p,h in sources.items())
meta={'scope':'Configured default and actual random-policy startup proof/no-proof and replay; targeted coverage only','source_sha256':sources,'binary_sha256':sha(pin/'probe'),'archive_sha256':sha(pin/'source.tar.gz')}
(pin/'PIN.json').write_text(json.dumps(meta,indent=2)+'\n')
old=json.loads((root/'benchmarks/runs/20261003-ramsey-public-action/manifest.json').read_text())
bindings={p:h for p,h in old['bindings'].items() if p.startswith(old['parent']+'/')}
for p in [run/'freeze.py',run/'run.py',run/'job.sh',pin/'probe',pin/'source.tar.gz',pin/'PIN.json']:
 bindings[str(p.relative_to(root))]=sha(p)
(run/'manifest.json').write_text(json.dumps({'parent':old['parent'],'binary':str((pin/'probe').relative_to(root)),'bindings':bindings,'scope':meta['scope']},indent=2)+'\n')
print(pin,len(sources))
