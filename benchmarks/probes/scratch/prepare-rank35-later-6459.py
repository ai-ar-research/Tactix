#!/usr/bin/env python3
"""Current6424 versus6305 after the inherited1M adverse-control prefix."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6389-rank35-stamp-extension'
c=root/'benchmarks/runs/20260909-6459-rank35-later-prefix'; assert not c.exists()
prior=old/'summary.json'; s=json.loads(prior.read_text())
assert s['decisions']['complete_equal'] and s['matching_decisions_from_start']==99197378
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest
gate=root/'benchmarks/runs/20260909-6427-activation-stamp-BC-gate/solver-checkpoint-6429.json'
assert json.loads(gate.read_text())['solver_records']==66
authority=root/'benchmarks/runs/20260909-6424-activation-stamp-build/manifest.json'
b=json.loads(authority.read_text())
for rel,digest in b['files'].items(): assert sha(root/b['source']/rel)==digest
binary=root/'build/pin-6424-activation-stamp-trace/tactix'
assert sha(binary)==b['binaries'][str(binary.relative_to(root))]
m=json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(old/rel)==digest
for folder in ('pins','logs','traces','results'): (c/folder).mkdir(parents=True)
shutil.copy2(binary,c/'pins/candidate'); shutil.copy2(old/'pins/previous',c/'pins/previous')
(c/'run.py').write_text((old/'run.py').read_text().replace('tactix-6389-','tactix-6459-'))
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6389-rank35-extension','6459-rank35-later'))
m['authorities']['candidate']={'path':str(authority.relative_to(root)),'sha256':sha(authority)}
m.update(stop_conflict=3000000,stream_from=1000000,common_decisions=99197378,
    prior_common_hash='140843d3c85c31da',
    prior_closure={'path':str(prior.relative_to(root)),'sha256':sha(prior)},
    earlier_closure={str(prior.relative_to(root)):sha(prior)},
    provisional_gate={'path':str(gate.relative_to(root)),'sha256':sha(gate)},
    purpose='Locate the later Rank35 adverse decision split of current6424 versus6305. Current full8942669 conflicts versus prior4434673, while old6357/6305 agreed through1M.',
    validation='Revalidate the inherited99197378-row hash for BOTH current arms before claiming the earlier1M closure. Validate complete threshold overlap and every suffix row through3M. Changed earlier hash requires earlier localization, not solver-failure classification. Partial proof cleanup is not acceptance.',
    test_consideration='No solver/observer/parser behavior edit. Reuse maintained complete-row/boundary/overlap negative controls and65benchmark13watcher tests; exact manifest/source/input identities required. Native not rerun; semantic repair6424 may alter earlier assumptions.',
    limitation='Bounded current/earlier-candidate diagnostic, not a native full reference refresh, final proof or acceptance timing.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
