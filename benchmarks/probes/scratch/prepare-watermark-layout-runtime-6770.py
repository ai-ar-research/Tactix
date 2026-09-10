#!/usr/bin/env python3
"""Predeclare exact-work Rank38 cost and Rank46 repair-retention repetitions."""
from pathlib import Path
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
subprocess.run([sys.executable, str(root/'tools/slurm/session_deadline.py'),
    '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
gate = root/'benchmarks/runs/20260909-6766-watermark-layout-BC-gate/summary-6768.json'
g = json.loads(gate.read_text())
assert g['solver_records'] == 66 and len(g['pairs']) == 33
assert all(p['reported_work_equal'] and p['certificate_equal'] for p in g['pairs'])
proof = root/'benchmarks/runs/20260909-6769-watermark-layout-proof-control/summary.json'
assert json.loads(proof.read_text())['checked_proofs'] == 8
source = Path(__file__).with_name('prepare-collapse-timing-6332.py')
code = source.read_text()
changes = {
    '20260909-6332-collapse-three-wave-runtime': '20260909-6770-watermark-layout-runtime',
    '20260909-6176-learned-sort-rank4-runtime': '20260909-6766-watermark-layout-BC-gate',
    '20260909-6311-substitute-collapse-BC-gate': '20260909-6766-watermark-layout-BC-gate',
    "('candidate',6305,'substitute-collapse','20260909-6320-substitute-collapse-ranks1-50')":
        "('candidate',6763,'watermark-layout','20260909-6743-reduce-watermark-ranks1-50')",
    "('previous',6269,'vivify-binary-retry','20260909-6278-vivify-binary-ranks1-50')":
        "('previous',6730,'reduce-watermark','20260909-6743-reduce-watermark-ranks1-50')",
    '(32,33,40)': '(38,46)',
    "len(rows)==18": "len(rows)==12",
    'range(0,18,2)': 'range(0,12,2)',
    "'--array=0-5','--array=0-17'": "'--array=0-65','--array=0-11'",
    "'6176-sort-rank4','6332-collapse-timing'": "'6766-watermark-layout-BC','6770-watermark-layout-runtime'",
    "array='0-17'": "array='0-11'",
    "witnesses={'32':'primary Rank32 search reduction','33':'second productive changed-work family','40':'observed adverse search-work control'}":
        "witnesses={'38':'repeatable5.89% unchanged-work cost regression','46':'preserve historical-watermark work repair and price its unchanged work'}, dependencies={str(p.relative_to(root)):sha(p) for p in (gate/'summary-6768.json',root/'benchmarks/runs/20260909-6765-watermark-layout-control/summary.json',root/'benchmarks/runs/20260909-6769-watermark-layout-proof-control/summary.json')}",
    'Three sequential full-width six-task waves0-5,6-11,12-17; expected solver outcomes permit next wave without waiting proofs. Middle wave reverses arm order. No node constraints or throttles.':
        'Three sequential full-width four-task waves0-3,4-7,8-11; each rank owns a separate concurrent pair. Expected solver outcomes permit the next wave while proofs run. Middle wave reverses arm order; no throttle or node constraint.',
    '6305 retained-endpoint retry causes repeatable changed-work runtime effects versus6269, including the adverseRank40 case.':
        'Moving the unchanged marker into existing padding restores old member offsets. Does this remove repeatable Rank38 cost while preserving Rank46 work and speed? Both arms must match the same6730 per-rank certificate/work; no speedup assumed.',
    'All18 expected independently checked clean outcomes; every arm exact to its own work/proof oracle. Report per-rank median/range of three paired ratios and each arm spread; absolute claim requires <=2% spread. No fixed50 competitiveness or promotion from subset.':
        'All12 expected independently checked clean outcomes; both arms exact to the same6730 per-rank work/proof oracle. Report each three-pair median/range and complete arm spreads; absolute claim requires <=2% spread. Preserve Rank46 closure. No full50 or promotion claim from this subset.',
    'Unchanged pinned runner/oracles; existing39 benchmark13watcher tests. Matrix validates18tasks,pairedarmcoverage; source gates105/104,72fixtures,8proofs alreadypassed. New timing compares different search work,not isolated instruction cost.':
        'Layout-only6763 passes112/112/111 full suites with288 watermark cases;6765 proves restored offsets,6769 eight independent proofs exact6730,6768 all33 B/C pairs exact work/certificates.218 benchmark and19 watcher fixtures cover the reused timing readers;12-task paired matrix asserted.'}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__':str(source), '__name__':'__main__'})
