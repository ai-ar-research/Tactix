#!/usr/bin/env python3
"""Preserve strict5566 and account explicitly for broad-trace-only journal rows."""
from collections import Counter
from pathlib import Path
import sys

def journal_rows(path):
    result = []
    for line in path.read_text().splitlines():
        if not line or line.startswith('#'): continue
        fields = line.split('\t')
        assert int(fields[1]) == len(result)
        result.append(fields[:1] + fields[2:])
    return result

def project(current, previous, select, expected):
    extra = [r for r in current if select(r)]
    assert Counter(r[1] for r in extra) == expected
    assert [r for r in current if not select(r)] == previous
    return extra

def fixtures():
    old = [['tactix','start','0'],['tactix','end','2']]
    added = ['tactix','checkpoint','1']
    choose = lambda r:r[1]=='checkpoint'
    assert project([old[0],added,old[1]],old,choose,{'checkpoint':1}) == [added]
    assert project(old,old,choose,{}) == []
    for bad in ([old[1],added,old[0]],[old[0],old[1]],
                [old[0],added,added,old[1]],[old[0],['tactix','other','1'],old[1]]):
        try: project(bad,old,choose,{'checkpoint':1})
        except AssertionError: pass
        else: raise AssertionError('Changed journal accepted')
    print('PASS: two positive/four negative journal-projection fixtures',flush=True)

fixtures()
if '--self-test' in sys.argv: raise SystemExit(0)
source = Path(__file__).with_name('compare-small-factor-5566.py').read_text()
anchor = "            assert sha(parent / 'traces' / name) == digest == r['trace_sha256'][name]"
assert source.count(anchor) == 1
source = source.replace(anchor, '''
            assert sha(parent / 'traces' / name) == digest
            if digest != r['trace_sha256'][name]:
                assert c.name == '20260908-5564-rank32-small-factor'
                before = journal_rows(parent / 'traces' / name)
                after = journal_rows(c / 'traces' / name)
                if name == 'candidate.macro.tsv':
                    extra = project(after,before,
                        lambda row:row[1] in ('trace_begin','retier','checkpoint'),
                        {'trace_begin':1,'retier':15,'checkpoint':89})
                else:
                    assert name == 'kissat.boundaries.tsv'
                    extra = project(after,before,lambda row:row[1]=='analysis',{'analysis':1})
                    assert extra == [['kissat','analysis','end','19375','18522616',
                        '224211025','159100','134588','24531','594773','3986','213567']]
                assert all(int(row[2] if name.endswith('macro.tsv') else row[3]) <= 100000 for row in extra)
'''.rstrip())
source = source.replace('parser_fixtures=19, evidence=evidence,',
    "parser_fixtures=25, observer_projection={'candidate_macro':{'trace_begin':1,'retier':15,'checkpoint':89},'native_boundary':{'analysis_end_at19375':1},'all_prior_rows':'equal in original order after removing only these observer-only event families; all complete decisions, prefix and partial proof bytes equal per arm'}, evidence=evidence,")
exec(compile(source,__file__,'exec'))
