"""Strict statuses for the independent fixed-proof development oracles.

The legacy lrat-check executable is deliberately not an acceptance oracle:
6906 demonstrated false acceptance of an empty lemma on a satisfiable input.
Use cake_lpr for native LRAT, and drat-trim for native DRAT.
"""
from typing import Literal


def proof_check_status(checker: str, returncode: int, stdout: str,
                       stderr: str) -> Literal['verified', 'rejected', 'error']:
    if checker not in ('drat-trim', 'cake_lpr'):
        raise ValueError('unsupported acceptance oracle: ' + checker)
    lines = [line.strip() for line in (stdout + '\n' + stderr).splitlines()]
    if ('s NOT VERIFIED' in lines or
            any('Checking failed at' in line or
                'empty clause not derived at end of proof' in line
                for line in lines)):
        return 'rejected'
    success = 's VERIFIED' if checker == 'drat-trim' else 's VERIFIED UNSAT'
    # A recognized token from the wrong checker, a mere substring, or a
    # success-looking message followed by an abnormal exit is not acceptance.
    if returncode == 0 and success in lines:
        return 'verified'
    return 'error'
