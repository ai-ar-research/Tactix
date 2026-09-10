#!/usr/bin/env python3
"""Classify both terminal observer failures, then shelve this search branch."""
import json
from collections import Counter
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from certificate_prefix import hash_prefix
from search_window import read_search
c=root/'benchmarks/runs/20260910-6804-rank21-analysis-admission'
old=root/'benchmarks/runs/20260909-6798-rank21-queue-bumps'
target=c/'terminal-6813.json';assert not target.exists()
hashes={};states={}
def bind(p,expected=None):
    digest=sha(p);assert expected is None or digest==expected,p
    hashes[str(p.relative_to(root))]=digest
configuration=c/'configuration-6810.json';bind(configuration)
for rel,digest in json.loads(configuration.read_text())['hashes'].items():bind(root/rel,digest)
for window in ('early','late'):
    sub=c/window;assert not (sub/'summary-6806.json').exists()
    m=json.loads((sub/'manifest.json').read_text())
    for rel,digest in m['hashes'].items():bind(sub/rel,digest)
    states[window]={}
    for arm in ('candidate','kissat'):
        p=sub/f'results/{arm}.json';bind(p);r=json.loads(p.read_text())
        control=json.loads((old/f'results/{arm}.json').read_text())
        assert r['exit_code']==(124 if arm=='candidate' else 0) and r['scratch_deleted']
        assert r['binary_sha256']==control['binary_sha256']==m['hashes']['pins/'+arm]
        for name,digest in r['trace_sha256'].items():bind(sub/'traces'/name,digest)
        decisions=sub/f'traces/{arm}.decisions.tsv';n=decisions.stat().st_size
        assert hash_prefix(old/f'traces/{arm}.decisions.tsv',n)['sha256']==sha(decisions)
        state=dict(exit_code=r['exit_code'],scratch_deleted=True,available_decision_bytes=n,
            all_available_decision_bytes_exact=True,accepted_observer=False)
        if arm=='candidate':
            assert r['command'][:2]==['timeout','1200']
            assert not (sub/'traces/candidate.prefix.tsv').exists()
            assert n<(old/'traces/candidate.decisions.tsv').stat().st_size
            state.update(classification='bounded1200s observer timeout, not a solve loss or proof rejection',
                complete50K_replay=False,partial_proof_unvalidated=True)
        else:
            assert r['partial_certificate']==control['partial_certificate']
            assert r['prior_certificate_prefix']==control['prior_certificate_prefix']
            for name in ('kissat.decisions.tsv','kissat.prefix.tsv','kissat.state.tsv','kissat.bumps.tsv'):
                assert r['trace_sha256'][name]==control['trace_sha256'][name],name
            content=(sub/'traces/kissat.search.tsv').read_text();lines=content.splitlines()
            assert lines[-1]=='# truncated at 250000 records'
            rows=[line.split('\t') for line in lines[3:-1]]
            assert len(rows)==250000 and all(len(row)==22 and row[0]=='kissat' and int(row[1])==i for i,row in enumerate(rows))
            try:read_search(content,solver='kissat',window=tuple(m['search_window']),cap=m['trace_row_cap'])
            except ValueError:pass
            else:raise AssertionError('Capped native trace accepted')
            counts=Counter((row[3],row[2]) for row in rows)
            state.update(classification='clean full50K solver replay but capped detail; invalid observer',
                complete50K_decisions_prefix_state_bumps_proof_exact=True,old10K_proof_exact=True,
                search_rows=len(rows),last_event=rows[-1][2],last_conflict=int(rows[-1][3]),
                selected_rows=sum(m['search_window'][0]<=int(row[3])<=m['search_window'][1] for row in rows),
                largest_event_groups=[dict(conflict=int(key[0]),event=key[1],rows=count) for key,count in counts.most_common(8)])
            assert state['last_conflict']==40571 and state['last_event'].startswith('factor_')
        states[window][arm]=state
for p in (Path(__file__),root/'tools/bench/search_window.py',root/'tools/bench/certificate_prefix.py'):bind(p)
out=dict(hashes=hashes,states=states,
    conclusion='Both current legs time out at1200s with exact available decision bytes but missing50K prefix; both native legs finish exact full replay but detail hits250000 rows during out-of-window Factor at40571. Strict6806 remains failed; neither pair is accepted for causal analysis.',
    cause='6810 proves the stop environment key is unrecognized and finite-mode snapshot suppression was inactive. Independently, CONTROL_ONLY admits Factor details emitted outside the selected search window. Correcting the stop key alone is not demonstrated to fix that cap.',
    next_action='User redirected effort to inprocessing unit cost, with scheduling/CDCL parity as safeguards. Do not execute prepared6812 or submit further Rank21 search probes. Preserve accepted6800/6803 frontier and the unresolved CDCL/search-work gap without inferring cause from invalid6804 detail.',
    limitation='No solver soundness defect or final proof rejection is observed. No source/pin mutation, manual deletion or retry occurred; only runner-owned temporary scratch was removed on exit. Current6730 remains isolated/unfuzzed and overall goal unmet.')
with target.open('x') as stream:json.dump(out,stream,indent=2);stream.write('\n')
print(out['conclusion'])
