#!/usr/bin/env python3
"""Transfer the repeated clause-arena memory repair across all fifty ranks."""
from pathlib import Path

base = Path(__file__).with_name('prepare-factor-arena-full50-7201.py')
code = base.read_text()
changes = {
    'import json\n': 'import json\nimport datetime as dt\n',
    '20260911-7201-factor-arena-ranks1-50': '20260911-7312-clause-realloc-ranks1-50',
    'check-submit-factor-arena-runtime-7186.py': 'check-submit-clause-realloc-runtime-7306.py',
    '2026-09-11-factor-arena-assessment-7192.json': '2026-09-11-clause-realloc-assessment-7309.json',
    "assert a['runtime_expected'] == 18 and a['profile_expected'] == 6":
        "assert a['expected_outcomes'] == 30",
    "assert all(r['absolute_headline_stable'] for r in a['spreads'].values())":
        """# The plan's2% gate limits absolute headlines, not complete paired evidence
# or a full-set memory investigation. Retain every unstable arm explicitly.
for spread in a['spreads'].values():
    assert spread['absolute_headline_stable'] == (max(spread['by_arm'].values()) <= .02)""",
    "assert a['complete_factor']['max'] < 1":
        """assert a['runtime']['31']['paired']['peak_rss_kib']['max'] < 1
assert a['runtime']['9']['paired']['peak_rss_kib']['max'] < 1
assert a['runtime']['25']['paired']['peak_rss_kib']['max'] <= 1
p = root/'benchmarks/runs/20260911-7310-clause-memory-rank31/resident-analysis-7311.json'
diagnostic = json.loads(p.read_text()); bind(p)
assert all(v['exact_release_peak_reproduced'] for v in diagnostic['arms'].values())
for rel, digest in diagnostic['hashes'].items(): bind(root/rel, digest)""",
    '20260910-7183-factor-arena-BC': '20260911-7305-clause-realloc-BC',
    "    p = bc/f'results/{task:03d}.json'\n    assert classify(json.loads(p.read_text()), True)['checked']; bind(p)":
        """    p = bc/f'results/{task:03d}.json'; final = p.exists()
    if not final: p = bc/f'provisional/{task:03d}.json'
    assert classify(json.loads(p.read_text()), final)['status'] in ('pending_validation','checked_SAT','checked_UNSAT'); bind(p)""",
    '20260910-7180-factor-arena-build': '20260911-7302-clause-realloc-build',
    'build/pin-7180-factor-arena-release/tactix': 'build/pin-7302-clause-realloc-release/tactix',
    '20260910-7167-vivify-revalidation-ranks1-50': '20260911-7283-realloc-ranks1-50',
    "m.update(arms={'tactix':arm}, evidence_sha256=evidence,":
        "m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), arms={'tactix':arm}, evidence_sha256=evidence,",
    'Isolated7180 Factor arena on all50 after exact three-wave runtime/profile evidence. No Congruence fusion or telemetry ablation. Preserve every rank, solve loss and RSS tradeoff.':
        'Isolated7302 clause-arena realloc on all50 versus own7267 after all30 exact targeted outcomes. Measure the breadth of the memory gain and every runtime/RSS regression. No further source changes; fixed native denominator and all ranks retained.',
    'Retain the candidate for full-set investigation: complete Factor median0.872448474, Rank31/35 whole-runtime0.993302108/0.986103414, Rank38neutral1.000755619 but+6220KiB. Not production acceptance or source fuzz waiver.':
        'Memory-priority full-set investigation, not a speedup claim: all three Rank31 RSS pairs must improve and preserve9/25 savings. External7310 must reproduce both current Release peaks. All paired runtime regressions and complete arm spreads remain in7309; any spread above2% rejects that absolute headline, not the paired evidence. No favorable-repeat selection, promotion or source-fuzz waiver.',
    ' All66 BC checks complete; all18 targeted runtimes and6 profiles exact. Copied full50 runner/matrix unchanged; maintained fixed-runtime and strict fingerprint fixtures cover this transfer.':
        ' All66 BC solver outcomes/33 pairs and all30 targeted outcomes exact; independent checks asynchronous.7303 twenty sanitizer legs and7304 156proofs/156models/two rejecting controls passed. Copied full50 runner/matrix unchanged; strict raw fingerprints retain every physical counter.',
    'All50 exact retained7144 work and certificate fingerprints before scoring. Pending independent checks allow development, not acceptance. Inspect memory on every rank; fixed native denominator unchanged.':
        'All50 exact own7267 work/certificate fingerprints INCLUDING physical dense counters before provisional scoring. Classify every loss/invalidity and preserve every RSS/time regression. Pending independent checks permit development, not acceptance; final models/proofs and zero residue required. Fixed native denominator unchanged.',
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'),
     {'__file__': str(Path(__file__)), '__name__': '__main__'})
