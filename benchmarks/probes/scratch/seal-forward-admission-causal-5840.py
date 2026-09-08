#!/usr/bin/env python3
"""Seal the old BC gate and record the new candidate's causal frontier."""
import csv
import hashlib
import itertools
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
here = Path(__file__).resolve().parent
sealer = here / 'seal-congruence-gates-5372.py'
program = sealer.read_text()
old = "[('20260907-5371-congruence-prefix-runtime',12),('20260907-5372-congruence-prefix-BC-gate',66)]"
assert program.count(old) == 1
program = program.replace(old, "[('20260908-5806-forward-unit-BC-gate',66)]")
exec(compile(program, str(sealer), 'exec'), {'__file__': str(sealer), '__name__': '__main__'})
c = root / 'benchmarks/runs/20260908-5837-forward-admission-rank32-prefix'
p = c / 'summary.json'
s = json.loads(p.read_text())
assert s['matching_decisions'] == 61154590 and s['cached_overlap_rows'] == 2
assert s['suffix_comparison']['left'][0] == s['suffix_comparison']['right'][0] == 225987
fields = 'name stage conflicts decisions active unassigned irred red binary'.split()
def rows(path):
    lines = path.read_text().splitlines()
    return list(csv.DictReader(lines[2:], fieldnames=lines[1][2:].split('\t'), delimiter='\t'))
native = root / s['cached_bindings']['native']['record']
cp = c / 'traces/candidate.boundaries.tsv'
np = native.parent.parent / 'traces/kissat.boundaries.tsv'
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(cp) == s['complete_trace_sha256'][cp.name]
assert sha(np) == json.loads(native.read_text())['trace_sha256'][np.name]
raw = {'candidate': rows(cp), 'native': rows(np)}
portable = {arm: [{key: row[key].lower() if key == 'name' else row[key] for key in fields}
                 for row in records if row['name'].lower() in ('strengthen', 'bve')]
            for arm, records in raw.items()}
first = next(dict(ordinal=i, candidate=a, native=b) for i, (a, b) in
             enumerate(itertools.zip_longest(portable['candidate'], portable['native'])) if a != b)
assert first['ordinal'] == 40
assert all(a[key] == b[key] for a, b in zip(s['target_boundaries']['candidate'], s['target_boundaries']['native'])
           for key in fields if key != 'name')
macro = c / 'traces/candidate.macro.tsv'
assert sha(macro) == s['complete_trace_sha256'][macro.name]
events = [row for row in rows(macro) if row['conflicts'] == '225987']
native_events = [row for row in raw['native'] if row['conflicts'] == '225987']
target = c / 'causal-followup.json'
assert not target.exists()
result = dict(summary_sha256=sha(p), matching_decisions=s['matching_decisions'],
    matching_forward_bve_boundaries=40, first_later_forward_bve_difference=first,
    frontier_conflict=225987, candidate_events=events, native_events=native_events,
    conclusion='Root-only admission repair closes the179463 forward/BVE count mismatch and preserves61,154,590 decisions. First new split occurs after stable Reduce/Reorder at225987,not at a forward/BVE count split.',
    next_action='Price5838 full40 and5836 B/C separately;inspect common-state Reduce/Reorder at225987 on Rank32. Independently repair the proven post-forward dense-closure gap on Rank31.',
    tooling_correction='Cached native100K end and300K suffix overlap by two identical conflict100000 decisions. Comparator verifies exact overlap bytes and counts them once;three overlap controls pass. Initial no-overlap assumption was rejected before sealing.',
    limitation='Portable counts and decision prefix are not complete physical-state identity or runtime attribution. Proof checks/fuzz and cumulative acceptance remain open.')
target.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({key: result[key] for key in ('matching_decisions', 'matching_forward_bve_boundaries', 'frontier_conflict', 'conclusion')}, indent=2))
