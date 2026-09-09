#!/usr/bin/env python3
"""Track the ten extraction-exit native pairs into actual Tactix full-call exit."""
from collections import Counter
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from congruence_stages import read_stages
from congruence_trace import read_binaries
c=root/'benchmarks/runs/20260909-6510-rank33-congruence-stages'
target=c/'gate-pair-localization-6515.json'; assert not target.exists()
p=c/'summary.json'; s=json.loads(p.read_text()); assert s['parent_replay']
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
with (c/'traces/candidate.congruence-binaries.tsv').open() as f: t=read_stages(f,227617)
with (c/'traces/kissat.congruence-binaries.tsv').open() as f: k=read_binaries(f,'kissat',227617)
counter=lambda frame:Counter(r[:2] for r in frame['rows'])
extra=counter(k[3])-counter(k[2]); assert sum(extra.values())==10
present=extra & counter(t[4]); absent=extra-counter(t[4])
sources={'.claude/worktrees/runtime-congruence-stages-6508/src/rules/congruence/congruence.cpp',
    'tools/solvers/variants/kissat-macro-heap-6090/src/congruence.c'}
out=dict(parent_summary_sha256=sha(p),native_first_extraction_added_pairs=list(sorted(extra.items())),
    present_at_tactix_first_full_exit=list(sorted(present.items())),
    absent_at_tactix_first_full_exit=list(sorted(absent.items())),
    source_sha256={r:sha(root/r) for r in sorted(sources)},
    limitation='Presence at full exit is not time/order identity. Absence may reflect root satisfaction or representative choice, not failed inference. Need aligned units/representatives and full large-output comparison before a closure repair.')
target.write_text(json.dumps(out,indent=2)+'\n'); print(json.dumps(out,indent=2))
