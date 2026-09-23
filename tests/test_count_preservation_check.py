#!/usr/bin/env python3
"""Independent certificates for fixed signed count-conservation networks."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/bench'))
from proof_oracle import check_routed, pb_proof_has_header

pins = ROOT / 'benchmarks/baselines/sc24-26-32g-5000s/pins'
with tempfile.TemporaryDirectory(prefix='count-preservation-') as directory:
    work = Path(directory)
    subprocess.run([sys.argv[1], str(work)], check=True)
    proofs = list(work.glob('*.pbp'))
    assert len(proofs) == 4
    for proof in proofs:
        cnf = proof.with_suffix('.cnf')
        for candidate in (proof, work / 'automatic.pbp'):
            if candidate != proof:
                result = subprocess.run([sys.argv[2], str(cnf), str(candidate)],
                                        capture_output=True, text=True)
                assert result.returncode == 20, result
            assert pb_proof_has_header(candidate)
            checked = check_routed(None, None, None, None, None, cnf, candidate,
                                   work, timeout=60, veripb=pins/'veripb',
                                   cake_pb=pins/'cake-pb-cnf')
            assert checked[0], (proof.name, checked)
        no_proof = subprocess.run([sys.argv[2], str(cnf)], capture_output=True)
        assert no_proof.returncode == 20
        print(proof.name, 'API + default VeriPB/CakePB verified; no-proof agrees')
    # Original identities survive duplicate literals, duplicate clauses, and
    # tautologies; input-normalization DSR remains private and is discarded.
    lines = (work/'count3r.cnf').read_text().splitlines()
    clauses = [line.split()[0] + ' ' + line for line in lines[1:]]
    clauses += ['1 -1 0', clauses[0]]
    variables = lines[0].split()[2]
    cnf = work/'normalized.cnf'
    cnf.write_text(f'p cnf {variables} {len(clauses)}\n'+'\n'.join(clauses)+'\n')
    proof = work/'normalized.pbp'
    result = subprocess.run([sys.argv[2], str(cnf), str(proof)], capture_output=True)
    assert result.returncode == 20 and pb_proof_has_header(proof), result
    checked = check_routed(None, None, None, None, None, cnf, proof, work,
                           timeout=60, veripb=pins/'veripb', cake_pb=pins/'cake-pb-cnf')
    assert checked[0], checked
    # Deleting the derivation must not be accepted as a counting certificate.
    proof.write_text('pseudo-Boolean proof version 3.0\noutput NONE ;\n'
                     'conclusion UNSAT : -1 ;\nend pseudo-Boolean proof ;\n')
    assert not check_routed(None, None, None, None, None, cnf, proof, work,
                            timeout=60, veripb=pins/'veripb',
                            cake_pb=pins/'cake-pb-cnf')[0]
    print('raw-normalization proof verified; missing-derivation control rejected')
