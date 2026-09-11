#!/usr/bin/env python3
"""Measure complete breadth of a repeated mixed memory/runtime tradeoff."""
from pathlib import Path
base=Path(__file__).with_name('prepare-factor-arena-full50-7201.py')
code=base.read_text()
changes={
    'import json\n':'import json\nimport datetime as dt\n',
    '20260911-7201-factor-arena-ranks1-50':'20260911-7283-realloc-ranks1-50',
    'check-submit-factor-arena-runtime-7186.py':'check-submit-dense-realloc-runtime-7273.py',
    '2026-09-11-factor-arena-assessment-7192.json':'2026-09-11-dense-realloc-assessment-7281.json',
    "assert a['runtime_expected'] == 18 and a['profile_expected'] == 6":"assert a['expected_outcomes'] == 30",
    "assert a['complete_factor']['max'] < 1":
        "assert all(a['runtime'][rank]['paired']['peak_rss_kib']['max']<1 for rank in ('9','25'))\nassert a['runtime']['31']['paired']['peak_rss_kib']['min']>1\nassert a['runtime']['38']['paired']['wall_seconds']['min']>1\nfor name in ('2026-09-11-realloc-propagation-cost-7280.json',):\n    p=root/'benchmarks/reports'/name; q=json.loads(p.read_text()); bind(p)\n    for rel,digest in q['hashes'].items(): bind(root/rel,digest)\np=root/'benchmarks/runs/20260911-7277-realloc-rank31-memory/comparison-7282.json'; q=json.loads(p.read_text()); bind(p)\nassert q['exact_structural_rows']==10538\nfor rel,digest in q['hashes'].items(): bind(root/rel,digest)",
    '20260910-7183-factor-arena-BC':'20260911-7272-dense-realloc-BC',
    "    p = bc/f'results/{task:03d}.json'\n    assert classify(json.loads(p.read_text()), True)['checked']; bind(p)":
        "    p=bc/f'results/{task:03d}.json'; final=p.exists()\n    if not final: p=bc/f'provisional/{task:03d}.json'\n    assert classify(json.loads(p.read_text()),final)['status'] in ('pending_validation','checked_SAT','checked_UNSAT'); bind(p)",
    '20260910-7180-factor-arena-build':'20260911-7271-dense-realloc-build',
    'build/pin-7180-factor-arena-release/tactix':'build/pin-7267-dense-realloc-release/tactix',
    '20260910-7167-vivify-revalidation-ranks1-50':'20260911-7262-occurrence-ranks1-50',
    "m.update(arms={'tactix':arm}, evidence_sha256=evidence,":"m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),arms={'tactix':arm}, evidence_sha256=evidence,",
    'Isolated7180 Factor arena on all50 after exact three-wave runtime/profile evidence. No Congruence fusion or telemetry ablation. Preserve every rank, solve loss and RSS tradeoff.':
        'Isolated7267 realloc payload versus own7253 on all50 after all30 exact targeted outcomes. Determine the breadth of memory reduction and every runtime/RSS regression. All ranks and fixed native denominator retained; no further composition or source change.',
    'Retain the candidate for full-set investigation: complete Factor median0.872448474, Rank31/35 whole-runtime0.993302108/0.986103414, Rank38neutral1.000755619 but+6220KiB. Not production acceptance or source fuzz waiver.':
        'Mixed memory-priority investigation:9/25 repeated RSS ratios.898698409/.869630463;31RSS1.136853840 and38runtime1.056273864 explicitly adverse. All repeats retained, spreads<=2%. Rank38 complete propagation profile1.087223941, not an allocator-callee cost. Rank31 trace does not reproduce Release RSS despite10538 exact structural rows. Full50 decides breadth; no net speedup, promotion or fuzz waiver implied.',
    ' All66 BC checks complete; all18 targeted runtimes and6 profiles exact. Copied full50 runner/matrix unchanged; maintained fixed-runtime and strict fingerprint fixtures cover this transfer.':
        ' All66 BC solver outcomes/33 pairs exact; all30 runtime outcomes and the matched Rank38 profile pair exact. Checks continue asynchronously. Full50 runner/matrix unchanged. Strict raw7256 fingerprints preserve every physical counter; no diagnostic exclusion for allocation-only change.',
    'All50 exact retained7144 work and certificate fingerprints before scoring. Pending independent checks allow development, not acceptance. Inspect memory on every rank; fixed native denominator unchanged.':
        'All50 exact own7253 work and certificate fingerprints INCLUDING physical dense moves/defrags before scoring. Pending parent/candidate checks permit provisional comparisons only; every independent model/proof and zero residue required for acceptance. All RSS/time regressions retained. Fixed native denominator unchanged.',
}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
