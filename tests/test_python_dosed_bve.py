"""Independent ordinary BVE scheduling versus real native run_phase."""
from pathlib import Path
import ctypes as C
import importlib.util
import json
import subprocess
import sys
import tempfile
import types
from unittest.mock import patch

library, fixture, observations, root = map(Path, sys.argv[1:])
sys.path[:0] = [str(root/'python'), str(root/'tests'), str(root/'tools/bench')]
from tactix import BveOrder, Execution, Session, ScriptedCDCL, capi
from python_policy_reference import NativeInprocessSchedule, InprocessSlot, PhaseOutcome
from test_native_query_proof import formula
from test_native_query_append_proof import satisfiable
from proof_oracle import certificate_route
spec = importlib.util.spec_from_file_location('dosed_bve_proofs', root/'tools/bench/run.py')
runner = importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
for tool in (runner.VERIPB, runner.CAKE_PB_CNF, runner.DSR_TRIM,
             runner.DSR_COMPRESS, runner.SR_CHECK, runner.CAKE_LPR):
    assert Path(tool).is_file(), tool
lib = capi.load_library(library)
rules = {lib.tactix_rule_name(i).decode(): i for i in range(lib.tactix_rule_count())}
context = types.SimpleNamespace(dll=lib, rules=rules, _callback_depth=0)
stat_names = ('BVE_ELIMINATED','BVE_RESOLUTIONS','PROPAGATIONS','TICKS','ROOT_FIXED',
              'ACTIVE_VARIABLES','BVE_MARKS','ARENA_WORDS','SEARCH_TICKS','DECISIONS','PROBE_TICKS')
checked = models = phases = 0
canonical = None
with tempfile.TemporaryDirectory(prefix='python-dosed-bve-') as scratch:
    directory = Path(scratch)
    for mode, fmt in [('none', None), ('dsr', capi.TACTIX_PROOF_DSR_ASCII), ('pbp', capi.TACTIX_PROOF_PBP)]:
        prefix = directory/('native-'+mode)
        rows = [json.loads(line) for line in subprocess.check_output([str(fixture), str(prefix), mode], text=True).splitlines()]
        assert len(rows) == 57
        if canonical is None: canonical = rows
        else: assert rows == canonical, 'proof format changed native schedule'
        for row in rows:
            stem = Path(str(prefix)+'-'+str(row['id']))
            n, clauses = formula(Path(str(stem)+'.cnf'))
            proof = directory/f"python-{mode}-{row['id']}.proof"
            handle = lib.tactix_init();assert handle
            execution = Execution(context, None, handle);view = execution.c
            schedule = NativeInprocessSchedule.from_execution(execution);schedule.initialized = True
            slot = InprocessSlot('BVE', group='Eliminate', preprocess=True,
                max_applies=1, permille=row['permille'], budget_aware=bool(row['aware']))
            params = capi.tactix_online_parameters(kind=capi.TACTIX_ONLINE_PARAMETERS_BVE)
            params.value.bve = capi.tactix_online_bve_parameters(**row['profile'])
            try:
                for clause in clauses:
                    for literal in (*clause, 0): assert lib.tactix_add_checked(handle, literal) == 0
                if fmt is not None: assert lib.tactix_proof(handle, str(proof).encode(), fmt) == 0
                view.tactix_status()  # same input/proof boundary as the native fixture
                assert view.tactix_set_guidance_publication(capi.TACTIX_GUIDANCE_NATIVE) == 0
                if row['stable']: execution.apply('SwitchMode')
                if row['formula'] == 3:
                    assert view.tactix_set_variable_retention(5, 1) == 0
                    execution.decide(-1)
                    assert execution.apply('Propagate') == 2
                    assert execution.apply('ConflictAnalysis') == 0
                    assert view.tactix_stat(capi.TACTIX_STAT_CONFLICTS) > 0
                if row['formula'] in (4,6):
                    for variable in (1,3,7,8): assert view.tactix_bump_variable(variable, n) == 0
                assert view.tactix_install_parameters(rules['BVE'], C.byref(params)) == 0
                work = capi.tactix_work_settings(fields=capi.TACTIX_WORK_BUDGET | capi.TACTIX_WORK_OFFSET |
                    capi.TACTIX_WORK_BOUNDED | capi.TACTIX_WORK_MIDROUND, budget=123, offset=5, bounded=0, midround=1)
                assert view.tactix_install_work_settings(C.byref(work)) == 0
                assert view.tactix_install_tick_attribution(capi.TACTIX_STAT_PROBE_TICKS) == 0
                meter = lambda v: v.tactix_stat(capi.TACTIX_STAT_BVE_RESOLUTIONS)
                def run_slot(v, entry, outer):
                    if not schedule.admit_slot(entry, schedule.observe(v)): return PhaseOutcome()
                    schedule.refresh_entry_limit(entry, schedule.observe(v), meter(v), outer)
                    return schedule.run_bve_slot(execution, entry, outer, params,
                        bound=0, maximum_bound=True, stopped=lambda: False)
                def rounds(v):
                    return schedule.run_rounds(v, 'Eliminate', [slot], run_slot, stopped=lambda: bool(row['halted']))
                progress = dict(propagate=0, work=0, deleted=0, refuted=0)
                propagate, flush = BveOrder.propagate, BveOrder.flush_units
                def observe_propagation(order):
                    before = view.tactix_stat(capi.TACTIX_STAT_PROPAGATIONS)
                    result = propagate(order)
                    progress['propagate'] += 1
                    progress['refuted'] += result == 20
                    progress['work'] += view.tactix_stat(capi.TACTIX_STAT_PROPAGATIONS)-before
                    return result
                def observe_flush(order, begin):
                    before = view.tactix_stat(capi.TACTIX_STAT_IRREDUNDANT_CLAUSES)
                    flush(order, begin)
                    progress['deleted'] += before-view.tactix_stat(capi.TACTIX_STAT_IRREDUNDANT_CLAUSES)
                with patch.object(BveOrder, 'propagate', observe_propagation), patch.object(BveOrder, 'flush_units', observe_flush):
                    phase = schedule.execute_pipeline(view, 'Eliminate', rounds, stopped=lambda: bool(row['halted']))
                if row['formula'] == 4:
                    assert all(progress[key] > 0 for key in ('propagate','work','deleted')), progress
                if row['formula'] == 5: assert progress['refuted'] == 1, progress
                if row['formula'] == 6:
                    assert progress['deleted'] > 0, progress
                    if row['permille']: assert phase == 20
                observed = [view.tactix_stat(getattr(capi, 'TACTIX_STAT_'+name)) for name in stat_names]
                assert phase == row['phase'] and observed == row['stats'], (row, phase, observed)
                clock = schedule.clocks['Eliminate']
                assert [getattr(clock, name) for name in ('next_conflicts','rounds','reductions_mark',
                    'marks_mark','fixed_mark','material_mark','blocked','blocked_episodes','anchor')] == row['clock'], row
                assert slot.limit == row['limit'] and slot.applies == row['applies'], row
                assert schedule.bve_rounds_run == (0 if row['halted'] else 1)
                assert view.tactix_read_work_settings(C.byref(work)) == 0
                assert work.budget == row['budget'] and work.offset == 5 and not work.bounded and work.midround
                answer = execution.default_solve()  # same explicit completion for both phase arms
                assert answer == row['answer'] == (10 if satisfiable(n, clauses) else 20)
                if answer == 10:
                    native_model = set(map(int, Path(str(stem)+'.model').read_text().split()))
                    assert all((v in native_model) != (-v in native_model) for v in range(1,n+1))
                    assert all(any(l in native_model for l in clause) for clause in clauses)
                    models += 1
                    model = {lib.tactix_value(handle, v) for v in range(1,n+1)}
                    assert all((v in model) != (-v in model) for v in range(1,n+1))
                    assert all(any(literal in model for literal in clause) for clause in clauses)
                    models += 1
                phases += 1
            finally:
                execution._active = False
                lib.tactix_release(handle)
            if fmt is not None:
                native = Path(str(stem)+'.proof')
                assert native.read_bytes() == proof.read_bytes(), row
                if row['answer'] == 20:
                    cnf = Path(str(stem)+'.cnf')
                    route = 'pbp' if mode == 'pbp' else ('dsr-cake_lpr' if certificate_route(proof) == 'drat' else 'dsr-trestle')
                    assert runner.check_proof(mode, str(cnf), str(proof), 30, route=route)[:3] == (True,'VERIFIED',runner.CHK_VERIFIED)
                    checked += 1
                    wrong = directory/f"wrong-{mode}-{row['id']}.cnf"
                    weakened = next(clauses[:i]+clauses[i+1:] for i in range(len(clauses))
                                    if satisfiable(n, clauses[:i]+clauses[i+1:]))
                    wrong.write_text(f'p cnf {n} {len(weakened)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in weakened))
                    rejected = runner.check_proof(mode,str(wrong),str(proof),30,route=route)
                    assert not rejected[0] and rejected[2] == runner.CHK_REJECTED, rejected
                    checked += 1
# Exercise the same entry in actual persistent query views, with Python search.
profile = next(row['profile'] for row in canonical if row['formula'] == 0 and row['aware'] and row['permille'] == 1000)
def policy(execution, restored):
    assert execution.c.tactix_set_guidance_publication(capi.TACTIX_GUIDANCE_NATIVE) == 0
    schedule = NativeInprocessSchedule.from_execution(execution)
    slot = InprocessSlot('BVE', group='Eliminate', preprocess=True, permille=1000, budget_aware=True)
    params = capi.tactix_online_parameters(kind=capi.TACTIX_ONLINE_PARAMETERS_BVE)
    params.value.bve = capi.tactix_online_bve_parameters(**profile)
    def run_slot(view, entry, outer):
        if not schedule.admit_slot(entry, schedule.observe(view)):
            return PhaseOutcome()
        schedule.refresh_entry_limit(entry, schedule.observe(view),
            view.tactix_stat(capi.TACTIX_STAT_BVE_RESOLUTIONS), outer)
        return schedule.run_bve_slot(execution, entry, outer, params, bound=0,
            maximum_bound=True, stopped=lambda: schedule.query_stopped(execution))
    def rounds(view):
        return schedule.run_rounds(view, 'Eliminate', [slot], run_slot, stopped=lambda: schedule.query_stopped(execution))
    if schedule.execute_pipeline(execution.c, 'Eliminate', rounds, stopped=lambda: NativeInprocessSchedule.query_stopped(execution)) == 20: return 20
    return ScriptedCDCL()(execution, restored)
original = [[1,2,3],[-1,2,4],[1,-2,5],[-1,-2,-3],[3,4],[-3,5],[6,7],[-6,8],[-7,8],[-8,4]]
with Session(library, strategy=policy) as session:
    for clause in original: session.add(clause)
    first = session.solve()
    assert first.status == 10 and all(any(l in first.model for l in clause) for clause in original)
    assert all((v in first.model) != (-v in first.model) for v in range(1,9))
    second = session.solve([-1,-2,-3]);assert second.status == 20
with tempfile.TemporaryDirectory(prefix='dosed-session-proof-') as scratch:
    stem = Path(scratch)/'query';second.export(stem)
    cnf, proof = Path(str(stem)+'.cnf'), Path(str(stem)+'.proof')
    assert not satisfiable(*formula(cnf))
    mode = 'pbp' if proof.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
    route = 'pbp' if mode == 'pbp' else ('dsr-cake_lpr' if certificate_route(proof) == 'drat' else 'dsr-trestle')
    assert runner.check_proof(mode,str(cnf),str(proof),30,route=route)[:3] == (True,'VERIFIED',runner.CHK_VERIFIED)
    checked += 1;models += 1
print(f'Ordinary dosed BVE: {phases} native/Python phases, {models} models, {checked} refutations/negatives; format trajectories and proof bytes agree')

# Cancel during atomic BVE, propagation, and phase exit; retry owns its proof.
def cancellation_case(original, *, propagate_refutation=False, at_exit=False, at_preparation=False):
    latch, armed, interrupted, propagation = [False], [not at_preparation], [], []
    def stopped_policy(execution, restored):
        assert execution.c.tactix_set_guidance_publication(capi.TACTIX_GUIDANCE_NATIVE) == 0
        schedule = NativeInprocessSchedule.from_execution(execution)
        if at_preparation and not latch[0]:
            execution.decide(1)
            assert execution.apply('Propagate') == 2
            assert execution.apply('ConflictAnalysis') == 0
            armed[0] = True
        slot = InprocessSlot('BVE', group='Eliminate', preprocess=True, permille=1000, budget_aware=True)
        params = capi.tactix_online_parameters(kind=capi.TACTIX_ONLINE_PARAMETERS_BVE)
        params.value.bve = capi.tactix_online_bve_parameters(**profile)
        params.value.bve.gate_mask = 0
        stopped = lambda: schedule.query_stopped(execution)
        def run_slot(view, entry, outer):
            if not schedule.admit_slot(entry, schedule.observe(view)): return PhaseOutcome()
            schedule.refresh_entry_limit(entry, schedule.observe(view),
                view.tactix_stat(capi.TACTIX_STAT_BVE_RESOLUTIONS), outer)
            return schedule.run_bve_slot(execution, entry, outer, params, bound=0,
                maximum_bound=True, stopped=stopped)
        def body(view):
            outcome = schedule.run_rounds(view, 'Eliminate', [slot], run_slot, stopped=stopped)
            if at_exit: latch[0] = True
            return outcome
        raw_propagate = BveOrder.propagate
        def observe(order):
            before = schedule._root_refuted(execution.c)
            result = raw_propagate(order)
            propagation.append((before, result, schedule._root_refuted(execution.c)))
            return result
        with patch.object(BveOrder, 'propagate', observe):
            result = schedule.execute_pipeline(execution.c, 'Eliminate', body, stopped=stopped)
        interrupted.append(schedule.last_phase_interrupted)
        if schedule.last_phase_interrupted:
            def forbidden(_): raise AssertionError('stopped pipeline entered its body')
            assert schedule.execute_pipeline(execution.c, 'Eliminate', forbidden, stopped=stopped) == 0
            assert schedule.last_phase_interrupted and not schedule.unusable and schedule.phase is None
        return result
    with Session(library, strategy=stopped_policy) as session:
        for clause in original: session.add(clause)
        def cancel(_):
            if armed[0]: latch[0] = True
        if not at_exit: session.set_learn(cancel, 2)
        session.set_terminate(lambda: latch[0])
        assert session.solve().status == 0 and latch[0] and interrupted == [True]
        if propagate_refutation: assert (False, 0, True) in propagation, propagation
        if at_preparation: assert not propagation
        session.set_terminate(None);session.set_learn(None, 0)
        retried = session.solve();assert retried.status == 20
    with tempfile.TemporaryDirectory(prefix='dosed-stop-retry-') as scratch:
        stem=Path(scratch)/'query';retried.export(stem)
        cnf,proof=Path(str(stem)+'.cnf'),Path(str(stem)+'.proof')
        assert not satisfiable(*formula(cnf))
        mode='pbp' if proof.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
        route='pbp' if mode=='pbp' else ('dsr-cake_lpr' if certificate_route(proof)=='drat' else 'dsr-trestle')
        assert runner.check_proof(mode,str(cnf),str(proof),30,route=route)[:3] == (True,'VERIFIED',runner.CHK_VERIFIED)
cancellation_case([[1,2],[1,-2],[-1,2],[-1,-2]])
cancellation_case([[1,2],[-1,2],[-2,3],[-2,-3]], propagate_refutation=True)
cancellation_case([[1,2],[1,-2],[-1,2],[-1,-2]], at_exit=True)
cancellation_case([[1,2],[1,-2],[-1,2],[-1,-2]], at_preparation=True)
print('Preparation/BVE/propagation/exit cancellation and stopped re-entry return UNKNOWN; all four retries have checked proofs')
