"""Strict statuses for the independent fixed-proof development oracles.

The legacy lrat-check executable is deliberately not an acceptance oracle:
6906 demonstrated false acceptance of an empty lemma on a satisfiable input.
Use cake_lpr for native LRAT, drat-trim for native DRAT, and the exact
dsr-trim -> binary LSR -> Trestle chain for native DSR.
"""
from typing import Literal
from pathlib import Path


def _at_most_three_integer_tokens(path):
    """Read a possible trivial LSR without materializing a real proof."""
    tokens = []
    value = 0
    sign = 1
    digits = 0
    in_token = False
    try:
        with Path(path).open('rb') as source:
            while True:
                chunk = source.read(64 * 1024)
                if not chunk:
                    break
                for byte in chunk:
                    if byte in b' \t\r\n\v\f':
                        if not in_token:
                            continue
                        if not digits:
                            return None
                        tokens.append(sign * value)
                        if len(tokens) > 3:
                            return None
                        value, sign, digits, in_token = 0, 1, 0, False
                    elif not in_token and byte == ord('-'):
                        sign, in_token = -1, True
                    elif ord('0') <= byte <= ord('9'):
                        value = 10 * value + byte - ord('0')
                        digits += 1
                        in_token = True
                    else:
                        return None
        if in_token:
            if not digits:
                return None
            tokens.append(sign * value)
        return tokens if len(tokens) <= 3 else None
    except OSError:
        return None


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
    tokens = _at_most_three_integer_tokens(lsr_path)
    if tokens is None:
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


def _binary_token(data, index):
    """Decode one dsr-trim signed varint, rejecting truncation/overflow."""
    value = 0
    shift = 0
    while index < len(data) and shift < 64:
        byte = data[index]
        index += 1
        value |= (byte & 0x7f) << shift
        if byte < 0x80:
            number = -(value >> 1) if value & 1 else value >> 1
            return number, index
        shift += 7
    return None


def _encode_binary_token(number):
    value = (abs(number) << 1) | (number < 0)
    encoded = bytearray()
    while True:
        byte = value & 0x7f
        value >>= 7
        encoded.append(byte | (0x80 if value else 0))
        if not value:
            return bytes(encoded)


def bridge_trivial_input_binary_lsr(cnf_path, lsr_path) -> bool:
    """Add the missing input-empty hint to dsr-trim's binary trivial LSR."""
    lsr_path = Path(lsr_path)
    try:
        data = lsr_path.read_bytes()
    except OSError:
        return False
    if len(data) > 32 or not data or data[0] != 3:
        return False
    decoded = []
    index = 1
    while index < len(data):
        token = _binary_token(data, index)
        if token is None:
            return False
        value, index = token
        decoded.append(value)
        if len(decoded) > 3:
            return False
    if len(decoded) != 3 or decoded[1:] != [0, 0]:
        return False

    # Reuse the narrowly-scoped textual bridge to validate the CNF and obtain
    # the unique first empty-clause hint without duplicating DIMACS parsing.
    temporary = lsr_path.with_name(lsr_path.name + '.trivial-text.tmp')
    try:
        temporary.write_text(f'{decoded[0]} 0 0\n')
        if not bridge_trivial_input_lsr(cnf_path, temporary):
            return False
        tokens = [int(token) for token in temporary.read_text().split()]
    finally:
        temporary.unlink(missing_ok=True)
    lsr_path.write_bytes(bytes([3]) + b''.join(
        _encode_binary_token(token) for token in tokens))
    return True
