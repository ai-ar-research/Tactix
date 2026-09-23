#!/usr/bin/env python3
"""Fixed owner handoff: Congruence ladders must not outlive their inference."""
from pathlib import Path
import subprocess
import sys
import tempfile
import resource
import signal

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools/bench'))
from proof_oracle import check_dsr

case = ROOT/'tools/fuzz/corpus/regressions/032-congruence-transient-sr'
pins = ROOT/'benchmarks/runs/20260921-am333-lymphosat-models-wv100/pins'
with tempfile.TemporaryDirectory(prefix='congruence-sr-check-') as directory:
    work = Path(directory)
    for mode in ([], ['--dsr'], ['--no-binary']):
        for program in ('--random-policy=335110095', '--replay='+str(case/'min.trace')):
            proof = work/'proof.dsr'
            result = subprocess.run([sys.argv[1], *mode, program,
                                     str(case/'min.cnf'), str(proof)],
                                    capture_output=True, text=True, timeout=60)
            assert result.returncode == 20, result
            checked = check_dsr(pins/'dsr-trim', pins/'dsr-compress', pins/'srcheck',
                                case/'min.cnf', proof, work, timeout=60,
                                every_lemma=True)
            assert checked[0], (mode, program, checked)
            print(mode, program.split('=')[0], 'backward + forward DSR/Trestle verified')
    cnf = work/'contradictory-units.cnf'
    cnf.write_text('p cnf 1 2\n1 0\n-1 0\n')
    def disable_core():
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    for mode in ([], ['--dsr'], ['--no-binary'], ['--drat'],
                 ['--drat', '--no-binary'], ['--lrat']):
        result = subprocess.run([sys.argv[1], *mode, str(cnf), '/dev/full'],
                                capture_output=True, text=True, timeout=60,
                                preexec_fn=disable_core)
        assert result.returncode == -signal.SIGABRT, (mode, result)
        assert 's UNSATISFIABLE' not in result.stdout, (mode, result)
    print('all CLI formats reject failed publication before UNSAT')
