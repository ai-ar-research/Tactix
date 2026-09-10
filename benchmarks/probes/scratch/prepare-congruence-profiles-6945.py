#!/usr/bin/env python3
"""A single paired Rank47 profile, isolated from Factor timing and references."""
from pathlib import Path
source = Path(__file__).with_name('prepare-inprocessing-profiles-6815.py')
code = source.read_text()
for before, after in {
    '20260910-6815-inprocessing-release-profiles':'20260910-6945-congruence-release-profiles',
    '20260910-6814-inprocessing-cost-census':'20260910-6944-congruence-cost-selection',
    "s['selected_ranks']==[32,39]":"s['selected_ranks']==[47]",
    '(32,39)':'(47,)',
    "[('32','tactix'),('32','kissat'),('39','tactix'),('39','kissat')]":
        "[('47','tactix'),('47','kissat')]",
    '--array=0-3':'--array=0-1',
    "array='0-3'":"array='0-1'",
    '6815-inprocess-profiles':'6945-congruence-profiles',
    'User-directed inprocessing unit-cost focus on current6730: Rank32 broad exact-full-conflict/decision control and Rank39 Sweep-heavy UNSAT control.':
        'Congruence unit-cost diagnosis on Rank47, selected6944 from existing checked41-50 records. Current/nativefive applications but differing gates/merges/search work; require own-pin replay, not cross-arm work identity.',
    'Existing count-cache neutral result and unsupported Factor rewrite remain rejected as speedup explanations.':
        'Isolated Factor6925 timing proceeds independently; do not compose an unaccepted source or change its gate.',
    'All four expected UNSAT':'Both expected UNSAT',
    'Rank32 counters match native but do not prove whole-formula/propagation identity. Rank39 differs in work; percentages alone are not unit prices.':
        'Rank47 differs in work; percentages and nestednativeCPU/currentwall timers alone are not unit prices.42/43 extra closure/search work remains open.',
    'Five new timer tests and seven source-key-preflight tests; maintained benchmark suite244 and watcher19. Sampling wrapper copied byte-for-byte from validated5544. Exact four-task matrix and within-arm replay protect unchanged protocol; no global event trace or stop setting is enabled.':
        'Unchanged maintained timer/counter/classification readers;262 benchmark/22 watcher tests. Byte-identical5544 sampling wrapper, exacttwo-task matrix and complete within-arm work/cert replay. No globaltrace or stop. No new parser behavior or source edit.',
    'One diagnostic four-task array':'One diagnostic two-task array',
    "m['prepare_failure']='First unsubmitted attempt failed before manifest publication because current6743 owns protected hashes in launch.json, not manifest.json. Partial files preserved in sibling prepare-attempt1; no solver ran and no accepted artifact was changed.'":
        "m['provenance_note']='New6945 uses the corrected6815 preparation logic; no failed scaffold or solver retry in this campaign. Native denominator unchanged.'",
}.items():
    assert before in code, before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
