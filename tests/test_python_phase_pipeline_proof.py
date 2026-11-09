"""Check Python-authored phase execution and cleanup against original CNFs."""
from pathlib import Path
import importlib.util
import json
import ctypes as C
import subprocess
import sys
import tempfile
from dataclasses import asdict

library, fixture, root = map(Path, sys.argv[1:])
sys.path[:0] = [str(root/'python'), str(root/'tools/bench')]
from tactix import Session, capi
from python_policy_reference import NativeInprocessSchedule, InprocessSlot, PhaseOutcome, ScriptedDefault
from proof_oracle import certificate_route

spec = importlib.util.spec_from_file_location('phase_proof_runner', root/'tools/bench/run.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
config = json.loads(subprocess.check_output([str(fixture)], text=True).splitlines()[0])['config']
lib = capi.load_library(library)
rules = {lib.tactix_rule_name(i).decode(): i for i in range(lib.tactix_rule_count())}
normal = [[1, 2], [1, -2], [-1, 3], [-3, 4]]
wide = [[1, 2, 3], [-1, 2, 3], [1, -2, 3], [1, 2, -3]]
cases = [(group, mode, normal) for group in ('Probe', 'Eliminate')
         for mode in ('empty', 'bce', 'batch-exception', 'batch-cleanup-error', 'exception', 'invalid-outcome', 'false-unsat')]
cases += [('Probe', 'probe', [[1, 2], [1, -2], [-1, 2], [-1, -2]]),
          ('Probe', 'early', [[1], [-1]]), ('Eliminate', 'early', [[1], [-1]]),
          ('Probe', 'startup-sat', [[1]]),
          ('Probe', 'startup-substitute', [[1, 2], [1, -2], [-1, 2], [-1, -2]])]
cases += [('Eliminate', mode, normal) for mode in ('bve-maximum', 'bve-advance', 'fast-empty')]
cases += [('Probe', 'startup-decisions', normal),
          ('Probe', 'budgeted-unsat', [[1, 2], [1, -2], [-1, 2], [-1, -2]])]
cases += [(group, mode, wide) for group in ('Probe', 'Eliminate') for mode in ('empty', 'bce')]
round_formula = [[-1, 2], [-1, -2], [1, 3, 4], [5, 6, 7],
                 [-5, 6, 7], [5, -6, 7], [5, 6, -7]]
cases += [(group, mode, round_formula) for group in ('Probe', 'Eliminate')
          for mode in ('rounds-bce', 'rounds-forward', 'rounds-stop', 'rounds-limit')]
cases += [('Eliminate', 'rounds-fast', round_formula)]
cases += [('Probe', 'rounds-unsat', [[1, 2], [1, -2], [-1, 2], [-1, -2]])]
canonical = {}
proofs = models = 0
with tempfile.TemporaryDirectory(prefix='python-phase-proofs-') as raw:
    directory = Path(raw)
    for index, (group, mode, formula) in enumerate(cases):
        n = max(abs(lit) for clause in formula for lit in clause)
        cnf = directory/f'{index}.cnf'
        cnf.write_text(f'p cnf {n} {len(formula)}\n' +
                       ''.join(' '.join(map(str, clause))+' 0\n' for clause in formula))
        for label, fmt in [('none', None), ('dsr', capi.TACTIX_PROOF_DSR_ASCII),
                           ('pbp', capi.TACTIX_PROOF_PBP), ('auto', capi.TACTIX_PROOF_AUTO)]:
            handle = lib.tactix_init()
            assert handle
            proof = directory/f'{index}-{label}.proof'
            try:
                if fmt is not None: assert lib.tactix_proof(handle, str(proof).encode(), fmt) == 0
                for clause in formula:
                    for literal in [*clause, 0]: lib.tactix_add(handle, literal)
                assert lib.tactix_set_guidance_publication(handle, capi.TACTIX_GUIDANCE_NATIVE) == 0
                view = capi.SolverView(lib, handle, lambda: None)
                schedule = NativeInprocessSchedule(config)
                schedule.clocks[group].material_mark = 123
                actions = []
                supports = []
                raw_support = view.tactix_support
                def recorded_support(kind, argument=0):
                    supports.append((kind, argument))
                    return raw_support(kind, argument)
                view.tactix_support = recorded_support
                def body(view):
                    if mode == 'exception': raise LookupError('phase callback failed')
                    if mode == 'invalid-outcome': return PhaseOutcome(10)
                    if mode == 'false-unsat': return PhaseOutcome(20)
                    schedule.begin_round(view)
                    result = 0
                    slot = InprocessSlot('Substitute' if mode == 'budgeted-unsat' else 'BCE', group=group, budget_aware=True, preprocess=True, permille=1000)
                    def meter(view): return view.tactix_stat(capi.TACTIX_STAT_SUBSTITUTE_TICKS if slot.rule == 'Substitute' else capi.TACTIX_STAT_BCE_TICKS)
                    if mode in ('batch-exception', 'batch-cleanup-error'):
                        if mode == 'batch-cleanup-error':
                            raw_install = view.tactix_install_work_settings
                            calls = []
                            def install(pointer):
                                calls.append(1)
                                result = raw_install(pointer)
                                return -1 if len(calls) == 2 else result
                            view.tactix_install_work_settings = install
                        def fail(view): raise LookupError('batch callback failed')
                        schedule.refresh_entry_limit(slot, schedule.observe(view), meter(view), 0)
                        schedule.run_budgeted_batch(view, slot, meter, fail)
                    if mode in ('bce', 'probe', 'startup-substitute', 'budgeted-unsat', 'bve-maximum', 'bve-advance'):
                        name = ('BVE' if mode.startswith('bve-') else 'BCE' if mode == 'bce'
                                else 'Substitute' if mode in ('startup-substitute', 'budgeted-unsat') else 'Probe')
                        rule = rules[name]
                        if mode in ('bce', 'budgeted-unsat'):
                            assert schedule.admit_slot(slot, schedule.observe(view))
                            schedule.refresh_entry_limit(slot, schedule.observe(view), meter(view), 0)
                            result = schedule.run_budgeted_batch(view, slot, meter,
                                lambda view: view.tactix_apply_instance(rule, capi.TACTIX_INSTANCE_CANONICAL))
                        else:
                            result = view.tactix_apply_instance(rule, capi.TACTIX_INSTANCE_CANONICAL)
                        assert result in (0, 1, 20)
                        if mode in ('bce', 'bve-maximum', 'bve-advance'): assert result == 0
                        if mode == 'budgeted-unsat': assert result == 20
                        actions.append((rule, result))
                    return PhaseOutcome(20 if result == 20 else 0,
                                        bve_ran=mode.startswith('bve-'),
                                        maximum_bound=mode == 'bve-maximum')
                if mode.startswith('rounds-'):
                    if mode != 'rounds-unsat':
                        assert view.tactix_decide(1) == 0
                        assert view.tactix_apply(rules['Propagate']) == 2
                        assert view.tactix_apply(rules['ConflictAnalysis']) == 0
                        assert view.tactix_apply_instance(rules['Restart'], 0) == 0
                        assert view.tactix_apply(rules['Propagate']) == 0
                    slot = InprocessSlot('BVE' if mode == 'rounds-fast' else
                        'SubsumeForward' if mode == 'rounds-forward' else
                        'Probe' if mode == 'rounds-unsat' else 'BCE',
                        group=group, preprocess=True, budget_aware=True,
                        permille=1000 if mode == 'rounds-unsat' else 0,
                        forward_pair=mode == 'rounds-forward')
                    def meter(view):
                        fields = ('FORWARD_VISITS', 'STRENGTHEN_VISITS') if mode == 'rounds-forward' else (
                            'BCE_TICKS',) if slot.rule == 'BCE' else ('PROBE_TICKS',)
                        return sum(view.tactix_stat(getattr(capi, 'TACTIX_STAT_' + field)) for field in fields)
                    stop = [False]
                    calls = []
                    sentinel = InprocessSlot('BCE', group=group)
                    def slot_body(view, slot, outer):
                        calls.append((slot is sentinel, outer))
                        if slot is sentinel:
                            assert mode == 'rounds-limit'
                            return PhaseOutcome()
                        if mode == 'rounds-fast':
                            assert schedule.phase == ('Eliminate', True)
                            try: schedule.run_rounds(view, group, [], slot_body, initial_fast_elimination=False)
                            except ValueError: pass
                            else: raise AssertionError('conflicting phase mode was accepted')
                            applicable = (C.c_int * view.tactix_rule_count())()
                            count = view.tactix_rules_applicable(applicable, len(applicable))
                            result = (view.tactix_apply_instance(rules['BVE'], capi.TACTIX_INSTANCE_CANONICAL)
                                      if rules['BVE'] in applicable[:count] else 1)
                            actions.append((outer, rules['BVE'], result))
                            return PhaseOutcome(20 if result == 20 else 0, bve_ran=True, maximum_bound=True)
                        observation = schedule.observe(view)
                        if not schedule.admit_slot(slot, observation): return PhaseOutcome()
                        schedule.refresh_entry_limit(slot, observation, meter(view), outer)
                        if not schedule.batch_admissible(slot, meter(view)): return PhaseOutcome()
                        applicable = (C.c_int * view.tactix_rule_count())()
                        count = view.tactix_rules_applicable(applicable, len(applicable))
                        if rules[slot.rule] not in applicable[:count]: return PhaseOutcome()
                        result = schedule.run_budgeted_batch(view, slot, meter,
                            lambda view: view.tactix_apply_instance(rules[slot.rule], capi.TACTIX_INSTANCE_CANONICAL))
                        actions.append((outer, rules[slot.rule], result))
                        if result == 0:
                            result = view.tactix_apply(rules['Propagate'])
                            actions.append((outer, rules['Propagate'], result))
                        stop[0] = mode == 'rounds-stop'
                        return PhaseOutcome(20 if result == 20 else 0,
                                            eliminate_limit_reached=mode == 'rounds-limit')
                    selected = ([sentinel, slot] if mode == 'rounds-fast' else
                                [slot, sentinel] if mode in ('rounds-stop', 'rounds-limit') else [slot])
                    result = schedule.execute_pipeline(view, group,
                        lambda view: schedule.run_rounds(view, group, selected, slot_body, stopped=lambda: stop[0]),
                        initial_fast_elimination=mode == 'rounds-fast')
                    assert schedule.phase is None
                    if mode == 'rounds-unsat':
                        assert result == 20 and any(code == 20 for _, _, code in actions), (result, actions, calls)
                    if mode == 'rounds-fast':
                        assert len(calls) > 1 and all(not sentinel for sentinel, _ in calls), calls
                    if mode == 'rounds-stop': assert calls == [(False, 0)], calls
                    if mode == 'rounds-limit': assert calls == [(False, 0), (True, 0)], calls
                elif mode in ('exception', 'batch-exception', 'batch-cleanup-error', 'invalid-outcome', 'false-unsat'):
                    expected = LookupError if mode in ('exception', 'batch-exception', 'batch-cleanup-error') else ValueError
                    try: schedule.execute_pipeline(view, group, body)
                    except expected as error:
                        if mode == 'batch-cleanup-error':
                            assert len(error.phase_cleanup_errors) == 1
                            assert str(error.phase_cleanup_errors[0]) == 'cannot clear batch work settings'
                            del view.tactix_install_work_settings
                    else: raise AssertionError('callback error was suppressed')
                    if mode in ('batch-exception', 'batch-cleanup-error'):
                        work = capi.tactix_work_settings()
                        assert view.tactix_read_work_settings(C.byref(work)) == 0
                        assert (work.budget, work.offset, work.bounded, work.midround) == (0, 0, 0, 0)
                    assert schedule.unusable
                    try: schedule.reset([])
                    except RuntimeError: pass
                    else: raise AssertionError('reset revived failed schedule')
                    # The bracket closed, but failed bookkeeping must not be reused.
                    schedule = NativeInprocessSchedule(config)
                    result = schedule.execute_pipeline(view, group, lambda view: PhaseOutcome())
                elif mode.startswith('startup-'):
                    def skipped(view): raise AssertionError('terminal startup ran slots')
                    hook = (lambda view: view.tactix_status()) if mode == 'startup-sat' else (
                        lambda view: body(view).status)
                    if mode == 'startup-decisions':
                        def hook(view):
                            for literal in (1, 2):
                                assert view.tactix_decide(literal) == 0
                                assert view.tactix_apply(rules['Propagate']) in (0, 1)
                            assert view.tactix_status() == 10
                            return 10
                    result = schedule.execute_pipeline(view, group, skipped, run_startup=hook)
                    assert result == (10 if mode in ('startup-sat', 'startup-decisions') else 20)
                    assert schedule.clocks[group].rounds == 0
                else:
                    result = schedule.execute_pipeline(view, group, body,
                                                       initial_fast_elimination=mode == 'fast-empty')
                if mode in ('bce', 'batch-exception', 'batch-cleanup-error', 'budgeted-unsat'):
                    work = capi.tactix_work_settings()
                    assert view.tactix_read_work_settings(C.byref(work)) == 0
                    assert (work.budget, work.offset, work.bounded, work.midround) == (0, 0, 0, 0)
                reschedules = sum(kind == capi.TACTIX_SUPPORT_RESCHEDULE_BVE for kind, _ in supports)
                assert reschedules == int(mode in ('bve-advance', 'fast-empty', 'rounds-fast'))
                if mode == 'bve-maximum':
                    assert schedule.clocks[group].material_mark == schedule.observe(view)['SUBSUME_MATERIAL']
                if mode == 'bve-advance':
                    assert schedule.clocks[group].marks_mark == schedule.clocks[group].fixed_mark == 0
                    assert schedule.clocks[group].material_mark == 123
                if mode == 'fast-empty':
                    assert schedule.clocks[group].rounds == 0
                    assert schedule.clocks[group].material_mark == 123
                policy = ScriptedDefault()
                if result not in (10, 20): result = policy.run(view, rules)
                assert result == (20 if mode in ('probe', 'early', 'startup-substitute', 'budgeted-unsat', 'rounds-unsat') else 10)
                model = tuple(v if lib.tactix_value(handle, v) > 0 else -v
                              for v in range(1, n+1)) if result == 10 else ()
                if model:
                    assert all(any(lit in model for lit in clause) for clause in formula)
                    models += 1
                trajectory = (result, model, actions, supports, policy.trace, schedule.observe(view),
                              {name: asdict(clock) for name, clock in schedule.clocks.items()})
                assert trajectory == canonical.setdefault(index, trajectory), (index, label)
            finally:
                lib.tactix_release(handle)
            if result == 20 and fmt is not None:
                kind = 'pbp' if proof.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
                route = 'pbp' if kind == 'pbp' else (
                    'dsr-cake_lpr' if certificate_route(proof) == 'drat' else 'dsr-trestle')
                checked = runner.check_proof(kind, str(cnf), str(proof), 30, route=route)
                assert checked[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), checked
                proofs += 1
    for group in ('Probe', 'Eliminate'):
        handle = lib.tactix_init()
        try:
            for literal in [1, 2, 0]: lib.tactix_add(handle, literal)
            view = capi.SolverView(lib, handle, lambda: None)
            schedule = NativeInprocessSchedule(config)
            original = KeyboardInterrupt('callback interrupted with an open context')
            def interrupted(view):
                assert view.tactix_support(capi.TACTIX_SUPPORT_ENTER_CONTEXT,
                                           capi.TACTIX_CONTEXT_SPECULATION) == 0
                raise original
            try: schedule.execute_pipeline(view, group, interrupted)
            except KeyboardInterrupt as error:
                assert error is original and len(error.phase_cleanup_errors) == 1
                assert schedule.unusable
            else: raise AssertionError('callback exception was replaced or suppressed')
            try: schedule.execute_pipeline(view, group, lambda view: PhaseOutcome())
            except RuntimeError: pass
            else: raise AssertionError('failed-cleanup execution was reused')
        finally:
            lib.tactix_release(handle)
        handle = lib.tactix_init()
        try:
            for literal in [1, 2, 0]: lib.tactix_add(handle, literal)
            view = capi.SolverView(lib, handle, lambda: None)
            schedule = NativeInprocessSchedule(config)
            def unclosed_context(view):
                assert view.tactix_support(capi.TACTIX_SUPPORT_ENTER_CONTEXT,
                                           capi.TACTIX_CONTEXT_SPECULATION) == 0
                return PhaseOutcome()
            try: raise LookupError('unrelated caller exception')
            except LookupError as unrelated:
                try: schedule.execute_pipeline(view, group, unclosed_context)
                except RuntimeError:
                    assert schedule.unusable and not hasattr(unrelated, 'phase_cleanup_errors')
                else: raise AssertionError('cleanup failure was swallowed by caller exception')
        finally:
            lib.tactix_release(handle)
print(f'Python public phase pipelines: {len(cases)*4} runs, {models} models, {proofs} proofs; format trajectories match')

session_models = session_proofs = 0
with tempfile.TemporaryDirectory(prefix='python-phase-sessions-') as raw:
    for group in ('Probe', 'Eliminate'):
        calls = []
        def strategy(execution, restored):
            calls.append(restored)
            schedule = NativeInprocessSchedule(config)
            result = schedule.execute_pipeline(execution.c, group, lambda view: PhaseOutcome())
            return result if result in (10, 20) else ScriptedDefault()(execution, restored)
        with Session(library, strategy=strategy) as session:
            for clause in normal: session.add(clause)
            for assumptions in ((), (2,), (-2,)):
                result = session.solve(assumptions)
                assert result.status == 10 and all(lit in result.model for lit in assumptions)
                assert all(any(lit in result.model for lit in clause) for clause in normal)
                session_models += 1
        assert calls
        def refute(execution, restored):
            def body(view):
                code = view.tactix_apply_instance(rules['Substitute'], capi.TACTIX_INSTANCE_CANONICAL)
                assert code == 20
                return PhaseOutcome(20)
            return NativeInprocessSchedule(config).execute_pipeline(execution.c, group, body)
        with Session(library, strategy=refute) as session:
            for clause in [[1, 2], [1, -2], [-1, 2], [-1, -2]]: session.add(clause)
            result = session.solve()
            assert result.status == 20
            prefix = Path(raw)/group
            result.export(prefix)
        proof = prefix.with_suffix('.proof')
        kind = 'pbp' if proof.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
        route = 'pbp' if kind == 'pbp' else (
            'dsr-cake_lpr' if certificate_route(proof) == 'drat' else 'dsr-trestle')
        checked = runner.check_proof(kind, str(prefix.with_suffix('.cnf')), str(proof), 30, route=route)
        assert checked[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), checked
        session_proofs += 1
        original = KeyboardInterrupt('phase strategy callback interrupted')
        def interrupted_strategy(execution, restored):
            def body(view):
                assert view.tactix_support(capi.TACTIX_SUPPORT_ENTER_CONTEXT,
                                           capi.TACTIX_CONTEXT_SPECULATION) == 0
                raise original
            return NativeInprocessSchedule(config).execute_pipeline(execution.c, group, body)
        with Session(library, strategy=interrupted_strategy) as session:
            for clause in wide: session.add(clause)
            try: session.solve()
            except RuntimeError as wrapper:
                assert wrapper.__cause__ is original and len(original.phase_cleanup_errors) == 1
            else: raise AssertionError('ctypes boundary lost the phase callback error')
            session.set_strategy(None)
            result = session.solve()
            assert result.status == 10 and all(any(lit in result.model for lit in clause) for clause in wide)
            session_models += 1
print(f'Python phase Session boundary: {session_models} models, {session_proofs} proofs, callback identity and recovery pass')
