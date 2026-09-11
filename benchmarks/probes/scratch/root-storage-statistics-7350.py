#!/usr/bin/env python3
"""Classify storage telemetry explicitly; never turn a changed solve into parity.

main.cpp emits physical nvars, cumulative removed slots and currently retained
extension slots (despite saying 'added'). Preserve retirement categories,
compaction pass counts, all decision counts and every other non-timing line.
This is a diagnostic classification, not a replacement acceptance gate.
"""
import re
PATTERNS = {
    'retired': re.compile(r'c retired     (\d+) vars \((\d+) substituted, (\d+) eliminated\) of (\d+)'),
    'compact': re.compile(r'c compact     (\d+) passes removed (\d+) internal variables'),
    'extension': re.compile(r'c extension   (\d+) vars added, (\d+) decisions on them \((\d+\.\d+)% of (\d+)\)'),
    'denseocc': re.compile(r'c denseocc    (\d+) slice moves, (\d+) defrags'),
}
def describe(record):
    lines = record['solver_statistics']; assert isinstance(lines,list) and lines
    assert all(isinstance(line,str) for line in lines)
    found, normalized = {}, []
    for line in lines:
        if line.startswith('c timing'): continue
        key = next((key for key in PATTERNS if line.startswith('c '+key+' ')),None)
        if key is None: normalized.append(line); continue
        assert key not in found,('duplicate telemetry',key)
        match=PATTERNS[key].fullmatch(line); assert match,('malformed telemetry',line)
        fields=match.groups(); found[key]=fields
        keep={'retired':fields[:3],'compact':fields[:1],
              'extension':fields[1:],'denseocc':()}[key]
        normalized.append((key,keep))
    assert set(found)==set(PATTERNS),('missing telemetry',set(PATTERNS)-set(found))
    assert int(found['retired'][0])==int(found['retired'][1])+int(found['retired'][2])
    signature=tuple(record[key] for key in ('verdict','instance','certificate_sha256','certificate_bytes'))+(tuple(normalized),)
    return signature,dict(physical_vars=int(found['retired'][3]),removed_slots=int(found['compact'][1]),
        compact_passes=int(found['compact'][0]),retained_extensions=int(found['extension'][0]),
        dense_moves=int(found['denseocc'][0]),dense_defrags=int(found['denseocc'][1]))
def compare(a,b):
    sa,pa=describe(a); sb,pb=describe(b)
    conserved=pa['physical_vars']+pa['removed_slots']==pb['physical_vars']+pb['removed_slots']
    return dict(storage_only=sa==sb and conserved,slot_total_conserved=conserved,current=pa,candidate=pb)
def fixtures():
    lines=['c decisions   20 (0 via API)','c retired     3 vars (1 substituted, 2 eliminated) of 100',
           'c compact     2 passes removed 10 internal variables',
           'c extension   9 vars added, 4 decisions on them (20.00% of 20)',
           'c denseocc    8 slice moves, 0 defrags']
    a=dict(verdict='UNSAT',instance='x',certificate_sha256='a',certificate_bytes=1,solver_statistics=lines)
    changed=[line.replace('of 100','of 90').replace('removed 10','removed 20').replace('9 vars added','5 vars added').replace('8 slice moves','12 slice moves') for line in lines]
    b=dict(a,solver_statistics=changed); assert compare(a,b)['storage_only']
    for before,after in [('20 (0 via API)','21 (0 via API)'),('3 vars (1','4 vars (2'),('2 passes','3 passes'),
                         ('4 decisions','5 decisions'),('removed 20','removed 21')]:
        altered=[line.replace(before,after) for line in changed]
        assert altered!=changed and not compare(a,dict(b,solver_statistics=altered))['storage_only']
    for key,value in [('verdict','SAT'),('instance','y'),('certificate_sha256','b'),('certificate_bytes',2)]:
        assert not compare(a,dict(b,**{key:value}))['storage_only']
    assert not compare(a,dict(b,solver_statistics=changed+['c new work 1']))['storage_only']
    for invalid in [changed[:-1],changed+[changed[-1]],[s+' trailing' if s.startswith('c denseocc') else s for s in changed]]:
        try: describe(dict(b,solver_statistics=invalid))
        except AssertionError: pass
        else: raise AssertionError('invalid telemetry accepted')
    print('root storage telemetry: conserved slots, ten changed-work/certificate controls and three schema failures PASS')
if __name__=='__main__': fixtures()
