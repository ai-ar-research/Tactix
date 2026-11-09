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
from python_policy_reference import InprocessClock, InprocessSlot, NativeInprocessSchedule, PhaseOutcome

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
    if row.get('kind') == 'followup':
        schedule = NativeInprocessSchedule(row['config'])
        slot = InprocessSlot('BCE', preprocess=True, followup=row['followup'])
        admitted = schedule.admit_slot(slot, {'CONFLICTS': 0}, congruence_changed=row['congruence'], sweep_changed=row['sweep'])
        assert slot.followup_mode == row['mode'], row
        assert admitted == (row['mode'] != 'None'), row
        coverage.add('followup-' + row['mode'])
        continue
    if row.get('kind') == 'admission':
        schedule = NativeInprocessSchedule(row['config'])
        slot = InprocessSlot(**row['slot'])
        admitted = schedule.admit_slot(slot, row['observation'])
        assert admitted == (not row['decline']), row
        assert (slot.applies, slot.followup_mode, slot.decline) == (row['applies'], row['mode'], row['decline']), row
        coverage.add('admission-' + (slot.decline or 'allowed'))
        continue
    if row.get('kind') == 'budget':
        schedule = NativeInprocessSchedule(row['config'])
        slot = InprocessSlot(**row['slot'])
        schedule.clocks[slot.group].anchor = row['anchor']
        schedule.begin_round()
        for check in row['checks']:
            if check['new_slot']:
                slot = InprocessSlot(check['rule'], group=slot.group, permille=check['permille'], forward_pair=True)
                coverage.add('pair-inherited')
            if check['reset']:
                if slot.permille % 2:
                    schedule.enter_phase(row['observation'], slot.group)
                    coverage.add('phase-budget-reset')
                else:
                    schedule.begin_round()
                    coverage.add('round-reset')
            assert schedule.set_entry_limit(slot, row['observation'], check['meter']) == check['limit'], row
            assert slot.wrapped == check['wrapped'], row
        if slot.rule == 'Factor' and slot.doses and slot.budget_aware:
            coverage.add('factor-later')
        coverage.add('budget-' + slot.rule)
        continue
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
            slot = InprocessSlot('BVE', group='Eliminate', permille=1001)
            assert schedule.set_entry_limit(slot, row['before'], 0, initial_fast_elimination=True) == row['fast_limit'], row
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
                    'followup-None', 'followup-Eager', 'followup-Final',
                    'admission-allowed', 'admission-preprocess', 'admission-disabled', 'admission-cap',
                    'pair-inherited', 'round-reset', 'phase-budget-reset', 'factor-later', 'budget-BCE', 'budget-Substitute', 'budget-Factor',
                    'budget-SubsumeForward', 'budget-Strengthen', 'blocked', 'startup', 'eliminate-equality', 'probe-equality', 'logn-boundary',
                    'reductions-reopen', 'marks-reopen', 'root-reopen', 'material-reopen',
                    'BVE_MARKS-lowered', 'ROOT_FIXED-lowered', 'SUBSUME_MATERIAL-lowered',
                    'zero-clamp', 'probe-ahead', 'equal-rounds',
                    'phase-empty', 'phase-root-growth', 'phase-early-unsat', 'phase-late-unsat',
                    'phase-maximum', 'phase-maximum-growth', 'phase-advance', 'phase-fast', 'phase-material-change',
                    'post-phase-deadline', 'phase-Probe-search', 'phase-Probe-startup',
                    'phase-Eliminate-search'}, coverage
print(f'native/Python phase clocks: {len(rows)} observation cases, all deadline and watermark checks pass')

if str(library) != '-':
    lib = capi.load_library(library)
    rule_ids = {lib.tactix_rule_name(i).decode(): i for i in range(lib.tactix_rule_count())}
    executed = set()
    for line in rows:
        row = json.loads(line)
        if (row.get('kind') != 'phase' or row['before']['CONFLICTS'] or
                row['mode'] not in ('empty', 'material-change')):
            continue
        handle = lib.tactix_init()
        assert handle
        try:
            for clause in ([1, 2], [1, -2], [-1, 3], [-3, 4]):
                for literal in [*clause, 0]: lib.tactix_add(handle, literal)
            assert lib.tactix_status(handle) == 0
            view = capi.SolverView(lib, handle, lambda: None)
            schedule = NativeInprocessSchedule(row['config'])
            schedule.clocks = {name: InprocessClock(**clock) for name, clock in row['clocks'].items()}
            assert schedule.observe(view) == row['before'], row
            def run_rounds(view):
                schedule.begin_round(view)
                if row['mode'] == 'material-change':
                    slot = InprocessSlot('BCE', preprocess=True, max_applies=1)
                    assert schedule.admit_slot(slot, schedule.observe(view))
                    rule = rule_ids['BCE']
                    assert view.tactix_apply_instance(rule, capi.TACTIX_INSTANCE_CANONICAL) in (0, 1)
                return PhaseOutcome()
            assert schedule.execute_pipeline(view, row['group'], run_rounds) == row['result']
            assert schedule.observe(view) == row['after'], row
            assert {name: asdict(clock) for name, clock in schedule.clocks.items()} == row['finished'], row
            executed.add((row['group'], row['mode']))
        finally:
            lib.tactix_release(handle)
    assert executed == {(group, mode) for group in ('Probe', 'Eliminate')
                        for mode in ('empty', 'material-change')}, executed
    print('Python public phase execution matches four actual native pipelines')
