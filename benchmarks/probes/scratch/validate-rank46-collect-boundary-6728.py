#!/usr/bin/env python3
"""Replay-check all boundary observers before attributing the cursor reset."""
from pathlib import Path
source=Path(__file__).with_name('validate-rank49-search-window-6641.py')
code=source.read_text()
changes={
    '20260909-6639-rank49-search-window':'20260909-6726-rank46-collect-boundary',
    'summary-6641.json':'summary-6728.json',
    "assert extra['extra_counts'] == dict(trace_begin=1, retier=15, checkpoint=90)":
        "assert extra['extra_counts'].get('trace_begin') == 1\n            assert set(extra['extra_counts']) <= {'trace_begin','retier','checkpoint'}",
    "assert r['partial_certificate'] == old['partial_certificate']":
        "assert r['partial_certificate'] == old['partial_certificate']\n    assert r['prior_certificate_prefix'] == old['prior_certificate_prefix']",
    "assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm + '.search.tsv'}":
        "assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm+suffix for suffix in ('.search.tsv','.formula.tsv','.reduce.tsv','.clause-pose-series.tsv')}",
    'rank=49, stop=100000':'rank=46, stop=1000000',
    'Manifest expected current macro byte identity; global tracing instead adds106 source-backed macro rows: trace_begin1,retier15,checkpoint90. The new tested subsequence validator proves every22699 original row unchanged except sequence shifts; extras retained in full. Native allows only analysis/end within the declared window. No solver failure or relaxed certificate oracle.':
        'Full own1M decisions/prefix/partial proof and prior100K proof prefix remain exact. Only documented macro additions and native analysis/end in62676..62681 allowed. New formula/pose-series/Reduce observers and native verbosity do not change the original solver replay.'}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
anchor="with target.open('x') as stream:"; assert code.count(anchor)==1
code=code.replace(anchor,"""def module(name,filename):
    p=Path(__file__).with_name(filename); bind(p)
    spec=importlib.util.spec_from_file_location(name,p)
    result=importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result
formula=module('formula_parser','compare-rank32-formula-5785.py'); formula.fixtures()
series=module('pose_parser','validate-clause-pose-series-6009.py'); series.fixtures()
from compare_reduce_trace import compare as compare_reduction
def formula_row(line):
    fields=line.rstrip('\\n').split('\\t')
    assert len(fields)==13 and fields[-1] in ('garbage=0','garbage=1')
    return dict(state=formula.parse_line('\\t'.join(fields[:-1])),garbage=int(fields[-1][-1]))
good='0\\t3\\t0000000000000001\\t-3,1,2\\tpose=1,-3,2\\tglue=0\\tpriority=0\\tsearched=2\\tused=0\\treason=0\\tshrunken=0\\tvalues=-3:-1,1:0,2:0\\tgarbage='
assert not formula_row(good+'0')['garbage'] and formula_row(good+'1')['garbage']
for bad in ('2','', '0\\tgarbage=1'):
    try: formula_row(good+bad)
    except (AssertionError,ValueError): pass
    else: raise AssertionError('invalid garbage extension accepted')
key=tuple(map(int,m['reason_key'].split(','))); frames={}; inventory={}
for arm in arms:
    with (c/f'traces/{arm}.clause-pose-series.tsv').open() as f: frames[arm]=series.parse(f,set(key))
    assert all(f['conflicts']==62677 and f['name'] in ('reduce','sparsecollect') for f in frames[arm])
    assert {f['stage'] for f in frames[arm]}=={'begin','end'}
    with (c/f'traces/{arm}.formula.tsv').open() as f:
        assert next(f).rstrip()=='# '+('tactix' if arm=='candidate' else arm)+'-formula-set\\t2'
        assert next(f).rstrip()=='# redundant\\tsize\\thash\\tliterals'
        records=[formula_row(line) for line in f]
    assert records
    selected=[dict(index=i,**r) for i,r in enumerate(records) if r['state'][0][1]==key]
    assert selected
    first_red=next((dict(index=i,**r) for i,r in enumerate(records) if r['state'][0][0]),None)
    inventory[arm]=dict(total=len(records),garbage=sum(r['garbage'] for r in records),
        live_counts=dict(Counter(r['state'][0][0] for r in records if not r['garbage'])),
        selected=selected,first_redundant_record=first_red)
    bind(c/f'logs/{arm}.out')
bind(root/'tools/bench/compare_reduce_trace.py')
bind(Path(__file__).with_name('validate-rank46-collect-boundary-6728.py'))
out.update(selected_clause_frames=frames,formula_inventory=inventory,
    reduction_comparison=compare_reduction(c/'traces/candidate.reduce.tsv',c/'traces/kissat.reduce.tsv'),
    boundary_limitation='Formula rows preserve physical iteration order, including garbage; no assumption that physical rows equal logical replacement positions. Selected frames give raw refs and cursor states. Native verbosity is hash-bound, not accepted as a timing refresh.')
"""+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
