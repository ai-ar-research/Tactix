"""Strict statuses for the independent fixed-proof development oracles.

The legacy lrat-check executable is deliberately not an acceptance oracle:
6906 demonstrated false acceptance of an empty lemma on a satisfiable input.
Use cake_lpr for native LRAT, drat-trim for native DRAT, and the exact
dsr-trim -> textual LSR -> Trestle chain for native DSR.
"""
from typing import Literal
from pathlib import Path


def proof_check_status(checker: str, returncode: int, stdout: str,
                       stderr: str) -> Literal['verified', 'rejected', 'error']:
    if checker not in ('drat-trim', 'cake_lpr', 'dsr-trim', 'srcheck'):
        raise ValueError('unsupported acceptance oracle: ' + checker)
    lines = [line.strip() for line in (stdout + '\n' + stderr).splitlines()]
    if ('s NOT VERIFIED' in lines or 's INVALID' in lines or
            any('Checking failed at' in line or
                'empty clause not derived at end of proof' in line
                for line in lines)):
        return 'rejected'
    success = ('s VERIFIED' if checker == 'drat-trim'
               else 's VERIFIED UNSAT')
    # drat-trim's parse-time trivial-UNSAT branch prints the verified token but
    # does not update `sts` before returning `(sts != UNSAT)`, so that one exact
    # success path exits 1.  It is source-confirmed and distinct from a generic
    # abnormal exit carrying success-looking text.
    trivial_drat = (checker == 'drat-trim' and returncode == 1 and
                    'c trivial UNSAT' in lines and success in lines)
    # A recognized token from the wrong checker, a mere substring, or any other
    # success-looking message followed by an abnormal exit is not acceptance.
    if (returncode == 0 and success in lines) or trivial_drat:
        return 'verified'
    if checker == 'dsr-trim' and returncode != 0 and any(
            line.startswith(('Error:', 'ERROR:')) for line in lines):
        return 'rejected'
    return 'error'


def bridge_trivial_input_lsr(cnf_path, lsr_path) -> bool:
    """Add the missing input-empty hint in dsr-trim's trivial LSR output.

    Pinned dsr-trim verifies a CNF containing an empty clause and emits the
    syntactically valid but hintless line ``n+1 0 0``. Trestle correctly
    rejects that line. Replace only that exact output with ``n+1 0 k 0``, where
    input clause k is empty. Trestle remains the authority for acceptance.
    """
    cnf_path, lsr_path = Path(cnf_path), Path(lsr_path)
    try:
        tokens = [int(token) for token in lsr_path.read_text().split()]
    except (OSError, ValueError):
        return False
    if len(tokens) != 3 or tokens[1:] != [0, 0]:
        return False

    declared = None
    clause_id = 0
    current_nonempty = False
    empty_id = None
    try:
        with cnf_path.open(errors='replace') as source:
            for raw in source:
                line = raw.strip()
                if not line or line.startswith('c'):
                    continue
                if line.startswith('p'):
                    fields = line.split()
                    if (declared is not None or len(fields) != 4 or
                            fields[:2] != ['p', 'cnf']):
                        return False
                    declared = int(fields[3])
                    continue
                for token in line.split():
                    lit = int(token)
                    if lit:
                        current_nonempty = True
                    else:
                        clause_id += 1
                        if not current_nonempty and empty_id is None:
                            empty_id = clause_id
                        current_nonempty = False
    except (OSError, ValueError):
        return False
    if (declared is None or current_nonempty or clause_id != declared or
            empty_id is None or tokens[0] != declared + 1):
        return False
    lsr_path.write_text(f'{declared + 1} 0 {empty_id} 0\n')
    return True
