"""Compare Python native phase clocks with C++ using public observations.

This qualifies scheduling components, not the unfinished full DefaultPolicy.
"""
from pathlib import Path
import json
import copy
from dataclasses import asdict
import subprocess
import sys

library, fixture, root = map(Path, sys.argv[1:])
sys.path.insert(0, str(root/'python'))
from tactix import Session, UNKNOWN, capi
from python_policy_reference import InprocessClock, NativeInprocessSchedule

observed = []
def inspect(execution, restored):
    before = NativeInprocessSchedule.observe(execution.c)
    assert before['BVE_MARKS'] != (1 << 64) - 1
    assert before['SUBSUME_MATERIAL'] != (1 << 64) - 1
    for _ in range(8): assert NativeInprocessSchedule.observe(execution.c) == before
    observed.append(before)
    return UNKNOWN

if str(library) != '-':
    with Session(library, strategy=inspect) as session:
        session.add([1, 2, 3])
        assert session.solve().status == UNKNOWN
    assert len(observed) == 1

coverage = set()
rows = subprocess.check_output([str(fixture)], text=True).splitlines()
for line in rows:
    row = json.loads(line)
    if row.get('kind') == 'phase':
        schedule = NativeInprocessSchedule(row['config'])
        schedule.initialized = True
        schedule.clocks = {group: InprocessClock(**clock) for group, clock in row['clocks'].items()}
        group = row['group']
        schedule.enter_phase(row['before'], group)
        assert not schedule.clocks[group].blocked
        assert schedule.clocks[group].material_mark == row['clocks'][group]['material_mark']
        stale = copy.deepcopy(schedule)
        rescheduling = []
        def reschedule_bve():
            rescheduling.append(asdict(schedule.clocks[group]))
            return 1  # Public support no-op is accepted, as native does.
        flags = dict(reschedule_bve=reschedule_bve, bve_ran=row['bve_ran'], maximum_bound=row['maximum_bound'],
                     initial_fast_elimination=row['initial_fast_elimination'],
                     unsat=row['result'] == 20)
        reschedule = False if row['early'] else schedule.leave_phase(row['after'], group, **flags)
        assert reschedule == (row['mode'] == 'advance'), row
        if row['initial_fast_elimination']:
            schedule.finish_initial_fast_elimination(reschedule_bve)
        assert len(rescheduling) == int(reschedule or row['initial_fast_elimination']), row
        if reschedule:
            assert rescheduling[0]['marks_mark'] == row['before']['BVE_MARKS']
            assert rescheduling[0]['fixed_mark'] == row['before']['ROOT_FIXED']
            assert rescheduling[0]['material_mark'] == row['clocks'][group]['material_mark']
        for name, clock in schedule.clocks.items():
            assert asdict(clock) == row['finished'][name], row
        coverage.add('phase-'+row['mode'])
        if row['mode'] == 'material-change':
            stale.leave_phase(row['before'], group, **flags)
            assert stale.clocks[group].next_conflicts != schedule.clocks[group].next_conflicts
            coverage.add('post-phase-deadline')
        if row['mode'] == 'empty':
            coverage.add('phase-'+group+('-search' if row['before']['CONFLICTS'] else '-startup'))
        continue
    observation = row['observation']
    schedule = NativeInprocessSchedule(row['config'])
    schedule.initialized = row['initialized']
    schedule.clocks = {group: InprocessClock(**clock) for group, clock in row['clocks'].items()}
    if row['reduced']: schedule.note_reduce(observation)
    for check in row['checks']:
        group = check['group']
        due = schedule.want_phase(observation, group)
        clock = schedule.clocks[group]
        assert due == check['due'], row
        assert (clock.blocked, clock.blocked_episodes) == (check['blocked'], check['episodes']), row
        coverage.add((group, due))
        if check['episodes']: coverage.add('blocked')
        if due and group == 'Probe' and not observation['CONFLICTS']: coverage.add('startup')
        if due and group == 'Eliminate' and observation['CONFLICTS'] == 99: coverage.add('eliminate-equality')
        entered = (row['initialized'] and not row['reduced'] and
                   not row['config']['preprocess_only'])
        initial = row['clocks'][group]
        if (entered and not row['config']['watermarks'] and group == 'Probe' and
            initial['rounds'] and observation['CONFLICTS'] == initial['next_conflicts']):
            assert not due
            coverage.add('probe-equality')
        if entered and row['config']['watermarks'] and initial['rounds'] and observation['CONFLICTS'] > initial['next_conflicts']:
            if group == 'Probe' and observation['REDUCTIONS'] != initial['reductions_mark']:
                assert due
                coverage.add('reductions-reopen')
            if group == 'Eliminate':
                growth = [observation[name] > initial[field] for name, field in
                          [('BVE_MARKS', 'marks_mark'), ('ROOT_FIXED', 'fixed_mark'),
                           ('SUBSUME_MATERIAL', 'material_mark')]]
                for index, name in enumerate(['marks-reopen', 'root-reopen', 'material-reopen']):
                    if growth[index] and sum(growth) == 1:
                        assert due
                        coverage.add(name)
                if not any(growth):
                    assert not due
                    for name, field in [('BVE_MARKS', 'marks_mark'), ('ROOT_FIXED', 'fixed_mark'),
                                        ('SUBSUME_MATERIAL', 'material_mark')]:
                        if observation[name] < initial[field]: coverage.add(name+'-lowered')
    if not row['config']['split_clocks']:
        probe, eliminate = (row['clocks'][group]['rounds'] for group in ('Probe', 'Eliminate'))
        if probe == eliminate == 0: coverage.add('zero-clamp')
        if probe > eliminate: coverage.add('probe-ahead')
        if probe == eliminate and probe: coverage.add('equal-rounds')
    for group, expected in row['limits'].items():
        schedule.update_limit(observation, group)
        assert schedule.clocks[group].next_conflicts == expected, row
    if observation['IRREDUNDANT_CLAUSES'] == 22575:
        assert schedule.clocks['Probe'].next_conflicts == 8278
        coverage.add('logn-boundary')
assert coverage >= {('Probe', False), ('Probe', True), ('Eliminate', False), ('Eliminate', True),
                    'blocked', 'startup', 'eliminate-equality', 'probe-equality', 'logn-boundary',
                    'reductions-reopen', 'marks-reopen', 'root-reopen', 'material-reopen',
                    'BVE_MARKS-lowered', 'ROOT_FIXED-lowered', 'SUBSUME_MATERIAL-lowered',
                    'zero-clamp', 'probe-ahead', 'equal-rounds',
                    'phase-empty', 'phase-root-growth', 'phase-early-unsat', 'phase-late-unsat',
                    'phase-maximum', 'phase-maximum-growth', 'phase-advance', 'phase-fast', 'phase-material-change',
                    'post-phase-deadline', 'phase-Probe-search', 'phase-Probe-startup',
                    'phase-Eliminate-search'}, coverage
print(f'native/Python phase clocks: {len(rows)} observation cases, all deadline and watermark checks pass')
