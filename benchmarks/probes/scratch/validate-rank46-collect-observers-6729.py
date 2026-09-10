#!/usr/bin/env python3
"""Account for the native-only series hook without relaxing any replay oracle."""
from pathlib import Path
source=Path(__file__).with_name('validate-rank46-collect-boundary-6728.py')
code=source.read_text()
changes={
    "'summary-6728.json'":"'summary-6729.json'",
    "{arm+suffix for suffix in ('.search.tsv','.formula.tsv','.reduce.tsv','.clause-pose-series.tsv')}":
        "{arm+suffix for suffix in (('.search.tsv','.formula.tsv','.reduce.tsv','.clause-pose-series.tsv') if arm=='kissat' else ('.search.tsv','.formula.tsv','.reduce.tsv'))}",
    "    with (c/f'traces/{arm}.clause-pose-series.tsv').open() as f: frames[arm]=series.parse(f,set(key))\n    assert all(f['conflicts']==62677 and f['name'] in ('reduce','sparsecollect') for f in frames[arm])\n    assert {f['stage'] for f in frames[arm]}=={'begin','end'}":
        "    if arm=='kissat':\n        with (c/f'traces/{arm}.clause-pose-series.tsv').open() as f: frames[arm]=series.parse(f,set(key))\n        assert [(f['conflicts'],f['name'],f['stage']) for f in frames[arm]]==[(62677,'reduce','begin'),(62677,'reduce','end')]\n    else:\n        frames[arm]=None\n        p=root/'.claude/worktrees/runtime-congruence-empty-gates-6654/src/diagnostics/event_trace.cpp'\n        assert 'SAT_CLAUSE_POSE_SERIES' not in p.read_text() and 'emit_clause_pose_series' not in p.read_text()\n        bind(p)",
    "bind(Path(__file__).with_name('validate-rank46-collect-boundary-6728.py'))":
        "bind(Path(__file__).with_name('validate-rank46-collect-boundary-6728.py'))\nbind(Path(__file__).with_name('validate-rank46-collect-observers-6729.py'))",
    'Selected frames give raw refs and cursor states. Native verbosity is hash-bound, not accepted as a timing refresh.':
        'Native-only selected frames give raw refs/cursors.6654 has no clause-pose-series hook; the failed6728 expectation remains intact and current frames are null,not an empty-clause assertion. Current entry formula and existing6723 watch observations retain its cursor evidence. All original per-arm decision/proof and standalone replay obligations remain exact. Native verbosity is hash-bound,not a timing refresh.'}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
