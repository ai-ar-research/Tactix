#!/usr/bin/env python3
"""Prepare a full-set memory/runtime tradeoff gate only after all targeted waves."""
from pathlib import Path
base = Path(__file__).with_name('prepare-factor-arena-full50-7201.py')
code = base.read_text()
changes = {
    'import json\n':'import json\nimport datetime as dt\n',
    "m.update(arms={'tactix':arm}, evidence_sha256=evidence,":
        "m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), arms={'tactix':arm}, evidence_sha256=evidence,",
    '20260911-7201-factor-arena-ranks1-50':'20260911-7262-occurrence-ranks1-50',
    'check-submit-factor-arena-runtime-7186.py':'check-submit-occurrence-runtime-7257.py',
    '2026-09-11-factor-arena-assessment-7192.json':'2026-09-11-occurrence-assessment-7261.json',
    "assert a['complete_factor']['max'] < 1":
        "assert a['complete_eliminate']['max'] < 1\nfor rank in ('31', '35'):\n    assert a['runtime'][rank]['paired']['peak_rss_kib']['max'] < 1",
    '20260910-7183-factor-arena-BC':'20260911-7256-occurrence-BC',
    "    p = bc/f'results/{task:03d}.json'\n    assert classify(json.loads(p.read_text()), True)['checked']; bind(p)":
        "    p = bc/f'results/{task:03d}.json'; final = p.exists()\n    if not final: p = bc/f'provisional/{task:03d}.json'\n    assert classify(json.loads(p.read_text()), final)['status'] in ('checked_SAT', 'checked_UNSAT', 'pending_validation'); bind(p)",
    '20260910-7180-factor-arena-build':'20260911-7253-occurrence-cutover-build',
    'build/pin-7180-factor-arena-release/tactix':'build/pin-7253-occurrence-cutover-release/tactix',
    'Isolated7180 Factor arena on all50 after exact three-wave runtime/profile evidence. No Congruence fusion or telemetry ablation. Preserve every rank, solve loss and RSS tradeoff.':
        'Isolated7253 single-image occurrence cutover on all50 after every predeclared runtime/profile pair. Measure breadth of memory benefit and every runtime/RSS regression; do not compose GC/root/AND/arena/telemetry prototypes. Preserve all50 ranks and the fixed native denominator.',
    'Retain the candidate for full-set investigation: complete Factor median0.872448474, Rank31/35 whole-runtime0.993302108/0.986103414, Rank38neutral1.000755619 but+6220KiB. Not production acceptance or source fuzz waiver.':
        'Memory-priority transfer investigation, not a claim of runtime improvement or promotion. Require all three whole-Eliminate ratios below1 and all Rank31/35 paired RSS ratios below1 before preparation. Every runtime pair and Rank38 memory regression remains in the bound7261 assessment. No arbitrary minimum gain, best-of selection or relaxation of the original target.',
    ' All66 BC checks complete; all18 targeted runtimes and6 profiles exact. Copied full50 runner/matrix unchanged; maintained fixed-runtime and strict fingerprint fixtures cover this transfer.':
        ' All66 BC expected solver outcomes and33 exact pairs; independent checks may remain pending. All18 targeted runtimes and6 profiles exact. Copied full50 runner/matrix unchanged;7256 strict physical-counter fingerprint fixtures and maintained fixed-runtime tests cover the transfer. Same-record provisional/final identity retains physical counters.',
    'All50 exact retained7144 work and certificate fingerprints before scoring. Pending independent checks allow development, not acceptance. Inspect memory on every rank; fixed native denominator unchanged.':
        'All50 exact retained7144 work/certificates before scoring, excluding only timing and the two strictly parsed7256 physical dense move/defrag counters across arms. Preserve those counters within each own provisional/final record. Pending independent checks allow development, not acceptance. Every model/proof and zero residue remain mandatory; inspect memory on every rank. Fixed native denominator unchanged.',
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
