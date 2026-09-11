#!/usr/bin/env python3
"""Interpret the complete predeclared C1..82 core, not the capped C85 tail."""
import importlib.util
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT/'tools/bench'))
from search_window import read_search,FIELDS
from fixed_runtime import sha


def complete_core(body,*,solver,window,cap,core_end=82):
    lines=body.splitlines()
    assert body.endswith('\n') and window[0]<=core_end<window[1]
    marker=f'# truncated at {cap} records'
    truncated=lines[-1]==marker
    raw=lines[3:-1] if truncated else lines[3:]
    assert not any(line.startswith('#') for line in raw)
    assert len(raw)==cap if truncated else len(raw)<cap
    previous=-1;first_after=None
    for i,line in enumerate(raw):
        fields=line.split('\t')
        assert len(fields)==len(FIELDS) and fields[0]==solver and fields[1]==str(i)
        conflict=int(fields[3]);assert conflict>=previous;previous=conflict
        if first_after is None and conflict>core_end:first_after=i
    assert first_after is not None and first_after>0, 'no observed passage beyond core'
    if truncated:assert int(raw[-1].split('\t')[3])>core_end, 'cap intersects core'
    # Preserve the raw header and every complete row before the first later
    # conflict. This is an explicit analysis view, not a replacement raw trace.
    prefix='\n'.join(lines[:3]+raw[:first_after])+'\n'
    return read_search(prefix,solver=solver,window=window,cap=cap)


def fixtures():
    def row(i,c,e='analysis_end'):
        values=['0']*len(FIELDS)
        values[0]='kissat';values[1]=str(i);values[2]=e;values[3]=str(c)
        values[7]='focused';values[18]='0'*16;values[20]='-';values[21]='0'
        return '\t'.join(values)
    header='# tactix-event-trace\t1\n# detail-conflicts\t1\t85\n# '+'\t'.join(FIELDS)+'\n'
    body=header+row(0,1)+'\n'+row(1,82)+'\n'+row(2,83)+'\n'
    assert len(complete_core(body,solver='kissat',window=(1,85),cap=4))==2
    capped=body+'# truncated at 3 records\n'
    assert len(complete_core(capped,solver='kissat',window=(1,85),cap=3))==2
    bad=[body[:-1],body.replace('1\tanalysis_end\t82','4\tanalysis_end\t82'),
         body.replace('2\tanalysis_end\t83','2\tanalysis_end\t81'),
         body.replace('2\tanalysis_end\t83','2\tanalysis_end\t82'),
         body+'# unexpected\n',capped.replace('3 records','4 records')]
    for sample in bad:
        try:complete_core(sample,solver='kissat',window=(1,85),cap=4)
        except (AssertionError,ValueError):pass
        else:raise AssertionError('invalid core accepted')
    print('Complete-core view: two positive and six boundary/cap/framing controls PASS')


def main():
    fixtures()
    source=Path(__file__).with_name('validate-rank43-transactions-7404.py')
    code=source.read_text()
    changes={
        "TARGET=C/'solver-summary-7404.json'":"TARGET=C/'core-summary-7406.json'",
        'def transactions(rows,core_end=82):':'def transactions(rows,core_end=82,terminal_end=85):',
        "result[-1]['end']['conflicts']==85":"result[-1]['end']['conflicts']==terminal_end",
        "rows=read_search(sp.read_text(),solver='tactix' if arm=='candidate' else 'kissat',window=(1,85),cap=m['row_cap'])":
        "rows=complete_core(sp.read_text(),solver='tactix' if arm=='candidate' else 'kissat',window=(1,85),cap=m['row_cap'])",
        'txs[arm]=transactions(selected)':'txs[arm]=transactions(selected,terminal_end=82)',
        "observer_replay_validated=True,core_conflicts=[1,82]":"observer_replay_validated=True,core_conflicts=[1,82],full_window_1_85_accepted=False,core_helper_sha256=sha(Path(__file__)),core_helper=str(Path(__file__).relative_to(ROOT))",
        "Original7401 native cap and invalid per-integer learned-event assumption remain preserved. Full uncapped traces, complete own work/proof/decisions, exact captured native prefix and balanced transactions now required. Internal OTFS advances and reused-conflict paths are explicit. Native missing C84 conflict payload is outside the C1..82 causal core and is not accepted as formula equality. Different hook emissions are not automatically semantic differences or defects; no timing/strength claim.":
        "Both full C1..85 native trace attempts remain rejected:250000 and500000 caps fill at C85. This distinct view accepts only the predeclared C1..82 causal core, whose exact complete rows precede an observed later conflict and both cap points. Full solver work/proof/decision replay and exact250000 native prefix still required. All core analysis transactions close; OTFS counter advances and reused-conflict paths explicit. Missing C84 native conflict payload is outside this view, not waived as equal formula. No full-window, timing or strength claim; frozen7402/7404 validators unchanged."
    }
    for before,after in changes.items():
        assert code.count(before)==1,before
        code=code.replace(before,after)
    verbose="print(json.dumps(dict(records=result,first_transaction_differences=comparisons),indent=2))"
    assert code.count(verbose)==1
    code=code.replace(verbose,"print('Core replay PASS:', {a: (r['rows'],r['transactions']) for a,r in result.items()}); print('First differing transaction indices:', {k:None if v is None else v['index'] for k,v in comparisons.items()})")
    scope={'__file__':str(Path(__file__)),'__name__':'core7406','complete_core':complete_core}
    exec(compile(code,str(source),'exec'),scope)
    scope['fixtures']()
    assert sys.argv[1:] in ([],['--self-test'],['--seal'])
    if sys.argv[1:]!=['--self-test']:scope['validate'](sys.argv[1:]==['--seal'])


if __name__=='__main__':main()
