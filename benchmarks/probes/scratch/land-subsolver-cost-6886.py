#!/usr/bin/env python3
"""Land approved prototype evidence only; production unchanged, no push retry."""
from pathlib import Path
source = Path(__file__).with_name('land-watermark-followup-6762.py')
code = source.read_text()
for before,after in {
    '2026-09-09-watermark-followup-audit-6761.json':'2026-09-10-subsolver-cost-audit-6885.json',
    'Audit full50 watermark costs and localize Rank21 search split':
        'Prototype approved full-width binary watches with exact own-arm controls\\n\\n'
        'User explicitly approved isolated12-byte tagged watches: preserve full32-bit literal/reference, native8-byte logical effort, proof identities, ordering and policy. Kissat binary propagation avoids arena loads; host watch storage grows50%, so runtime/RSS must justify adoption. Rollback6730 unchanged. No FlatLists or packing expansion.\\n\\n'
        'Complete own-source113/113/112 suites; own-arm720configs/12960solves and direct core80configs/320solves match,120 fixed external proofs pass. Mixed-header6870 is non-acceptance diagnostic; fixture-only6879 compile failure preserved. B/C exact, timing/proofs async; not adopted, unfuzzed, full50 target unmet.\\n\\n'
        'Record adverse Sweep-copy memory tradeoff, independent shrink-only timing, and narrow purged-job watcher fix with22tests. Audit6885 binds unchanged campaigns, report, cleanup and handoff. Production unchanged; no denied push retry.',
}.items():
    assert before in code
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
