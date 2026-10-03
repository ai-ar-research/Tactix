from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
m=json.loads((run/'full-debug-manifest.json').read_text())
for p,h in m['sha256'].items():
 assert hashlib.sha256((root/p).read_bytes()).hexdigest()==h,p
print('Verified',len(m['sha256']),'immutable binaries/archive and runtime support bindings',flush=True)
