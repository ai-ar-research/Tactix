"""Exercise the public ER strategy surface through ctypes and persistent queries."""
from proof_test_support import proof_runner
from pathlib import Path
import ctypes as C
import sys
import tempfile

library, root = map(Path, sys.argv[1:])
sys.path[:0] = [str(root/'python'), str(root/'tools/bench')]
from tactix import Session, ScriptedCDCL, capi
runner = proof_runner(root, checkers=())
from proof_oracle import certificate_route
lib = capi.load_library(library)
rules = {lib.tactix_rule_name(i).decode(): i for i in range(lib.tactix_rule_count())}
assert lib.tactix_action_kind(rules['ExtendedResolution']) == capi.TACTIX_ACTION_PROOF_RULE


def regions(view, queued):
    assert view.tactix_regions_begin() == 0
    assert view.tactix_regions_begin() == -1
    start = view.tactix_fresh_variable_start()
    assert view.tactix_define_extension(0, 1, 2) == -1
    assert view.tactix_define_extension(capi.TACTIX_EXTENSION_AND, 0, 2) == -1
    assert view.tactix_fresh_variable_start() == start
    a = view.tactix_define_extension(capi.TACTIX_EXTENSION_AND, 1, 2)
    b = view.tactix_define_extension(capi.TACTIX_EXTENSION_AND, 2, 1)
    z = view.tactix_define_extension(capi.TACTIX_EXTENSION_XOR, a, b)
    assert (a, b, z) == (start, start+1, start+2)
    assert view.tactix_define_extension(capi.TACTIX_EXTENSION_OR, 1, 2) == start+3
    assert view.tactix_variable_retained(z) == 1
    assert view.tactix_set_variable_retention(z, 0) == 0
    assert view.tactix_variable_retained(z) == 0
    assert view.tactix_set_variable_retention(z, 1) == 0
    assert view.tactix_bump_variable(z, 3) == 0
    variables = (C.c_int * 7)(z, a, 2, b, 1, a, z)
    region = view.tactix_region_add(variables, len(variables))
    assert region == 0
    variables[:] = [0]*len(variables)  # The native region owns its input.
    assert view.tactix_region_apply(rules['Probe'], z, region, 1) == -1
    assert view.tactix_region_queue(rules['Sweep'], z, region+1, 4096) == -1
    if queued:
        assert view.tactix_region_queue(rules['Probe'], z, region, 0) == 0
        assert view.tactix_region_queue(rules['Sweep'], z, region, 4096) == 0
    else:
        assert view.tactix_region_apply(rules['Probe'], z, region, 0) == 1
        assert view.tactix_region_apply(rules['Sweep'], z, region, 4096) == 0
        assert view.tactix_stat(capi.TACTIX_STAT_REGION_UNITS) >= 1
        assert view.tactix_stat(capi.TACTIX_STAT_REGION_LEMMAS) > 1
    return start


def verify(stem):
    proof = stem.with_suffix('.proof')
    kind = 'pbp' if proof.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
    route = 'pbp' if kind == 'pbp' else ('dsr-cake_lpr' if certificate_route(proof) == 'drat' else 'dsr-trestle')
    checked = runner.check_proof(kind, str(stem.with_suffix('.cnf')), str(proof), 30, route=route)
    assert checked[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), checked


models = proofs = 0
with tempfile.TemporaryDirectory(prefix='python-er-') as tmp:
    folder = Path(tmp)
    # All clausal routes share the original client formula as their oracle.
    for fmt in (capi.TACTIX_PROOF_DSR_BINARY, capi.TACTIX_PROOF_PBP, capi.TACTIX_PROOF_AUTO):
        for queued in (False, True):
            for unsat in (False, True):
                formula = [[1, 2]] + ([[3, 4], [3, -4], [-3, 4], [-3, -4]] if unsat else [])
                stem = folder/f'{fmt}-{int(queued)}-{int(unsat)}'
                proof = stem.with_suffix('.proof')
                handle = lib.tactix_init()
                try:
                    assert lib.tactix_proof(handle, str(proof).encode(), fmt) == 0
                    for clause in formula:
                        for literal in [*clause, 0]: lib.tactix_add(handle, literal)
                    view = capi.SolverView(lib, handle, lambda: None)
                    regions(view, queued)
                    assert view.tactix_solve() == (20 if unsat else 10)
                    assert view.tactix_stat(capi.TACTIX_STAT_REGION_QUERIES) == 2
                    assert view.tactix_stat(capi.TACTIX_STAT_REGION_UNITS) == 1
                    if not unsat:
                        model = {view.tactix_value(v) for v in range(1, 3)}
                        assert all(any(lit in model for lit in clause) for clause in formula)
                        models += 1
                    assert view.tactix_regions_clear() == 0
                finally: lib.tactix_release(handle)
                if unsat:
                    cnf = stem.with_suffix('.cnf')
                    cnf.write_text(f'p cnf 4 {len(formula)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in formula))
                    verify(stem)
                    proofs += 1
    for queued in (False, True):
        starts = []
        def strategy(execution, restored):
            starts.append(regions(execution.c, queued))
            if queued:
                result = execution.default_solve()
            else:
                result = ScriptedCDCL()(execution, restored)
            assert execution.c.tactix_stat(capi.TACTIX_STAT_REGION_QUERIES) >= 2
            assert execution.c.tactix_regions_clear() == 0
            return result
        with Session(library, strategy=strategy) as session:
            session.add([1, 2])
            result = session.solve()
            assert result.status == 10 and any(lit in result.model for lit in (1, 2))
            models += 1
            # Client identities may later collide with helpers from a prior query.
            session.add([starts[0], starts[0]+1])
            session.add([starts[0]+2])  # Collides with the proven negative helper root.
            result = session.solve([-1])
            assert result.status == 10 and -1 in result.model and 2 in result.model
            assert any(lit in result.model for lit in (starts[0], starts[0]+1))
            assert starts[0]+2 in result.model
            models += 1
        with Session(library, strategy=strategy) as session:
            for clause in [[1,2],[3,4],[3,-4],[-3,4],[-3,-4]]: session.add(clause)
            result = session.solve()
            assert result.status == 20
            stem = folder/f'session-{int(queued)}'
            result.export(stem)
        verify(stem)
        proofs += 1
    # The final empty clause must use the ER equivalence q <-> input 1:
    # regional Probe derives -q, then native Propagate derives -1 and refutes F.
    contradiction = [[1,2],[1,-2],[-1,2],[-1,-2]]
    for fmt in (capi.TACTIX_PROOF_DSR_BINARY, capi.TACTIX_PROOF_PBP, capi.TACTIX_PROOF_AUTO):
        stem = folder/f'dependent-{fmt}'
        stem.with_suffix('.cnf').write_text('p cnf 2 4\n'+''.join(' '.join(map(str,c))+' 0\n' for c in contradiction))
        handle = lib.tactix_init()
        try:
            assert lib.tactix_proof(handle, str(stem.with_suffix('.proof')).encode(), fmt) == 0
            for clause in contradiction:
                for literal in [*clause,0]: lib.tactix_add(handle,literal)
            view = capi.SolverView(lib, handle, lambda: None)
            assert view.tactix_regions_begin() == 0
            q = view.tactix_define_extension(capi.TACTIX_EXTENSION_AND,1,1)
            assert q == 3
            region = view.tactix_region_add((C.c_int*3)(1,2,q),3)
            assert view.tactix_region_apply(rules['Probe'],q,region,0) == 0
            assert view.tactix_apply(rules['Propagate']) == 20
        finally: lib.tactix_release(handle)
        verify(stem)
        proofs += 1
    polls, armed = [0], [False]
    def terminate():
        if not armed[0]: return False
        polls[0] += 1
        return polls[0] >= 3
    def stopped_queue(execution, restored):
        regions(execution.c, True)
        armed[0] = True
        try:
            assert execution.default_solve() == 0
            assert execution.c.tactix_stat(capi.TACTIX_STAT_REGION_QUERIES) == 1
        finally: armed[0] = False
        return 0
    with Session(library, strategy=stopped_queue) as session:
        session.add([1,2])
        session.set_terminate(terminate)
        assert session.solve().status == 0 and polls[0] == 3
        session.set_terminate(None)
        session.set_strategy(ScriptedCDCL())
        result = session.solve()
        assert result.status == 10 and any(lit in result.model for lit in (1,2))
        models += 1
print(f'Python ER: {models} original-input models, {proofs} checked proofs; direct/queued regions and Session helper growth pass')
