#!/usr/bin/env python3
"""Exact five-witness comparison or once-only fullwidth ten-task wave."""
from pathlib import Path

def matrix_contract(rows):
    expected = [(wave,rank,arm) for wave in range(3) for rank in (9,25,31,35,38)
                for arm in (('previous','candidate') if wave == 1 else ('candidate','previous'))]
    assert len(rows) == len(expected) == 30
    for task,(row,(wave,rank,arm)) in enumerate(zip(rows,expected)):
        assert int(row['task']) == task and task//10 == wave
        assert int(row['rank']) == rank and row['arm'] == arm
        assert row['expected'] == ('SAT' if rank in (9,25) else 'UNSAT')
        if task%2: assert row['instance'] == rows[task-1]['instance']

def matrix_fixtures():
    rows = [dict(task=str(task),rank=str(rank),arm=arm,instance=str(rank),
                 expected='SAT' if rank in (9,25) else 'UNSAT')
            for task,(rank,arm) in enumerate((rank,arm)
                for wave in range(3) for rank in (9,25,31,35,38)
                for arm in (('previous','candidate') if wave == 1 else ('candidate','previous')))]
    matrix_contract(rows)
    invalid = [rows[:-1],rows[::-1]]
    for task,key,value in ((0,'rank','21'),(0,'expected','UNSAT'),(1,'instance','different'),
                           (10,'arm','candidate'),(20,'task','19')):
        copy = [dict(row) for row in rows]; copy[task][key] = value; invalid.append(copy)
    for bad in invalid:
        try: matrix_contract(bad)
        except AssertionError: pass
        else: raise AssertionError('invalid witness/wave matrix accepted')
    print('Dense realloc matrix: all30 identities, middle-wave reversal, seven invalid controls PASS')

base = Path(__file__).with_name('check-submit-factor-arena-runtime-7186.py')
code = base.read_text()
for before, after in {
    '20260910-7186-factor-arena-runtime':'20260911-7273-dense-realloc-runtime',
    'check-submit-factor-arena-BC-7183.py':'check-submit-dense-realloc-BC-7272.py',
    'h.fingerprint(':'h.raw_fingerprint(',
    "('pending_validation', 'checked_UNSAT')":"('pending_validation', 'checked_SAT', 'checked_UNSAT')",
    "assert len(matrix) == 18 and [int(r['task']) for r in matrix] == list(range(18))":'matrix_contract(matrix)',
    'range(18)':'range(30)',
    'range(0, 18, 2)':'range(0, 30, 2)',
    'i < 6':'i < 10',
    '/18 exact outcomes;':'/30 exact outcomes;',
    'range(6*wave)':'range(10*wave)',
    "f'--array={6*wave}-{6*wave+5}'":"f'--array={10*wave}-{10*wave+9}'",
    'range(6*wave, 6*wave+6)':'range(10*wave, 10*wave+10)',
    'Factor runtime':'Dense realloc runtime',
    '    fixtures()':'    fixtures(); matrix_fixtures()',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),dict(__file__=str(Path(__file__)),__name__='__main__',
    matrix_contract=matrix_contract,matrix_fixtures=matrix_fixtures))
