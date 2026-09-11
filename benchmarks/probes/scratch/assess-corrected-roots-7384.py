#!/usr/bin/env python3
"""Complete three-wave storage assessment; pending proofs never hide solver data."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from inprocessing_cost import paired_costs
def module(name,file):
    spec=importlib.util.spec_from_file_location(name,Path(__file__).with_name(file));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def cost_rows(rows):
    labels={'legacy':'previous','folded':'candidate'}
    assert all(r['arm'] in labels for r in rows)
    return [dict(r,arm=labels[r['arm']]) for r in rows]
def fixtures():
    h=module('distribution','assess-factor-arena-7192.py');h.fixtures()
    a=[dict(arm='legacy',task=0),dict(arm='folded',task=1)]
    assert cost_rows(a)==[dict(arm='previous',task=0),dict(arm='candidate',task=1)]
    assert a[0]['arm']=='legacy' and a[1]['arm']=='folded'
    for arm in ('previous','candidate','unknown'):
        try:cost_rows([dict(arm=arm)])
        except AssertionError:pass
        else:raise AssertionError('unexpected raw arm accepted')
    print('Root assessment: explicit lossless metric labels and three invalid-arm controls PASS')
def main():
    assert sys.argv[1:] in ([],['--self-test'],['--seal']);fixtures()
    if sys.argv[1:]==['--self-test']:return
    c=root/'benchmarks/runs/20260911-7382-corrected-root-runtime'
    target=root/'benchmarks/reports/2026-09-11-corrected-root-assessment-7384.json'
    if '--seal' in sys.argv:assert not target.exists(),'successful assessment frozen'
    reader=Path(__file__).with_name('check-submit-corrected-runtime-7382.py')
    raw=subprocess.check_output([sys.executable,str(reader)],text=True)
    gate=json.loads(raw[raw.index('{'):]);assert gate['cross_arm_storage_only']
    if gate['records']!=12:print('All12 solver outcomes not yet available; no assessment written');return
    hashes={}
    def bind(p,expected=None):
        d=sha(p);assert expected is None or d==expected,p;hashes[str(p.relative_to(root))]=d
    m=json.loads((c/'manifest.json').read_text());bind(c/'manifest.json')
    for rel,digest in m['pin_sha256'].items():bind(c/rel,digest)
    for rel,digest in m['dependencies'].items():bind(root/rel,digest)
    rows=[];states={}
    for t in range(12):
        p=c/f'results/{t:03d}.json';final=p.exists()
        if not final:p=c/f'provisional/{t:03d}.json'
        r=json.loads(p.read_text());s=classify(r,final)
        assert s['status'] in ('pending_validation','checked_SAT','checked_UNSAT')
        rows.append(r);states[t]=s;bind(p)
    costs=paired_costs(cost_rows(rows),[25,31],3,['BVE','Factor','Vivify','Substitute'])
    spreads={}
    for rank in (25,31):
        series={arm:[r['solve']['wall_seconds'] for r in rows if r['rank']==rank and r['arm']==arm] for arm in ('legacy','folded')}
        assert all(len(v)==3 for v in series.values())
        ranges={arm:max(v)/min(v)-1 for arm,v in series.items()}
        spreads[rank]=dict(by_arm=ranges,absolute_headline_stable=max(ranges.values())<=.02)
    for wave in range(3):bind(c/f'launch-wave{wave}.json')
    for p in (Path(__file__),reader,Path(__file__).with_name('assess-factor-arena-7192.py'),root/'tools/bench/inprocessing_cost.py',root/'tools/bench/test_inprocessing_cost.py'):bind(p)
    result=dict(hashes=hashes,expected_outcomes=12,checked_clean=sum(s['checked'] for s in states.values()),states=states,
        runtime=costs,spreads=spreads,cross_arm_classification=gate['pairs'],
        metric_labels={'previous':'corrected legacy7379','candidate':'corrected folded7373'},
        placement=[dict(task=r['task'],rank=r['rank'],arm=r['arm'],node=r['node'],context=r.get('runtime_context')) for r in rows],
        interpretation='All six declared pairs retained, including Rank31 time/RSS regressions. Exact own-repeat fields/certificates and cross-arm non-storage fields under unchanged7350. Physical+removed slots conserved. Report paired medians/full ranges and 2% full-arm spreads, never best-of. No arbitrary minimum gain.',
        decision='Use the complete Rank25 memory/runtime benefit and Rank31 tradeoff to decide the required full50 transfer. Slot timers are rule envelopes, not pure native unit cost or CDCL time.',
        limitation='No full50, final proof completion, fuzz, production promotion or memory parity from this targeted assessment. No reference refresh.')
    for rank,v in costs.items():print(rank,v['paired'],spreads[rank])
    if '--seal' in sys.argv:
        with target.open('x') as out:json.dump(result,out,indent=2)
        print(target)
if __name__=='__main__':main()
