import hashlib,json
from pathlib import Path
r=Path(__file__).resolve().parent
m=json.loads((r/'manifest.json').read_text())
for name,want in m['sha256'].items():
 h=hashlib.sha256()
 with open(name,'rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 if h.hexdigest()!=want:raise SystemExit('Changed input: '+name)
print('Verified '+str(len(m['sha256']))+' original/staged inputs',flush=True)
