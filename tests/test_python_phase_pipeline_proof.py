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
          for mode in ('rounds-bce', 'rounds-forward')]
cases += [('Eliminate', mode, round_formula) for mode in
          ('rounds-fast', 'rounds-fast-startup', 'rounds-stop', 'rounds-limit')]
cases += [('Eliminate', 'fast-early', [[1], [-1]]),
          ('Eliminate', 'fast-exception', normal), ('Eliminate', 'fast-cleanup-error', normal)]
substitute_formula = [[-1, 2], [1, -2], [2, 3], [-2, 4]]
cases += [(group, mode, substitute_formula) for group in ('Probe', 'Eliminate')
          for mode in ('typed-substitute-complete', 'typed-substitute-ceiling', 'typed-substitute-budget')]
cases += [('Probe', 'rounds-unsat', [[1, 2], [1, -2], [-1, 2], [-1, -2]])]
cases += [(group, mode, normal) for group in ('Probe', 'Eliminate') for mode in
          ('typed-error-configure', 'typed-error-advance', 'typed-error-status',
           'typed-interrupted', 'typed-budget-decline')]
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
                    fast = mode in ('rounds-fast', 'rounds-fast-startup')
                    bve = fast or mode in ('rounds-stop', 'rounds-limit')
                    if mode not in ('rounds-unsat', 'rounds-fast-startup'):
                        assert view.tactix_decide(1) == 0
                        assert view.tactix_apply(rules['Propagate']) == 2
                        assert view.tactix_apply(rules['ConflictAnalysis']) == 0
                        assert view.tactix_apply_instance(rules['Restart'], 0) == 0
                        assert view.tactix_apply(rules['Propagate']) == 0
                    slot = InprocessSlot('BVE' if bve else
                        'SubsumeForward' if mode == 'rounds-forward' else
                        'Probe' if mode == 'rounds-unsat' else 'BCE',
                        group=group, preprocess=not fast, defer_preprocess=fast,
                        budget_aware=True, permille=1000 if mode == 'rounds-unsat' else 0,
                        forward_pair=mode == 'rounds-forward')
                    def meter(view):
                        fields = ('FORWARD_VISITS', 'STRENGTHEN_VISITS') if mode == 'rounds-forward' else (
                            'BCE_TICKS',) if slot.rule == 'BCE' else ('PROBE_TICKS',)
                        return sum(view.tactix_stat(getattr(capi, 'TACTIX_STAT_' + field)) for field in fields)
                    stop, calls, changes = [False], [], []
                    sentinel = InprocessSlot('BCE', group=group)
                    before = schedule.observe(view)
                    try: schedule.run_rounds(view, group, [], lambda *args: PhaseOutcome(), initial_fast_elimination=True)
                    except RuntimeError: pass
                    else: raise AssertionError('rounds ran outside a phase')
                    assert schedule.phase is None and schedule.observe(view) == before
                    def slot_body(view, entry, outer):
                        calls.append((entry is sentinel, outer))
                        if entry is sentinel:
                            assert mode == 'rounds-limit'
                            return PhaseOutcome()
                        observation = schedule.observe(view)
                        if fast:
                            assert schedule.phase == ('Eliminate', True)
                            try: schedule.reset([])
                            except RuntimeError: pass
                            else: raise AssertionError('active phase reset was accepted')
                            try: schedule.admit_slot(slot, observation, initial_fast_elimination=False)
                            except ValueError: pass
                            else: raise AssertionError('conflicting phase mode was accepted')
                        if not schedule.admit_slot(slot, observation): return PhaseOutcome()
                        # Exercise inherited fast effort scaling; canonical BVE
                        # below is unbudgeted, not a native candidate-dose mirror.
                        schedule.refresh_entry_limit(slot, observation, 0 if bve else meter(view), outer)
                        if fast:
                            reference = (config['preprocess_initial_ticks'] if not observation['CONFLICTS'] else
                                max(observation['SEARCH_TICKS'] - schedule.clocks[group].anchor,
                                    config['minimum_effort_ticks']))
                            assert slot.limit == reference * config['initial_fast_elimination_bve_permille'] // 1000
                        if not bve and not schedule.batch_admissible(slot, meter(view)): return PhaseOutcome()
                        applicable = (C.c_int * view.tactix_rule_count())()
                        count = view.tactix_rules_applicable(applicable, len(applicable))
                        if rules[slot.rule] not in applicable[:count]: return PhaseOutcome()
                        active = view.tactix_stat(capi.TACTIX_STAT_ACTIVE_VARIABLES)
                        result = (view.tactix_apply_instance(rules[slot.rule], capi.TACTIX_INSTANCE_CANONICAL) if bve else
                            schedule.run_budgeted_batch(view, slot, meter,
                                lambda view: view.tactix_apply_instance(rules[slot.rule], capi.TACTIX_INSTANCE_CANONICAL)))
                        assert result >= 0
                        actions.append((outer, rules[slot.rule], result))
                        if result == 0:
                            result = view.tactix_apply(rules['Propagate'])
                            actions.append((outer, rules['Propagate'], result))
                        changes.append((active, view.tactix_stat(capi.TACTIX_STAT_ACTIVE_VARIABLES)))
                        stop[0] = mode == 'rounds-stop'
                        return PhaseOutcome(20 if result == 20 else 0, bve_ran=bve, maximum_bound=bve,
                                            eliminate_limit_reached=mode == 'rounds-limit')
                    selected = ([sentinel, slot] if fast else
                                [slot, sentinel] if mode in ('rounds-stop', 'rounds-limit') else [slot])
                    result = schedule.execute_pipeline(view, group,
                        lambda view: schedule.run_rounds(view, group, selected, slot_body, stopped=lambda: stop[0]),
                        initial_fast_elimination=fast)
                    assert schedule.phase is None
                    if mode == 'rounds-unsat':
                        assert result == 20 and any(rule == rules['Propagate'] and code == 20
                            for _, rule, code in actions), (result, actions, calls)
                    if fast: assert len(calls) > 1 and all(not sentinel for sentinel, _ in calls), calls
                    if bve: assert any(before != after for before, after in changes), changes
                    if mode == 'rounds-stop': assert calls == [(False, 0)], calls
                    if mode == 'rounds-limit': assert calls == [(False, 0), (True, 0)], calls
                    if mode in ('rounds-stop', 'rounds-limit'):
                        assert sum(kind == capi.TACTIX_SUPPORT_BEGIN_FORWARD_ROUND for kind, _ in supports) == 1
                elif mode.startswith('typed-substitute-'):
                    slot = InprocessSlot('Substitute', group=group, preprocess=True, budget_aware=True)
                    parameters = capi.tactix_online_parameters(kind=capi.TACTIX_ONLINE_PARAMETERS_SUBSTITUTE)
                    parameters.value.substitute.max_rounds = 2
                    parameters.value.substitute.demote_substituted = 1
                    parameters.value.substitute.round_effort_ceiling = mode != 'typed-substitute-complete'
                    packets = []
                    def typed(view, entry, outer):
                        assert schedule.admit_slot(slot, schedule.observe(view))
                        def meter(view): return view.tactix_stat(capi.TACTIX_STAT_SUBSTITUTE_TICKS)
                        schedule.refresh_entry_limit(slot, schedule.observe(view), meter(view), outer)
                        if mode == 'typed-substitute-budget': slot.limit = meter(view) + 1
                        code, packet = schedule.run_typed_batch(view, slot, meter, parameters)
                        assert packet.base.applications == 1 and packet.base.rule_id == rules['Substitute']
                        assert packet.base.selection_complete == int(mode == 'typed-substitute-complete')
                        packets.append(packet)
                        actions.append((outer, rules['Substitute'], code))
                        if code == 0: code = view.tactix_apply(rules['Propagate'])
                        return PhaseOutcome(20 if code == 20 else 0)
                    result = schedule.execute_pipeline(view, group,
                        lambda view: schedule.run_rounds(view, group, [slot], typed))
                    assert packets and view.tactix_stat(capi.TACTIX_STAT_SUBSTITUTE_VARS) > 0
                    assert slot.handed == int(mode == 'typed-substitute-budget') and slot.spent > slot.handed
                elif mode.startswith('typed-'):
                    raw_library = view._library
                    class ObservedLibrary:
                        def __init__(self): self.created, self.released = [], []
                        def __getattr__(self, name): return getattr(raw_library, name)
                        def tactix_online_executor_create(self, handle):
                            executor = raw_library.tactix_online_executor_create(handle)
                            self.created.append(executor)
                            return executor
                        def tactix_online_executor_release(self, executor):
                            self.released.append(executor)
                            raw_library.tactix_online_executor_release(executor)
                        def tactix_online_executor_advance(self, *args):
                            if mode == 'typed-error-advance': return -1
                            if mode in ('typed-error-status', 'typed-interrupted'):
                                packet = C.cast(args[-1], C.POINTER(capi.tactix_online_execution_result)).contents
                                packet.base.status = (capi.TACTIX_ONLINE_RESOURCE_EXHAUSTED if mode == 'typed-error-status'
                                                      else capi.TACTIX_ONLINE_INTERRUPTED)
                                return 0
                            return raw_library.tactix_online_executor_advance(*args)
                    observed_library = ObservedLibrary()
                    view._library = observed_library
                    slot = InprocessSlot('BCE' if mode == 'typed-budget-decline' else 'Substitute', group=group)
                    parameters = capi.tactix_online_parameters(kind=(capi.TACTIX_ONLINE_PARAMETERS_FACTOR
                        if mode == 'typed-error-configure' else capi.TACTIX_ONLINE_PARAMETERS_SUBSTITUTE))
                    parameters.value.substitute.max_rounds = 2
                    def injected(view):
                        slot.limit = 1
                        code, packet = schedule.run_typed_batch(view, slot,
                            lambda view: 2 if mode == 'typed-budget-decline' else 0, parameters)
                        if mode == 'typed-budget-decline':
                            assert (code, packet) == (1, None) and slot.decline == 'budget'
                        elif mode == 'typed-interrupted':
                            assert code == 1 and packet.base.status == capi.TACTIX_ONLINE_INTERRUPTED
                        return PhaseOutcome()
                    try:
                        if mode.startswith('typed-error-'):
                            try: schedule.execute_pipeline(view, group, injected)
                            except (ValueError if mode == 'typed-error-configure' else RuntimeError) as error:
                                assert schedule.unusable and schedule.phase is None
                                if mode == 'typed-error-status':
                                    assert error.execution_result.base.status == capi.TACTIX_ONLINE_RESOURCE_EXHAUSTED
                            else: raise AssertionError('typed failure was suppressed')
                        else:
                            assert schedule.execute_pipeline(view, group, injected) == 0
                            assert not schedule.unusable and schedule.phase is None
                    finally:
                        view._library = raw_library
                    assert observed_library.created == observed_library.released
                    assert len(observed_library.released) == int(mode != 'typed-budget-decline')
                    work = capi.tactix_work_settings()
                    assert view.tactix_read_work_settings(C.byref(work)) == 0
                    assert (work.budget, work.offset, work.bounded, work.midround) == (0, 0, 0, 0)
                    result = 0
                elif mode in ('fast-early', 'fast-exception', 'fast-cleanup-error'):
                    def fast_body(view):
                        if mode == 'fast-exception': raise LookupError('fast phase callback failed')
                        return PhaseOutcome()
                    if mode == 'fast-cleanup-error':
                        raw = view.tactix_support
                        def fail_reschedule(kind, argument=0):
                            result = raw(kind, argument)
                            return -1 if kind == capi.TACTIX_SUPPORT_RESCHEDULE_BVE else result
                        view.tactix_support = fail_reschedule
                    if mode == 'fast-early':
                        result = schedule.execute_pipeline(view, group, fast_body, initial_fast_elimination=True)
                        assert result == 20
                    else:
                        try: schedule.execute_pipeline(view, group, fast_body, initial_fast_elimination=True)
                        except (LookupError if mode == 'fast-exception' else RuntimeError): pass
                        else: raise AssertionError('fast phase error was suppressed')
                        assert schedule.unusable
                        if mode == 'fast-cleanup-error': view.tactix_support = raw
                        result = 0
                    assert schedule.phase is None
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
                    assert schedule.unusable and schedule.phase is None
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
                if mode in ('bce', 'batch-exception', 'batch-cleanup-error', 'budgeted-unsat') or mode.startswith('typed-substitute-'):
                    work = capi.tactix_work_settings()
                    assert view.tactix_read_work_settings(C.byref(work)) == 0
                    assert (work.budget, work.offset, work.bounded, work.midround) == (0, 0, 0, 0)
                reschedules = sum(kind == capi.TACTIX_SUPPORT_RESCHEDULE_BVE for kind, _ in supports)
                assert reschedules == int(mode in ('bve-advance', 'fast-empty', 'rounds-fast', 'rounds-fast-startup', 'fast-early', 'fast-exception', 'fast-cleanup-error'))
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
                assert result == (20 if mode in ('probe', 'early', 'startup-substitute', 'budgeted-unsat', 'rounds-unsat', 'fast-early') else 10)
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
            slot = InprocessSlot('Substitute', group=group, preprocess=True, budget_aware=True)
            parameters = capi.tactix_online_parameters(kind=capi.TACTIX_ONLINE_PARAMETERS_SUBSTITUTE)
            parameters.value.substitute.max_rounds = 2
            parameters.value.substitute.demote_substituted = 1
            def typed(view, entry, outer):
                assert schedule.admit_slot(slot, schedule.observe(view))
                def meter(view): return view.tactix_stat(capi.TACTIX_STAT_SUBSTITUTE_TICKS)
                schedule.refresh_entry_limit(slot, schedule.observe(view), meter(view), outer)
                code, packet = schedule.run_typed_batch(view, slot, meter, parameters)
                assert packet and packet.base.applications == 1
                if code == 0: code = view.tactix_apply(rules['Propagate'])
                return PhaseOutcome(20 if code == 20 else 0)
            result = schedule.execute_pipeline(execution.c, group,
                lambda view: schedule.run_rounds(view, group, [slot], typed))
            return result if result in (10, 20) else ScriptedDefault()(execution, restored)
        with Session(library, strategy=strategy) as session:
            for clause in normal: session.add(clause)
            for index, assumptions in enumerate(((), (2,), (-2,))):
                if index == 1: session.add([5, 6])
                result = session.solve(assumptions)
                assert result.status == 10 and all(lit in result.model for lit in assumptions)
                assert all(any(lit in result.model for lit in clause) for clause in
                           normal + ([[5, 6]] if index else []))
                session_models += 1
        assert calls
        def refute(execution, restored):
            schedule = NativeInprocessSchedule(config)
            slot = InprocessSlot('Substitute', group=group, preprocess=True, budget_aware=True)
            parameters = capi.tactix_online_parameters(kind=capi.TACTIX_ONLINE_PARAMETERS_SUBSTITUTE)
            parameters.value.substitute.max_rounds = 2
            def body(view, entry, outer):
                schedule.refresh_entry_limit(slot, schedule.observe(view),
                    view.tactix_stat(capi.TACTIX_STAT_SUBSTITUTE_TICKS), outer)
                code, packet = schedule.run_typed_batch(view, slot,
                    lambda view: view.tactix_stat(capi.TACTIX_STAT_SUBSTITUTE_TICKS), parameters)
                assert code == 20 and packet.base.status == capi.TACTIX_ONLINE_UNSAT
                assert packet.base.applications == 1
                return PhaseOutcome(20)
            return schedule.execute_pipeline(execution.c, group,
                lambda view: schedule.run_rounds(view, group, [slot], body))
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
