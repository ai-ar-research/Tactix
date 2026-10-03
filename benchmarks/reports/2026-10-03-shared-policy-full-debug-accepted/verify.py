import json,hashlib,sys
from pathlib import Path
r=Path(__file__).resolve().parent
m=json.loads((r/'manifest.json').read_text())
for name,want in m['sha256'].items():
 d=hashlib.sha256()
 with open(name,'rb') as f:
  for b in iter(lambda:f.read(1048576),b''):d.update(b)
 if d.hexdigest()!=want:raise SystemExit('Pin changed: '+name)
print('Verified '+str(len(m['sha256']))+' binary/support pins',flush=True)
