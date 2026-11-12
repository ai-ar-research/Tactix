"""Independent C/Python CRUX control over the same public solver interface."""
from pathlib import Path
import ctypes as C
import subprocess
import sys
import tempfile
from proof_test_support import check_unsat, proof_runner, satisfiable

library, root, compiler = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
sys.path.insert(0, str(root/'python'))
from tactix import capi
from tactix.strategies import Cruxmiter

runner = proof_runner(root)
lib = capi.load_library(library)
rules = {lib.tactix_rule_name(i).decode(): i for i in range(lib.tactix_rule_count())}

REFERENCE = r'''
#include "tactix.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static struct action { int rule, result; } *actions;
static size_t count, capacity;
static int traced_apply(tactix *s, int rule) {
  if (count == capacity) {
    capacity = capacity ? 2*capacity : 64;
    actions = realloc(actions, capacity*sizeof *actions);
    assert(actions);
  }
  const int result = tactix_apply(s, rule);
  actions[count++] = (struct action){rule, result};
  return result;
}
#define tactix_apply traced_apply
#include "strategy/presets/cruxmiter.c"
#undef tactix_apply
int reference_run(tactix *s, const char *path, uint64_t *counts) {
  cruxmiter_program program;
  assert(cruxmiter_begin(&program));
  const int result = cruxmiter_run(&program, s, NULL, NULL, NULL);
  FILE *trace = fopen(path, "w");
  assert(trace);
  for (size_t i = 0; i < count; ++i)
    fprintf(trace, "%d %d\n", actions[i].rule, actions[i].result);
  assert(!fclose(trace));
  counts[0] = program.requests; counts[1] = program.eligible;
  counts[2] = program.effective; counts[3] = program.rounds;
  free(actions); actions = NULL; count = capacity = 0;
  return result;
}
'''

# Equal AND gates exercise merging and substitution. Their disequality refutes
# during root preprocessing, while the satisfiable case reaches search handoff.
gates = [[-3,1],[-3,2],[3,-1,-2],[-4,1],[-4,2],[4,-1,-2]]
cases = (gates, gates + [[3,4],[-3,-4]], [[]], [[1],[-1]])
with tempfile.TemporaryDirectory(prefix='python-crux-parity-') as raw:
    directory = Path(raw)
    reference = directory/'reference.c'
    reference.write_text(REFERENCE)
    subprocess.run([compiler, '-shared', '-fPIC', '-O2', '-I'+str(root/'include'),
                    '-I'+str(root/'src'), str(reference), str(library),
                    '-Wl,-rpath,'+str(library.parent), '-o', str(directory/'reference.so')], check=True)
    ref = C.CDLL(str(directory/'reference.so'))
    for name, result, arguments in (
            ('cruxmiter_configure', C.c_int, [C.c_void_p]),
            ('cruxmiter_setting', C.c_char_p, [C.c_size_t, C.POINTER(C.c_char_p)]),
            ('reference_run', C.c_int, [C.c_void_p, C.c_char_p, C.POINTER(C.c_uint64)])):
        function = getattr(ref, name)
        function.restype, function.argtypes = result, arguments
    settings = []
    for index in range(len(Cruxmiter.settings)+1):
        value = C.c_char_p()
        name = ref.cruxmiter_setting(index, C.byref(value))
        if name is None:
            break
        settings.append((name.decode(), value.value.decode()))
    else:
        raise AssertionError('unterminated C CRUX settings table')
    assert tuple(settings) == Cruxmiter.settings

    merged = handed_off = 0
    for case, clauses in enumerate(cases):
        cnf = directory/f'{case}.cnf'
        cnf.write_text('p cnf 4 '+str(len(clauses))+'\n'+
                       ''.join(' '.join(map(str, clause))+' 0\n' for clause in clauses))
        expected = 10 if satisfiable(4, clauses) else 20
        trajectory = None
        for mode in (None, capi.TACTIX_PROOF_DSR_ASCII, capi.TACTIX_PROOF_PBP, capi.TACTIX_PROOF_AUTO):
            answers = []
            for native in (True, False):
                handle = lib.tactix_init()
                policy = Cruxmiter()
                proof = directory/f'{case}-{mode}-{native}.proof'
                try:
                    if native:
                        assert ref.cruxmiter_configure(handle) == 1
                    else:
                        for name, value in policy.settings:
                            assert lib.tactix_set_default_option(handle, name.encode(), value.encode()) == 0
                    if mode is not None:
                        assert lib.tactix_proof(handle, str(proof).encode(), mode) == 0
                    for clause in clauses:
                        for literal in (*clause, 0):
                            lib.tactix_add(handle, literal)
                    assert lib.tactix_set_guidance_publication(handle, capi.TACTIX_GUIDANCE_NATIVE) == 0
                    if native:
                        counts = (C.c_uint64*4)()
                        trace = directory/'actions'
                        result = ref.reference_run(handle, str(trace).encode(), counts)
                        actions = [tuple(map(int, line.split())) for line in trace.read_text().splitlines()]
                        counts = tuple(counts)
                    else:
                        result = policy.run(capi.SolverView(lib, handle, lambda: None), rules)
                        actions = [(rule, code) for rule, code, _, _ in policy.trace]
                        counts = policy.requests, policy.eligible, policy.effective, policy.rounds
                    handed_off += result == 30
                    if result == 30:
                        result = lib.tactix_solve(handle)
                    assert result == expected
                    counters = tuple(lib.tactix_stat(handle, i) for i in range(
                        1+max(value for name, value in capi.schema.CONSTANTS.items()
                              if name.startswith('TACTIX_STAT_'))))
                    model = tuple(lib.tactix_value(handle, v) for v in range(1, 5)) if result == 10 else ()
                    if model:
                        assert all(any(literal in model for literal in clause) for clause in clauses)
                    merged += counters[capi.TACTIX_STAT_CONGRUENCE_MERGES]
                finally:
                    lib.tactix_release(handle)
                data = proof.read_bytes() if mode is not None else b''
                if result == 20 and mode is not None:
                    check_unsat(runner, str(cnf), str(proof))
                answers.append((result, counters, model, actions, counts, data))
            assert answers[0] == answers[1], (case, mode, answers)
            if trajectory is None:
                trajectory = answers[0][:5]
            assert answers[0][:5] == trajectory, (case, mode, 'proof-dependent search')
    assert merged and handed_off
print('C/Python CRUX actions, counters, models and proof bytes agree in all four proof modes')
