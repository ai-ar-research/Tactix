#!/usr/bin/env python3
"""New post-score audit and explicit staging; old checkpoints remain frozen."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
root = Path(__file__).resolve().parents[3]
source = Path(__file__).with_name('audit-forward-preclean-5885.py')
code = source.read_text()
changes = {
    '2026-09-08-forward-preconnection-validation.json': '2026-09-08-forward-preconnection-final-validation.json',
    '2026-09-08-forward-preconnection-cleanup.md': '2026-09-08-forward-preconnection-runtime.md',
    '(5869, 5879, 5883)': '(5869, 5879, 5883, 5891, 5893)',
    '1674668,1674669,1674700,1674781': '1674668,1674669,1674700,1674781,1674840,1674849',
    '1674504,1674639,1674669,1674781': '1674504,1674639,1674669,1674781,1674840,1674849',
    '5870 tested isolated repair;runtime/causal gates pending.5869 exact Reduce selection match narrows Rank32 next diagnosis. No goal completion or promotion.':
        '5870 regresses full40 to1.165359x and C to1.015549x;Rank31 full702.344s despite native300K match.5883 confirms shared scores/assignment flags but different physical heap pose.5891/5893 diagnose next boundaries;no acceptance or promotion.',
}
code = re.sub('|'.join(re.escape(k) for k in sorted(changes,key=len,reverse=True)), lambda m: changes[m.group()], code)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
report = root / 'report-to-andrew/20260908-1520-preconnection-runtime'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (' in log
owned = {'PROGRESS.md', str(Path(__file__).relative_to(root)),
         'benchmarks/reports/2026-09-08-forward-preconnection-runtime.md',
         'benchmarks/reports/2026-09-08-forward-preconnection-final-validation.json'}
for number in range(5890,5895):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.*'): owned.add(str(p.relative_to(root)))
for number in (5880, 5883, 5891, 5893):
    c = next((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    for p in c.rglob('*'):
        if not p.is_file() or p.stat().st_size >= 5 << 20: continue
        if p.suffix not in ('.json','.py','.sbatch','.tsv','.patch'): continue
        if 'traces' in p.relative_to(c).parts and number != 5883: continue
        owned.add(str(p.relative_to(root)))
exceptions = []
r = subprocess.run(['git','diff','--cached','--check'],cwd=root,capture_output=True,text=True)
for line in r.stdout.splitlines():
    match = re.match(r'(.+):(\d+): (.+)',line)
    if not match: continue
    name, num, error = match.groups()
    p = root / name
    value = p.read_text().splitlines()[int(num)-1]
    ok = (error == 'trailing whitespace.' and ((p.suffix == '.patch' and value == ' ') or
          (p.suffix == '.log' and 'clauses in core' in value))) or (
          error == 'new blank line at EOF.' and name == 'benchmarks/runs/20260908-5876-forward-preclean-edges/forward-fixture.hpp')
    assert ok, (name,num,error,value)
    exceptions.append(dict(path=name,line=int(num),reason=error,sha256=sha(p)))
target = root / 'benchmarks/reports/2026-09-08-forward-preconnection-final-landing.json'
assert not target.exists()
target.write_text(json.dumps(dict(hashes={rel:sha(root / rel) for rel in sorted(owned)},
    andrew_report={str((report / name).relative_to(root)):sha(report / name) for name in ('report.tex','report.pdf')},
    frozen_whitespace_exceptions=exceptions,
    limitation='Local checkpoint,no push/promotion/deletion;full40 and C regressions remain open. Active5891/5893 andproof-check campaigns continue. Original audits/reports immutable.'),indent=2)+'\n')
owned.add(str(target.relative_to(root)))
subprocess.run(['git','add','-f','--',*sorted(owned)],cwd=root,check=True)
print('Final runtime checkpoint staged:',len(owned),'paths; whitespace exceptions',len(exceptions))
