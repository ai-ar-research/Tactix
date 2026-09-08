#!/usr/bin/env python3
"""Score remaining full-run losses and validate the unit-cost-only premise."""
from pathlib import Path
source = Path(__file__).with_name('analyze-bound-full-5477.py').read_text()
source = source.replace("old = runs / '20260907-5454-eliminate-limit-ranks1-40/solver-checkpoint.json'",
                        "old = runs / '20260907-5465-bound-advance-ranks1-40/solver-checkpoint.json'")
source = source.replace("new = runs / '20260907-5465-bound-advance-ranks1-40/solver-checkpoint.json'",
                        "new = runs / '20260908-5485-sweep-cursor-ranks1-40/solver-checkpoint.json'")
source = source.replace('5477 analysis pending complete 5465 solver snapshot',
                        '5499 analysis pending complete 5485 solver snapshot')
anchor = "    row = dict(rank=int(rank)"
assert source.count(anchor) == 1
source = source.replace(anchor, '''    for arm, record in records.items():
        prior = json.loads((root / previous[arm]['record']).read_text())
        stats = lambda r: [s for s in r['solver_statistics'] if not s.startswith('c timing')]
        assert prior['instance'] == record['instance']
        assert stats(prior) == stats(record), ('Unexpected non-timing work difference', rank, arm)
        assert record.get('certificate_sha256')
        assert prior['certificate_sha256'] == record['certificate_sha256'], ('Unexpected certificate-byte difference', rank, arm)
''' + anchor)
source = source.replace('out = dict(source_sha256=',
                        'out = dict(all_80_non_timing_statistics_and_certificates_match_prior=True, source_sha256=')
source = source.replace("decision='Prioritize the largest remaining measured losses and their pending causal traces. Bounded Rank-37 closure does not establish full-run trajectory equality. Keep source-backed general repairs while investigating remaining work and cost.'",
                        "decision='The watch/Sweep cursor stack preserves every complete non-timing statistic and certificate byte in all40 candidate runs; native controls also match their predecessor. The full same-window ratio remains above1.01. Retain supported unit-cost fixes while prioritizing the largest remaining work/cost losses and selected causal probes.'")
exec(compile(source, __file__, 'exec'))
