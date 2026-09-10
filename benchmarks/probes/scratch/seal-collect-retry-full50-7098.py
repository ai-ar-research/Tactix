#!/usr/bin/env python3
"""Seal the existing7030 score only after all150 control/reference records pass."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import reference, sha
candidate = root / 'benchmarks/runs/20260910-7053-collect-retry-ranks1-50'
parent = root / 'benchmarks/runs/20260909-6743-reduce-watermark-ranks1-50'
out = root / 'benchmarks/reports/2026-09-10-collect-retry-checked50-7098'
assert not out.exists()
bindings = {}
classes = {}
def checked(path):
    record = json.loads(path.read_text())
    state = classify(record, True, 'COMPLETED')
    assert state['checked'] and state['clean'] and state['status'] in ('checked_SAT', 'checked_UNSAT'), (path, state)
    bindings[str(path.relative_to(root))] = sha(path)
    return record
for arm, campaign in (('candidate', candidate), ('parent', parent)):
    states = {'SAT': 0, 'UNSAT': 0}
    for rank in range(1, 51):
        r = checked(campaign / f'results/{rank-1:03d}.json')
        assert r['rank'] == rank and r['task'] == rank - 1
        states[r['verdict']] += 1
    classes[arm] = states
ref = reference()
states = {'SAT': 0, 'UNSAT': 0}
for row in ref['records']:
    measurement = row['measurement']
    campaign = root / row.get('source_campaign', ref['source_campaign'])
    r = checked(campaign / f'results/{measurement["task"]:03d}.json')
    for key in ('solve', 'certificate_sha256', 'certificate_bytes', 'instance', 'rank', 'arm'):
        assert r[key] == measurement[key], (row['rank'], key)
    mpath = campaign / 'manifest.json'
    m = json.loads(mpath.read_text())
    bindings[str(mpath.relative_to(root))] = sha(mpath)
    assert r['binary_sha256'] == m['arms']['kissat']['sha256']
    states[r['verdict']] += 1
classes['kissat'] = states
source = Path(__file__).with_name('summarize-compact-stamp-full50-6371.py')
code = source.read_text()
changes = {
 '20260909-6366-compact-stamp-ranks1-50': candidate.name,
 '20260909-6320-substitute-collapse-ranks1-50': parent.name,
 '2026-09-09-compact-stamp-full50-6371': out.name,
 "('pending_validation', 'checked_SAT', 'checked_UNSAT')": "('checked_SAT', 'checked_UNSAT')",
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
summary = json.loads((out / 'summary.json').read_text())
assert summary['score']['checked_clean'] == summary['score']['reference_checked'] == 50
assert summary['score']['all_certificates_checked']
for path in (Path(__file__), source, root / 'tools/bench/fixed_runtime.py', root / 'tools/bench/checked_comparison.py'):
    bindings[str(path.relative_to(root))] = sha(path)
summary['hashes'].update(bindings)
summary.update(final_classification=classes, excluded_ranks=[], solve_losses=[],
    unvalidated_or_residue_ranks=[],
    status='Final checked control score.7030 remains isolated/unfuzzed;7087 prototype gates are separate and failed/pending. No new solver run or baseline refresh.')
(out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
lines = ['# Final checked7030 Rank1–50 comparison', '',
    'All50 candidate,50 predecessor and50 fixed-Kissat results are checked/clean.',
    'No exclusions, solve losses or timeouts. Each arm solves25 SAT and25 UNSAT.',
    'Cross-window fixed-reference score, not causal unit-cost evidence or prototype acceptance.', '',
    '| Rank | Verdict | Tactix seconds | Kissat seconds | Ratio | Gap seconds | Tactix GiB |',
    '|---:|---|---:|---:|---:|---:|---:|']
for row in summary['rows']:
    lines.append(f"|{row['rank']}|{row['expected']}|{row['candidate_seconds']:.3f}|{row['kissat_seconds']:.3f}|{row['candidate_over_kissat']:.4f}|{row['gap_seconds']:.3f}|{row['candidate_peak_rss_kib']/1048576:.3f}|")
group = summary['groups']['all50']
lines += ['', f"Total solver/PAR-2 sum: {group['candidate']:.3f}/{group['kissat']:.3f} seconds = {group['ratio']:.9f}x.",
          f"Required reduction to reach1.01x: {group['candidate']-1.01*group['kissat']:.4f} seconds.", '',
          'Source hashes, all final classifications, parent comparisons and complete work/RSS columns are in summary.json and comparison.tsv.',
          'Test consideration: unchanged maintained score/classification parsers; additionally assert150 final checked/clean records, exact frozen reference measurements and all table totals. No new solver behavior.']
(out / 'comparison.md').write_text('\n'.join(lines) + '\n')
print('SEALED150 checked/clean records; no timings changed')
