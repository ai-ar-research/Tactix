#!/usr/bin/env python3
"""Extend current semantic Rank32 beyond the checked 300K root-lock closure."""
from pathlib import Path
source = Path(__file__).with_name('prepare-priority-prefixes-6139.py')
code = source.read_text()
changes = {
    '((6139, 21, 10000), (6140, 4, 100000))': '((6183, 32, 1000000),)',
    "f'benchmarks/runs/20260908-{number}-rank{rank}-current-prefix'": "f'benchmarks/runs/20260909-{number}-rank{rank}-current-prefix'",
    'stream_from=0, common_decisions=1': 'stream_from=300000, common_decisions=69828932',
    'Rank4bothfreshUNSATcertificates pass;useashortcurrentprefix todistinguish changedwork fromunitcost beforeoptimizing.':
    'Rank32 root-lock6049 matches all69828932 decisions through300K. Full6107 proof/non-timing stats exactly equal6049. Extend current semantic/native decisions through1M to find the first later portable divergence. No acceptance timing.',
    'Both exactrequested stops,completecontiguous decisionstreams from0,hash-bound partialproofs/traces andclean scratch. Report firstmismatch withoutinferringcause fromlatercounts. Initialone-decision hash isnot afullprefix claim.':
    'Require first69828932 rolling decision hash10d4b4ca139060f5 onboth arms,then compare every streamed row from300K through1M. Bind old300K closure and both full6049/6107 proof/work records. Exactstop/cleanup and allboundaries required. If earlyhash changes,do not assume the cached closure; localizeearlier.',
}
for old, new in changes.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
marker = "    (c / 'run.py').write_text(code)"
assert code.count(marker) == 1
code = code.replace(marker, """    anchor = \"m['common_decisions'] if arm == 'candidate' else 1\"
    assert code.count(anchor) == 1
    code = code.replace(anchor, \"m['common_decisions']\")
""" + marker)
marker = "    m['hashes'] = {str(p.relative_to(c)): sha(p) for p in"
assert code.count(marker) == 1
code = code.replace(marker, """    bound = [root / 'benchmarks/runs/20260908-6060-root-lock-rank32-prefix/summary.json',
             root / 'benchmarks/runs/20260908-6061-root-lock-ranks1-40/results/031.json',
             root / 'benchmarks/runs/20260908-6115-substitute-publication-ranks1-50/results/031.json']
    before, after = [json.loads(p.read_text()) for p in bound[1:]]
    nt = lambda r: [s for s in r['solver_statistics'] if not s.startswith('c timing')]
    assert before['certificate_sha256'] == after['certificate_sha256'] and nt(before) == nt(after)
    m['earlier_closure'] = {str(p.relative_to(root)): sha(p) for p in bound}
    m['scope'] = 'Trace6127 represents semantic6107,not new sorting binary6169. New full50 must replay that parent before inheriting this localization. Missing native1M prefix is diagnostic only,not fixed-reference refresh.'
""" + marker)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
