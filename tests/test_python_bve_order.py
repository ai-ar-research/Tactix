"""Native/C/Python mutable BVE ordering, lifetimes and original-input oracles."""
from pathlib import Path
import ctypes as C
import importlib.util
import subprocess
import sys
import tempfile
import types

fixture, root = map(Path, sys.argv[1:3])
library = Path(sys.argv[3]) if len(sys.argv) == 4 else None
UNSAT_CASES = (1, 5)
sys.path[:0] = [str(root/'python'), str(root/'tools/bench'), str(root/'tests')]
from test_native_query_proof import formula
from test_native_query_append_proof import satisfiable
from proof_oracle import certificate_route
spec = importlib.util.spec_from_file_location('bve_order_oracle', root/'tools/bench/run.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
for tool in (runner.VERIPB, runner.CAKE_PB_CNF, runner.DSR_TRIM,
             runner.DSR_COMPRESS, runner.SR_CHECK, runner.CAKE_LPR):
    if not Path(tool).is_file():
        print('BVE ordering: missing checker', tool)
        sys.exit(77)

if library:
    from tactix import BveOrder, Execution, Session, ScriptedCDCL, capi
    lib = capi.load_library(library)
    rules = {lib.tactix_rule_name(i).decode(): i for i in range(lib.tactix_rule_count())}
    context = types.SimpleNamespace(dll=lib, rules=rules, _callback_depth=0)

    def profile(view, occurrences):
        params = capi.tactix_online_parameters(kind=capi.TACTIX_ONLINE_PARAMETERS_BVE)
        params.value.bve = capi.tactix_online_bve_parameters(growth_bound=16,
            occurrence_limit=occurrences, clause_size_limit=2**32-1,
            xor_arity_limit=5, resolvent_size_limit=2**32-1, definition_cores=2)
        assert view.c.tactix_install_parameters(rules['BVE'], C.byref(params)) == 0

    def applicable(view, rule):
        ids = (C.c_int * len(rules))()
        count = view.c.tactix_rules_applicable(ids, len(ids))
        return rules[rule] in ids[:count]

    def ordered_phase(view, variables, index=0, instances=None):
        rows = []
        with BveOrder(view, range(1, variables+1) if instances is None else instances) as order:
            if index in (3, 5):
                order.flush_units(0)
                rows.append(f'case={index} first_flush=0\n')
            nonzero_relevance = False
            while (packet := order.pop()) is not None:
                instance, begin = packet
                observed = order.candidate(instance)
                nonzero_relevance |= observed.relevance > 0
                skip = (capi.TACTIX_BVE_CANDIDATE_ASSIGNED | capi.TACTIX_BVE_CANDIDATE_RETIRED |
                        capi.TACTIX_BVE_CANDIDATE_RETAINED | capi.TACTIX_BVE_CANDIDATE_AFFINE |
                        capi.TACTIX_BVE_CANDIDATE_QUERY)
                result = propagated = 1
                if not observed.flags & skip and observed.flags & capi.TACTIX_BVE_CANDIDATE_RETRY:
                    result = order.apply('BVE', instance)
                    if result != 20 and applicable(view, 'Propagate'):
                        propagated = order.apply('Propagate')
                    if result != 20 and propagated != 20: order.flush_units(begin)
                rows.append(f'case={index} pop={instance} flags={observed.flags} '
                            f'pos={observed.positive_occurrences} neg={observed.negative_occurrences} '
                            f'begin={begin} result={result} propagated={propagated}\n')
                if 20 in (result, propagated): break
            if index == 4: assert nonzero_relevance
        return rows

    def python_fixture(prefix, mode, original):
        trace = []
        for index in range(6):
            n, clauses = formula(Path(f'{original}-{index}.cnf'))
            stem = Path(f'{prefix}-{index}')
            Path(str(stem)+'.cnf').write_bytes(Path(f'{original}-{index}.cnf').read_bytes())
            handle = lib.tactix_init()
            view = Execution(context, None, handle)
            try:
                for clause in clauses:
                    for literal in (*clause, 0): assert lib.tactix_add_checked(handle, literal) == 0
                if mode != 'none':
                    fmt = capi.TACTIX_PROOF_DSR_ASCII if mode == 'dsr' else capi.TACTIX_PROOF_PBP
                    assert lib.tactix_proof(handle, str(stem).encode()+b'.proof', fmt) == 0
                profile(view, len(clauses))
                assert view.c.tactix_set_guidance_publication(capi.TACTIX_GUIDANCE_NATIVE) == 0
                assert view.c.tactix_support(capi.TACTIX_SUPPORT_ROOT_PREPARATION, 0) == 0
                if index == 4:
                    assert view.c.tactix_apply(rules['SwitchMode']) == 0
                    assert view.c.tactix_bump_variable(1, 2) == 0
                    assert view.c.tactix_stat(capi.TACTIX_STAT_STABLE) == 1
                assert view.c.tactix_support(capi.TACTIX_SUPPORT_ENTER_INPROCESS_PHASE, capi.TACTIX_PHASE_ELIMINATE) == 0
                assert view.c.tactix_support(capi.TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION, 0) == 0
                trace.extend(ordered_phase(view, n, index))
                assert view.c.tactix_support(capi.TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0) in (0, 20)
                answer = view.default_solve()  # Finish this component fixture; not full-policy correspondence.
                assert answer == (20 if index in UNSAT_CASES else 10)
                eliminated = view.c.tactix_stat(capi.TACTIX_STAT_BVE_ELIMINATED)
                resolutions = view.c.tactix_stat(capi.TACTIX_STAT_BVE_RESOLUTIONS)
                ticks = view.c.tactix_stat(capi.TACTIX_STAT_TICKS)
                trace.append(f'case={index} answer={answer} eliminated={eliminated} resolutions={resolutions} ticks={ticks}\n')
                if answer == 10:
                    values = [lib.tactix_value(handle, v) for v in range(1, n+1)]
                    Path(str(stem)+'.model').write_text(' '.join(map(str, values))+' 0\n')
            finally:
                view._active = False
                lib.tactix_release(handle)
        return ''.join(trace)

def verify(prefix, mode):
    checked = models = 0
    for index in range(6):
        cnf, proof = Path(f'{prefix}-{index}.cnf'), Path(f'{prefix}-{index}.proof')
        n, clauses = formula(cnf)
        assert satisfiable(n, clauses) == (index not in UNSAT_CASES)
        if index not in UNSAT_CASES:
            model = set(map(int, Path(f'{prefix}-{index}.model').read_text().split()))
            assert all((v in model) != (-v in model) for v in range(1, n+1))
            assert all(any(l in model for l in clause) for clause in clauses)
            models += 1
        if mode == 'none':
            assert not proof.exists()
            continue
        if index not in UNSAT_CASES: continue
        route = 'pbp' if mode == 'pbp' else ('dsr-cake_lpr' if certificate_route(proof) == 'drat' else 'dsr-trestle')
        result = runner.check_proof(mode, str(cnf), str(proof), 30, route=route)
        assert result[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), result
        wrong = cnf.with_suffix('.wrong.cnf')
        lines = cnf.read_text().splitlines()
        header = lines[0].split();header[3] = str(int(header[3])-1)
        wrong.write_text(' '.join(header)+'\n'+'\n'.join(lines[2:])+'\n')
        assert satisfiable(n, formula(wrong)[1])
        result = runner.check_proof(mode, str(wrong), str(proof), 30, route=route)
        assert not result[0] and result[2] == runner.CHK_REJECTED, result
        checked += 2
    return checked, models

checked = models = 0
with tempfile.TemporaryDirectory(prefix='tactix-bve-order-') as tmp:
    directory = Path(tmp)
    golden, proofs = None, {}
    for mode in ('none', 'dsr', 'pbp'):
        native = directory/f'{mode}-native'
        for driver in ('native', 'public', *(['python'] if library else [])):
            prefix = directory/f'{mode}-{driver}'
            if driver == 'python': trace = python_fixture(prefix, mode, native)
            else: trace = subprocess.check_output([fixture, prefix, mode, driver], text=True, timeout=60)
            if golden is None: golden = trace
            assert trace == golden, (mode, driver, trace, golden)
            counts = verify(prefix, mode);checked += counts[0];models += counts[1]
            if mode != 'none':
                for index in range(6):
                    data = Path(f'{prefix}-{index}.proof').read_bytes()
                    key = mode, index
                    assert data == proofs.setdefault(key, data), (mode, driver, index)

if library:
    # Real persistent Python strategies select BVE/Propagate/flush, then
    # Python-authored CDCL. No default-solve handoff is used in this client.
    saved = []
    def policy(view, restored):
        if not restored:
            profile(view, len(original))
            assert view.c.tactix_set_guidance_publication(capi.TACTIX_GUIDANCE_NATIVE) == 0
            root_status = view.c.tactix_support(capi.TACTIX_SUPPORT_ROOT_PREPARATION, 0)
            if root_status == 20: return 20
            assert root_status == 0
            assert view.c.tactix_support(capi.TACTIX_SUPPORT_ENTER_INPROCESS_PHASE, capi.TACTIX_PHASE_ELIMINATE) == 0
            assert view.c.tactix_support(capi.TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION, 0) == 0
            try:
                candidates = []
                for variable in range(1, 9):
                    state = capi.tactix_bve_candidate_state()
                    if view.c.tactix_read_bve_candidate(variable, C.byref(state)) == 0:
                        candidates.append(variable)
                saved.append(BveOrder(view, candidates))
                ordered_phase(view, 8, instances=candidates)
            finally:
                assert view.c.tactix_support(capi.TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0) in (0, 20)
        return ScriptedCDCL()(view, restored)
    original = [[1,2,3],[-1,2,4],[1,-2,5],[-1,-2,-3],[3,4],[-3,5],[6,7],[-6,8],[-7,8],[-8,4]]
    with Session(library, strategy=policy) as session:
        for clause in original: session.add(clause)
        first = session.solve()
        assert first.status == 10 and all(any(l in first.model for l in c) for c in original)
        second = session.solve([-1,-2,-3])
        assert second.status == 20
    with tempfile.TemporaryDirectory(prefix='bve-order-session-') as tmp:
        stem = Path(tmp)/'query'
        second.export(stem)
        cnf, proof = Path(str(stem)+'.cnf'), Path(str(stem)+'.proof')
        assert not satisfiable(*formula(cnf))
        mode = 'pbp' if proof.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
        route = 'pbp' if mode == 'pbp' else ('dsr-cake_lpr' if certificate_route(proof) == 'drat' else 'dsr-trestle')
        result = runner.check_proof(mode, str(cnf), str(proof), 30, route=route)
        assert result[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), result
        checked += 1
    for order in saved:
        instance, begin = C.c_uint64(1234), C.c_uint32(5678)
        assert lib.tactix_bve_order_next(order._execution._view, order._handle,
            C.byref(instance), C.byref(begin)) == -1
        assert (instance.value, begin.value) == (1234, 5678)
        try: order.pop()
        except RuntimeError: pass
        else: raise AssertionError('expired execution order remained usable')
        order.close()

    closing, released = [], []
    def release_during_apply(_):
        if closing and closing[0]._handle:
            closing[0].close()
            released.append(True)
    def closing_policy(view, restored):
        assert not restored
        profile(view, 4)
        assert view.c.tactix_support(capi.TACTIX_SUPPORT_ROOT_PREPARATION, 0) == 0
        assert view.c.tactix_support(capi.TACTIX_SUPPORT_ENTER_INPROCESS_PHASE, capi.TACTIX_PHASE_ELIMINATE) == 0
        assert view.c.tactix_support(capi.TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION, 0) == 0
        with BveOrder(view, (1, 2)) as order:
            closing.append(order)
            instance, _ = order.pop()
            result = order.apply('BVE', instance)
            assert result == 20 and released and not order._handle
        assert view.c.tactix_support(capi.TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0) == 20
        return 20
    with Session(library, strategy=closing_policy) as session:
        for clause in ([1,2], [1,-2], [-1,2], [-1,-2]): session.add(clause)
        session.set_learn(release_during_apply, 2)
        result = session.solve()
        assert result.status == 20 and len(released) == 1
    with tempfile.TemporaryDirectory(prefix='bve-order-close-') as tmp:
        stem = Path(tmp)/'query';result.export(stem)
        cnf, proof = Path(str(stem)+'.cnf'), Path(str(stem)+'.proof')
        assert not satisfiable(*formula(cnf))
        mode = 'pbp' if proof.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
        route = 'pbp' if mode == 'pbp' else ('dsr-cake_lpr' if certificate_route(proof) == 'drat' else 'dsr-trestle')
        result = runner.check_proof(mode, str(cnf), str(proof), 30, route=route)
        assert result[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), result
        checked += 1

print(f'BVE ordering: {checked} checked refutations/negatives, {models} original models; '
      + ('native/C/Python' if library else 'native/C')
      + ' trajectories and same-format proof bytes agree')
