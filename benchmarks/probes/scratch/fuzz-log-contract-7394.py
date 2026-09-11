#!/usr/bin/env python3
"""Read terminal broad-fuzz coverage without counting timeouts as checked cases."""
import re


def parse(body,spec):
    corpus=re.findall(r'^corpus: (\d+) entries, (\d+) failures$',body,re.M)
    assert len(corpus)==1 and int(corpus[0][0])>0 and int(corpus[0][1])==0
    summaries=re.findall(r'^fuzz: (\d+) cases, (\d+) failures \| verdicts: (.*?) \| coverage: (\d+)/(\d+) cases reached a verdict \|',body,re.M)
    assert len(summaries)==1, 'missing or duplicate terminal summary'
    n,failures,verdicts,checked,denominator=summaries[0]
    n,failures,checked,denominator=map(int,(n,failures,checked,denominator))
    assert n==denominator==10000 and failures==0
    pairs=[piece.split('=') for piece in verdicts.split()]
    assert len({p[0] for p in pairs})==len(pairs)
    counts={k:int(v) for k,v in pairs}
    assert set(counts)<= {'SAT','UNSAT','TIMEOUT','UNKNOWN'} and sum(counts.values())==n
    assert checked==counts.get('SAT',0)+counts.get('UNSAT',0)
    assert checked>=spec['minimum_verdicts'] and (n-checked)/n<=spec['max_nonterminating_frac']
    reach=re.findall(r'^fuzz: reached \(cases firing each rule, of (\d+) with stats;.*?\): (.*)$',body,re.M)
    assert len(reach)==1 and int(reach[0][0])==checked
    values=dict(re.findall(r'(\w+)=(\d+)[!?]*',reach[0][1]))
    for rule in spec.get('require_reached','').split(','):
        if rule: assert rule in values and int(values[rule])>0,rule
    assert 'fuzz: REACH-SPEC MISS' not in body and 'fuzz: REQUIRED RULES NEVER FIRED' not in body
    assert not re.search(r'^fuzz: (?:non-terminating fraction|verdict coverage).* (?:exceeds|below)',body,re.M)
    assert re.search(r'^fuzz: stress target applications=\d+$',body,re.M)
    assert 'logical-cpus=100 physical-cores=50 workers=50 mem=400000M' in body
    return dict(corpus_entries=int(corpus[0][0]),generated=n,checked_verdicts=checked,
        lost_coverage=n-checked,verdicts=counts,reachability={k:int(v) for k,v in values.items()},
        corpus_crash_only_rows=[line for line in body.splitlines() if line.startswith('CORPUS crash-oracle only')])


def fixtures():
    body=('logical-cpus=100 physical-cores=50 workers=50 mem=400000M\n'
        'corpus: 30 entries, 0 failures\n'
        'fuzz: 10000 cases, 0 failures | verdicts: SAT=5000 TIMEOUT=100 UNSAT=4900 | coverage: 9900/10000 cases reached a verdict | rest\n'
        'fuzz: reached (cases firing each rule, of 9900 with stats; ! = never): Lucky=7 BVE=2\n'
        'fuzz: stress target applications=0\n')
    spec=dict(minimum_verdicts=9000,max_nonterminating_frac=.1,require_reached='Lucky,BVE')
    assert parse(body,spec)['checked_verdicts']==9900
    bad=[body.replace('10000 cases, 0 failures','10000 cases, 1 failures'),
         body.replace('30 entries, 0 failures','30 entries, 1 failures'),
         body.replace('coverage: 9900/','coverage: 10000/'),body.replace('BVE=2','BVE=0!'),
         body.replace('workers=50','workers=2'),body.replace('SAT=5000','SAT=4999'),
         body+body,body.replace('fuzz: stress target applications=0\n','')]
    for invalid in bad:
        try: parse(invalid,spec)
        except AssertionError: pass
        else: raise AssertionError('invalid terminal fuzz coverage accepted')
    print('Terminal fuzz coverage: eight rejection controls PASS')


if __name__=='__main__': fixtures()
