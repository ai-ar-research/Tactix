#!/usr/bin/env python3
"""Fixed structural micro-proofs, independently checked against original CNF."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/bench'))
from proof_oracle import check_routed, pb_proof_has_header

pins = ROOT / 'benchmarks/baselines/sc24-26-32g-5000s/pins'
with tempfile.TemporaryDirectory(prefix='startup-parity-check-') as directory:
    work = Path(directory)
    subprocess.run([sys.argv[1], str(work)], check=True)
    proofs = list(work.glob('*.pbp'))
    assert len(proofs) >= 6
    for proof in proofs:
        result = check_routed(None, None, None, None, None,
                              proof.with_suffix('.cnf'), proof, work,
                              timeout=60, veripb=pins/'veripb',
                              cake_pb=pins/'cake-pb-cnf')
        assert result[0], (proof.name, result)
        print(proof.name, 'VeriPB + CakePB verified')
        if len(sys.argv) > 2:
            # The default CLI must use the same shared action as the API fixture.
            automatic = work / 'automatic.proof'
            solve = subprocess.run([sys.argv[2], str(proof.with_suffix('.cnf')),
                                    str(automatic)], capture_output=True, text=True)
            assert solve.returncode == 20, solve
            assert pb_proof_has_header(automatic)
            checked = check_routed(None, None, None, None, None,
                                   proof.with_suffix('.cnf'), automatic, work,
                                   timeout=60, veripb=pins/'veripb',
                                   cake_pb=pins/'cake-pb-cnf')
            assert checked[0], checked
    if len(sys.argv) > 2:
        # Normalized kernel clauses must still be justified against raw CNF
        # identities when the private normalization prefix is discarded.
        original = (work/'k4.cnf').read_text().splitlines()
        clauses = []
        for line in original[1:]:
            fields = line.split()
            clauses.append(fields[0] + ' ' + line)  # a duplicate literal
        clauses += ['1 -1 0', clauses[0]]  # tautology and repeated clause
        cnf = work/'normalized.cnf'
        cnf.write_text(f'p cnf 6 {len(clauses)}\n'+'\n'.join(clauses)+'\n')
        proof = work/'normalized.pbp'
        solve = subprocess.run([sys.argv[2], str(cnf), str(proof)],
                               capture_output=True, text=True)
        assert solve.returncode == 20 and pb_proof_has_header(proof), solve
        checked = check_routed(None, None, None, None, None, cnf, proof, work,
                               timeout=60, veripb=pins/'veripb',
                               cake_pb=pins/'cake-pb-cnf')
        assert checked[0], checked
        print('normalized.pbp VeriPB + CakePB verified against raw duplicates/tautology')
