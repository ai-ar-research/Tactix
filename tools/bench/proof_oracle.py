"""Strict statuses for the independent fixed-proof development oracles.

The legacy lrat-check executable is deliberately not an acceptance oracle:
6906 demonstrated false acceptance of an empty lemma on a satisfiable input.
Use cake_lpr for native LRAT, drat-trim for native DRAT, and the exact
dsr-trim -> binary LSR -> Trestle chain for native DSR.

Acceptance of a native DSR certificate routes by its content: DSR is a
superset of DRAT, so a certificate without any witnessed line is a DRAT proof
and is checked with the DRAT owner's pipeline (drat-trim -> cake_lpr), reached
by rewriting the binary line tags; a certificate with a witnessed line takes
the Satsuma+Kissat DSR pipeline with Trestle as the authority. Automatic startup
certificates may instead carry PBP: VeriPB elaborates it and CakePB checks the
result against the same original CNF. Both stages must verify.
"""
from typing import Literal
from pathlib import Path


def canonical_checker_cnf(original, destination):
    """Spell the original clause sequence for restricted verified readers.

    No clause/literal/order/multiplicity changes are permitted. The only header
    change is 0 -> 1 variables: an unused identity, needed by pinned Trestle.
    DRAT elaboration also needs this spelling: its reader interprets leading
    zeros as octal and mishandles CNF comments with binary proofs. Reject
    malformed input rather than silently repairing it into a different formula.
    """
    import hashlib
    if Path(destination).exists() and Path(original).samefile(destination):
        raise ValueError('checker spelling must not overwrite the original CNF')
    declared = None
    clauses = 0
    in_clause = False
    first_empty_clause = None
    duplicate_literal_clauses = 0
    seen_literals, repeated = set(), False
    digest = hashlib.sha256()
    spelled_digest = hashlib.sha256()
    with Path(original).open('rb') as source, Path(destination).open('wb') as target:
        def write(data):
            target.write(data)
            spelled_digest.update(data)

        for line_number, raw in enumerate(source, 1):
            digest.update(raw)
            line = raw.strip()
            if not line or line.startswith(b'c'):
                continue
            fields = line.split()
            if fields[0] == b'p':
                if (declared is not None or len(fields) != 4 or
                        fields[:2] != [b'p', b'cnf'] or
                        any(not item.isdigit() for item in fields[2:])):
                    raise ValueError(f'invalid DIMACS header at line {line_number}')
                header_numbers = [item.lstrip(b'0') or b'0' for item in fields[2:]]
                if len(header_numbers[0]) > 10 or len(header_numbers[1]) > 19:
                    raise ValueError('DIMACS header exceeds checker integer bounds')
                variables, count = map(int, header_numbers)
                if variables > 2147483647 or count > 9223372036854775807:
                    raise ValueError('DIMACS header exceeds checker integer bounds')
                declared = variables, count
                write(f'p cnf {max(1, variables)} {count}\n'.encode('ascii'))
                continue
            if declared is None:
                raise ValueError('DIMACS clauses precede the header')
            output = bytearray()
            for token in fields:
                digits = token[1:] if token.startswith(b'-') else token
                if not digits or not digits.isdigit():
                    raise ValueError(f'invalid DIMACS literal at line {line_number}')
                digits = digits.lstrip(b'0') or b'0'
                if len(digits) > 10:
                    raise ValueError(f'DIMACS literal exceeds header at line {line_number}')
                literal = int(digits) * (-1 if token.startswith(b'-') else 1)
                if abs(literal) > declared[0]:
                    raise ValueError(f'DIMACS literal exceeds header at line {line_number}')
                if literal:
                    repeated |= literal in seen_literals
                    seen_literals.add(literal)
                    output.extend(str(literal).encode('ascii') + b' ')
                    in_clause = True
                else:
                    output.extend(b'0\n')
                    clauses += 1
                    if not in_clause and first_empty_clause is None:
                        first_empty_clause = clauses
                    duplicate_literal_clauses += repeated
                    seen_literals.clear()
                    repeated = False
                    in_clause = False
                if clauses > declared[1]:
                    raise ValueError('DIMACS clause count exceeds header')
            write(output)
        if declared is None or in_clause or clauses != declared[1]:
            raise ValueError('missing DIMACS header, clause terminator, or declared clauses')
    return dict(original_sha256=digest.hexdigest(), checker_sha256=spelled_digest.hexdigest(), clauses=clauses,
                first_empty_clause=first_empty_clause,
                duplicate_literal_clauses=duplicate_literal_clauses,
                declared_variables=declared[0], checker_variables=max(1, declared[0]),
                transformation='clause-sequence-preserving DIMACS spelling')


def restate_duplicate_input_lrat(checker_cnf, spelling, lrat, destination):
    """Lift drat-trim's duplicate-normalized input hints to the original CNF.

    Each non-tautological repeated-literal input is restated by a fresh RUP
    clause with its original ID as the sole hint, then the original is deleted.
    Remap both RUP/RAT hints and deletions, shifting derived IDs past the prefix.
    CakeLPR still checks the lifted proof against the unmodified original clauses.
    The source LRAT is never overwritten. This is not used for native LRAT.
    """
    if not spelling['duplicate_literal_clauses']:
        return Path(lrat), 0
    for source in (checker_cnf, lrat):
        if Path(destination).exists() and Path(source).samefile(destination):
            raise ValueError('LRAT restatement must not overwrite its inputs')
    count = spelling['clauses']
    renamed = {}
    with Path(destination).open('w') as target:
        with Path(checker_cnf).open() as source:
            next(source)  # validated canonical header
            clause, identifier = [], 0
            for line in source:
                for token in line.split():
                    literal = int(token)
                    if literal:
                        clause.append(literal)
                        continue
                    identifier += 1
                    unique = dict.fromkeys(clause)
                    # Tautologies never supply a unit/conflict hint. Do not
                    # emit checker-divergent tautological RUP lemmas.
                    if (len(unique) < len(clause) and
                            not any(-lit in unique for lit in unique)):
                        new_id = count + len(renamed) + 1
                        renamed[identifier] = new_id
                        target.write(f'{new_id} ' + ' '.join(map(str, unique)) +
                                     f' 0 {identifier} 0\n')
                    clause.clear()
        if not renamed:
            return Path(lrat), 0
        offset = len(renamed)
        target.write(f'{count + offset} d ' + ' '.join(map(str, renamed)) + ' 0\n')

        def reference(token):
            value = int(token)
            index = abs(value)
            mapped = renamed.get(index, index if index <= count else index + offset)
            return str(-mapped if value < 0 else mapped)

        with Path(lrat).open() as source:
            for number, line in enumerate(source, 1):
                fields = line.split()
                if not fields:
                    continue
                if len(fields) < 3 or fields[-1] != '0':
                    raise ValueError(f'invalid converted LRAT line {number}')
                # A deletion's line label may repeat the last addition ID.
                label = str(int(fields[0]) + offset)
                if fields[1] == 'd':
                    rewritten = [label, 'd', *(reference(x) for x in fields[2:-1]), '0']
                else:
                    boundary = fields.index('0', 1)
                    rewritten = [label, *fields[1:boundary+1],
                                 *(reference(x) for x in fields[boundary+1:-1]), '0']
                target.write(' '.join(rewritten) + '\n')
    return Path(destination), len(renamed)


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
                       stderr: str, *, input_has_empty_clause=None
                       ) -> Literal['verified', 'rejected', 'error']:
    if checker not in ('drat-trim', 'cake_lpr', 'dsr-trim', 'srcheck',
                        'veripb', 'cake-pb-cnf'):
        raise ValueError('unsupported acceptance oracle: ' + checker)
    lines = [line.strip() for line in (stdout + '\n' + stderr).splitlines()]
    if ('s NOT VERIFIED' in lines or 's INVALID' in lines or
            any('Checking failed at' in line or
                'empty clause not derived at end of proof' in line
                for line in lines)):
        return 'rejected'
    success = ('s VERIFIED UNSATISFIABLE' if checker == 'veripb'
               else 's VERIFIED' if checker == 'drat-trim'
               else 's VERIFIED UNSAT')
    # drat-trim's parse-time trivial-UNSAT branch prints the verified token but
    # does not update `sts` before returning `(sts != UNSAT)`, so that one exact
    # success path exits 1.  It is source-confirmed and distinct from a generic
    # abnormal exit carrying success-looking text.
    trivial_drat = (checker == 'drat-trim' and returncode == 1 and
                    'c trivial UNSAT' in lines and success in lines)
    # Callers with validated input metadata also enforce the semantic condition
    # for this exceptional exit. A stand-in cannot invent an input empty clause.
    if trivial_drat and input_has_empty_clause is False:
        return 'rejected'
    # A recognized token from the wrong checker, a mere substring, or any other
    # success-looking message followed by an abnormal exit is not acceptance.
    if (returncode == 0 and success in lines) or trivial_drat:
        return 'verified'
    if checker == 'dsr-trim' and returncode != 0 and any(
            line.startswith(('Error:', 'ERROR:')) for line in lines):
        return 'rejected'
    return 'error'


def bridge_trivial_input_lrat(spelling, lrat_path, returncode, stdout, stderr=''):
    """Complete only drat-trim's parse-time empty-input LRAT omission.

    ``spelling`` must be metadata from canonical_checker_cnf on this input.
    Pinned drat-trim opens -L but exits 1 without writing a hint when an input
    clause is empty. Supply the exact original clause ID, then require CakeLPR
    as usual. Never overwrite a nonempty derivation or create a missing output.
    """
    empty_id = spelling['first_empty_clause']
    if (empty_id is None or returncode != 1 or
            proof_check_status('drat-trim', returncode, stdout, stderr,
                               input_has_empty_clause=True) != 'verified'):
        return False
    lrat = Path(lrat_path)
    if not lrat.is_file() or lrat.stat().st_size != 0:
        return False
    lrat.write_text(f"{spelling['clauses'] + 1} 0 {empty_id} 0\n")
    return True


def _trivial_input_empty_id(cnf_path, derived_id):
    cnf_path = Path(cnf_path)
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
                        return None
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
        return None
    if (declared is None or current_nonempty or clause_id != declared or
            empty_id is None or derived_id != declared + 1):
        return None
    return empty_id


def bridge_trivial_input_lsr(cnf_path, lsr_path) -> bool:
    """Add the missing input-empty hint in dsr-trim's trivial LSR output.

    Pinned dsr-trim verifies a CNF containing an empty clause and emits the
    syntactically valid but hintless line ``n+1 0 0``. Trestle correctly
    rejects that line. Replace only that exact output with ``n+1 0 k 0``, where
    input clause k is empty. Trestle remains the authority for acceptance.
    """
    cnf_path, lsr_path = Path(cnf_path), Path(lsr_path)
    tokens = _at_most_three_integer_tokens(lsr_path)
    if tokens is None or len(tokens) != 3 or tokens[1:] != [0, 0]:
        return False
    empty_id = _trivial_input_empty_id(cnf_path, tokens[0])
    if empty_id is None:
        return False
    lsr_path.write_text(f'{tokens[0]} 0 {empty_id} 0\n')
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

    empty_id = _trivial_input_empty_id(cnf_path, decoded[0])
    if empty_id is None:
        return False
    tokens = (decoded[0], 0, empty_id, 0)
    lsr_path.write_bytes(bytes([3]) + b''.join(
        _encode_binary_token(token) for token in tokens))
    return True


# --- the DSR checking pipeline ------------------------------------------------
# One home for how a native-DSR certificate is checked.  It is the SC2026
# verified SR pipeline exactly as the Satsuma+Kissat reference runs it
# (tools/bench/frozen_baseline.py, amended DSR branch), parameters included:
# dsr-trim takes no options, so it checks backward and trims to the core; a
# binary DSR yields a binary LSR; `dsr-compress -d` expands that to text; and
# Trestle verifies the text LSR.

def dsr_proof_is_binary(proof) -> bool:
    """Native binary DSR uses control-byte tags absent from ASCII DSR."""
    try:
        with open(proof, 'rb') as source:
            head = source.read(4096)
    except OSError:
        return False
    return any(byte not in b'0123456789- \t\n\r\v\fd' for byte in head)


# Binary DSR line tags (dsr-trim's encoding) and their binary DRAT equivalents.
DSR_BINARY_ADD, DSR_BINARY_DELETE = 1, 2
DRAT_BINARY_ADD, DRAT_BINARY_DELETE = ord('a'), ord('d')


class DSRFormatError(ValueError):
    """Malformed certificate bytes, distinct from runner or filesystem failure."""


def _dsr_binary_token(data, index):
    """Read one bounded uint32 literal encoding, preserving its original bytes."""
    start, value, shift = index, 0, 0
    while True:
        if index == len(data):
            raise DSRFormatError(f'truncated binary DSR token at byte {start}')
        byte = data[index]
        index += 1
        value |= (byte & 127) << shift
        if shift == 28 and (byte & 240):
            raise DSRFormatError(f'oversized binary DSR token at byte {start}')
        if not byte & 128:
            return value, index
        shift += 7


def _dsr_lines(proof, binary):
    """Yield (kind, tokens) per DSR line: kind is 'a' or 'd', tokens the
    encoded literals before the terminating zero (binary varint values or
    ASCII integers). Only the first two tokens are needed by callers that
    detect a witness, but every line is delimited exactly."""
    if binary:
        with open(proof, 'rb') as source:
            data = source.read()
        index, size = 0, len(data)
        while index < size:
            tag = data[index]
            index += 1
            if tag == DSR_BINARY_ADD:
                kind = 'a'
            elif tag == DSR_BINARY_DELETE:
                kind = 'd'
            else:
                raise DSRFormatError(f'invalid binary DSR tag {tag} at {index - 1}')
            tokens = []
            while True:
                value, index = _dsr_binary_token(data, index)
                if not value:
                    break
                tokens.append(value)
            yield kind, tokens
        return
    with open(proof, 'r', errors='replace') as source:
        for number, line in enumerate(source, 1):
            fields = line.split()
            if not fields:
                continue
            if fields[0] == 'd':
                kind, fields = 'd', fields[1:]
            else:
                kind = 'a'
            if not fields or fields[-1] != '0':
                raise DSRFormatError(f'unterminated ASCII DSR line {number}')
            try:
                tokens = [int(field) for field in fields[:-1]]
            except ValueError as error:
                raise DSRFormatError(f'invalid ASCII DSR token on line {number}') from error
            yield kind, tokens


def dsr_proof_has_witness(proof) -> bool:
    """True when any addition carries a substitution witness, i.e. its first
    literal (the pivot) reappears later in the line. A witnessed line is an SR
    step and is never a DRAT line; drat-trim would silently drop the repeated
    pivot as a duplicate literal, so routing must decide before drat-trim."""
    for kind, tokens in _dsr_lines(proof, dsr_proof_is_binary(proof)):
        if kind == 'a' and len(tokens) > 1 and tokens[0] in tokens[1:]:
            return True
    return False


def dsr_proof_as_drat(proof, drat):
    """Write the DRAT form of a witness-free DSR certificate to `drat`.

    ASCII DSR without witnesses is already ASCII DRAT and is copied. Binary DSR
    differs from binary DRAT only in its two line tags (1/2 against 'a'/'d');
    the literal varints are identical, so the tags are rewritten in place and
    every other byte is copied. Returns False on a witnessed line."""
    binary = dsr_proof_is_binary(proof)
    if not binary:
        if dsr_proof_has_witness(proof):
            return False
        with open(proof, 'rb') as source, open(drat, 'wb') as target:
            while chunk := source.read(1 << 24):
                target.write(chunk)
        return True
    with open(proof, 'rb') as source:
        data = bytearray(source.read())
    index, size = 0, len(data)
    while index < size:
        tag = data[index]
        if tag == DSR_BINARY_ADD:
            data[index] = DRAT_BINARY_ADD
        elif tag == DSR_BINARY_DELETE:
            data[index] = DRAT_BINARY_DELETE
        else:
            raise DSRFormatError(f'invalid binary DSR tag {tag} at {index}')
        index += 1
        first, count = None, 0
        while True:
            value, index = _dsr_binary_token(data, index)
            if not value:
                break
            if count == 0:
                first = value
            elif tag == DSR_BINARY_ADD and value == first:
                return False
            count += 1
    with open(drat, 'wb') as target:
        target.write(data)
    return True


# The frozen baseline's checker amendment 2: CakeML's 4096 MiB default heap
# exhausts on large certificates, so the reference runs cake_lpr with a
# 24576 MiB heap (the pipeline's parameter, not the binary's).
CAKE_LPR_ENVIRONMENT = {'CML_HEAP_SIZE': '24576'}


# Python's poll selector converts seconds to signed-C-int milliseconds. Bound
# each wait, not the caller's allowance (nor drat-trim's independent -t value).
MAX_CAPTURE_WAIT_SECONDS = ((1 << 31) - 1) // 1000


def run_captured(command, timeout=None, **kwargs):
    """Capture a process like subprocess.run without overflowing poll's wait.

    Ordinary allowances keep the existing subprocess.run path. Large ones use
    repeated communicate calls under ONE monotonic deadline; communicate keeps
    partially collected output between calls. Always reap on timeout or error.
    Keyword arguments are Popen options, e.g. env and preexec_fn.
    """
    import math
    import subprocess
    import time
    if timeout is None or timeout <= MAX_CAPTURE_WAIT_SECONDS:
        return subprocess.run(command, capture_output=True, text=True,
                              errors='replace', timeout=timeout, **kwargs)
    if not math.isfinite(timeout):
        raise ValueError('process timeout must be finite')
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          text=True, errors='replace', **kwargs) as process:
        deadline = time.monotonic() + timeout
        try:
            while True:
                remaining = max(0, deadline - time.monotonic())
                try:
                    stdout, stderr = process.communicate(
                        timeout=min(remaining, MAX_CAPTURE_WAIT_SECONDS))
                    return subprocess.CompletedProcess(command, process.returncode,
                                                       stdout, stderr)
                except subprocess.TimeoutExpired:
                    if time.monotonic() < deadline:
                        continue
                    process.kill()
                    stdout, stderr = process.communicate()
                    raise subprocess.TimeoutExpired(command, timeout,
                                                    output=stdout, stderr=stderr)
        except BaseException:
            process.kill()
            process.wait()
            raise


def stage_environment(name):
    """Extra environment for one pipeline stage, merged over os.environ."""
    import os
    environment = dict(os.environ)
    if name in ('cake_lpr', 'cake-pb-cnf'):
        environment.update(CAKE_LPR_ENVIRONMENT)
    return environment


def drat_command_with_timeout(command, timeout):
    """Do not let drat-trim's 40,000s default shorten an explicit allowance.

    Its -t parser uses a signed C int. Round fractional seconds up; the caller's
    original wall-clock timeout remains authoritative and is never enlarged.
    Keep the input/proof arguments and final LRAT output position unchanged.
    """
    import math
    if timeout is None:
        return list(command)
    if (isinstance(timeout, bool) or not isinstance(timeout, (int, float)) or
            timeout <= 0 or timeout > 2147483647 or not math.isfinite(timeout)):
        raise ValueError('drat-trim timeout must fit a positive signed C int')
    return list(command[:3]) + ['-t', str(math.ceil(timeout))] + list(command[3:])


def drat_stages(drat_trim, cake_lpr, cnf, drat, lrat, *, checker_cnf=None):
    """The DRAT owner's pipeline, as the baselines and the fuzz harness run
    it: drat-trim elaborates to LRAT, cake_lpr (under CAKE_LPR_ENVIRONMENT)
    is the authority."""
    return [('drat-trim', [drat_trim, checker_cnf or cnf, drat, '-L', lrat]),
            ('cake_lpr', [cake_lpr, checker_cnf or cnf, lrat])]


def pb_stages(veripb, cake_pb, cnf, proof, kernel, *, checker_cnf=None):
    """Pinned LymphoSAT pipeline; both readers use clause-preserving spelling.

    CakePB independently verifies VeriPB's elaborated proof.
    """
    return [('veripb', [veripb, '-u', '--cnf', '--elaborate', kernel,
                        checker_cnf or cnf, proof]),
            ('cake-pb-cnf', [cake_pb, checker_cnf or cnf, kernel])]


def pb_proof_has_header(proof) -> bool:
    with open(proof, 'rb') as source:
        return source.read(32).startswith(b'pseudo-Boolean proof version ')


def certificate_route(proof) -> Literal['drat', 'dsr', 'pbp']:
    """Recognize PBP's header before parsing DSR; extension is irrelevant."""
    if pb_proof_has_header(proof):
        return 'pbp'
    return 'dsr' if dsr_proof_has_witness(proof) else 'drat'


def reference_dsr_stages(dsr_trim, dsr_compress, srcheck, cnf, proof,
                         binary_lsr, text_lsr, *, checker_cnf=None):
    """The (name, command) stages of the reference DSR pipeline, in order.

    dsr-trim mirrors its input encoding: the canonical binary DSR yields a
    binary LSR, which needs the expansion stage; a diagnostic ASCII DSR yields
    the text LSR directly, so there is nothing to expand. Both CNF readers use
    the validated clause-preserving spelling when supplied."""
    if not dsr_proof_is_binary(proof):
        return [('dsr-trim', [dsr_trim, checker_cnf or cnf, proof, text_lsr]),
                ('srcheck', [srcheck, checker_cnf or cnf, text_lsr])]
    return [
        ('dsr-trim', [dsr_trim, checker_cnf or cnf, proof, binary_lsr]),
        ('dsr-compress', [dsr_compress, '-d', binary_lsr, text_lsr]),
        ('srcheck', [srcheck, checker_cnf or cnf, text_lsr]),
    ]


def every_lemma_dsr_stages(dsr_trim, dsr_compress, srcheck, cnf, proof,
                           binary_lsr, text_lsr, *, checker_cnf=None):
    """The audit pipeline: the reference stages with forward elaboration.

    Backward dsr-trim elaborates only the lemmas the refutation uses, so an
    invalid lemma outside that core is never examined.  Forward elaboration
    (`-f`) checks every line.  It is far too slow for benchmark certificates
    and is not the acceptance authority; the fuzz harness and the proof suites
    run it in addition to the reference pipeline, where proofs are small."""
    stages = reference_dsr_stages(dsr_trim, dsr_compress, srcheck, cnf, proof,
                                  binary_lsr, text_lsr, checker_cnf=checker_cnf)
    name, command = stages[0]
    assert name == 'dsr-trim'
    stages[0] = (name, [command[0], '-f'] + command[1:])
    return stages


def dsr_compress_status(returncode, output) -> Literal['verified', 'error']:
    """dsr-compress converts a format; it is not an acceptance oracle."""
    return ('verified' if returncode == 0 and 'ERROR' not in output and
            'not supported' not in output else 'error')


def check_routed(dsr_trim, dsr_compress, srcheck, drat_trim, cake_lpr, cnf,
                 proof, workdir, timeout=None, *, veripb=None, cake_pb=None):
    """Acceptance by content: (verified, failing stage, output, route)."""
    import subprocess
    try:
        route = certificate_route(proof)
    except DSRFormatError as error:
        return False, 'route', str(error), 'invalid-dsr'
    if route == 'pbp':
        verified, stage, output = check_pb(
            veripb, cake_pb, cnf, proof, workdir, timeout)
        return verified, stage, output, route
    if route == 'dsr':
        verified, stage, output = check_dsr(
            dsr_trim, dsr_compress, srcheck, cnf, proof, workdir, timeout)
        return verified, stage, output, route
    workdir = Path(workdir)
    drat, lrat = workdir / 'proof.drat', workdir / 'proof.lrat'
    checker_cnf = workdir / 'checker.cnf'
    try:
        spelling = canonical_checker_cnf(cnf, checker_cnf)
    except (OSError, ValueError) as error:
        return False, 'cnf', str(error), route
    for stale in (drat, lrat):
        if stale.exists():
            stale.unlink()
    if not dsr_proof_as_drat(proof, drat):
        return False, 'route', 'witnessed line met during rewrite', route
    checked_lrat = lrat
    for name, command in drat_stages(drat_trim, cake_lpr, cnf, drat, lrat,
                                     checker_cnf=checker_cnf):
        if name == 'drat-trim':
            command = drat_command_with_timeout(command, timeout)
        else:
            command = [*command[:-1], checked_lrat]
        try:
            done = run_captured([str(part) for part in command], timeout=timeout,
                                env=stage_environment(name))
        except subprocess.TimeoutExpired:
            return False, name, 'timed out', route
        if proof_check_status(name, done.returncode, done.stdout,
                              done.stderr, input_has_empty_clause=
                              spelling['first_empty_clause'] is not None) != 'verified':
            return False, name, done.stdout + '\n' + done.stderr, route
        if name == 'drat-trim':
            bridge_trivial_input_lrat(spelling, lrat, done.returncode,
                                      done.stdout, done.stderr)
            if not (lrat.is_file() and lrat.stat().st_size):
                return False, name, 'no LRAT produced', route
            try:
                checked_lrat, _ = restate_duplicate_input_lrat(
                    checker_cnf, spelling, lrat, workdir / 'proof.original-input.lrat')
            except ValueError as error:
                return False, 'input-restatement', str(error), route
    return True, None, '', route


def check_pb(veripb, cake_pb, cnf, proof, workdir, timeout=None):
    """Require both PB stages within one complete pipeline time allowance."""
    import subprocess
    import time
    if veripb is None or cake_pb is None:
        return False, 'pins', 'PBP requires pinned VeriPB and CakePB'
    started = time.monotonic()
    kernel = Path(workdir) / 'proof.kernel.pbp'
    checker_cnf = Path(workdir) / 'checker.cnf'
    try:
        canonical_checker_cnf(cnf, checker_cnf)
    except (OSError, ValueError) as error:
        return False, 'cnf', str(error)
    # Never accept a stale elaboration after a checker fails to produce one.
    if kernel.exists():
        kernel.unlink()
    for name, command in pb_stages(veripb, cake_pb, cnf, proof, kernel,
                                   checker_cnf=checker_cnf):
        remaining = (None if timeout is None else
                     timeout - (time.monotonic() - started))
        if remaining is not None and remaining <= 0:
            return False, name, 'timed out'
        try:
            done = run_captured([str(part) for part in command], timeout=remaining,
                                env=stage_environment(name))
        except subprocess.TimeoutExpired:
            return False, name, 'timed out'
        except OSError as error:
            return False, name, str(error)
        output = done.stdout + '\n' + done.stderr
        if proof_check_status(name, done.returncode, done.stdout,
                              done.stderr) != 'verified':
            return False, name, output
        if name == 'veripb' and not (kernel.is_file() and kernel.stat().st_size):
            return False, name, 'no kernel PBP produced'
    return True, None, ''


def check_dsr(dsr_trim, dsr_compress, srcheck, cnf, proof, workdir,
              timeout=None, every_lemma=False):
    """Run the reference pipeline; return (verified, failing stage, output).

    With `every_lemma` the forward audit pipeline runs afterwards and must
    verify too; its failing stage is reported with an `every-lemma:` prefix."""
    checker_cnf = Path(workdir) / 'checker.cnf'
    try:
        canonical_checker_cnf(cnf, checker_cnf)
    except (OSError, ValueError) as error:
        return False, 'cnf', str(error)
    verified, stage, output = _run_dsr_stages(
        reference_dsr_stages, dsr_trim, dsr_compress, srcheck, cnf, proof,
        workdir, timeout, checker_cnf)
    if verified and every_lemma:
        verified, stage, output = _run_dsr_stages(
            every_lemma_dsr_stages, dsr_trim, dsr_compress, srcheck, cnf,
            proof, workdir, timeout, checker_cnf)
        if not verified:
            stage = 'every-lemma:' + stage
    return verified, stage, output


def _run_dsr_stages(stages, dsr_trim, dsr_compress, srcheck, cnf, proof,
                    workdir, timeout, checker_cnf):
    import subprocess
    workdir = Path(workdir)
    binary_lsr, text_lsr = workdir / 'proof.binary.lsr', workdir / 'proof.lsr'
    for stale in (binary_lsr, text_lsr):
        if stale.exists():
            stale.unlink()
    for name, command in stages(
            dsr_trim, dsr_compress, srcheck, cnf, proof, binary_lsr, text_lsr,
            checker_cnf=checker_cnf):
        try:
            done = run_captured([str(part) for part in command], timeout=timeout)
        except subprocess.TimeoutExpired:
            return False, name, 'timed out'
        output = done.stdout + '\n' + done.stderr
        if name == 'dsr-compress':
            ok = (dsr_compress_status(done.returncode, output) == 'verified'
                  and text_lsr.is_file() and text_lsr.stat().st_size > 0)
        else:
            ok = proof_check_status(name, done.returncode, done.stdout,
                                    done.stderr) == 'verified'
        if name == 'dsr-trim' and ok:
            made = Path(command[-1])
            ok = made.is_file() and made.stat().st_size > 0
            if ok:
                (bridge_trivial_input_binary_lsr if made == binary_lsr
                 else bridge_trivial_input_lsr)(cnf, made)
        if not ok:
            return False, name, output
    return True, None, ''


def _main(argv):
    import argparse
    import tempfile
    here = Path(__file__).resolve().parents[2] / 'tools/checkers/bin'
    parser = argparse.ArgumentParser(
        description='check-dsr: the reference dsr-trim | dsr-compress -d | '
                    'Trestle pipeline. check: acceptance routed by content '
                    '(witness-free -> drat-trim | cake_lpr).')
    parser.add_argument('command', choices=['check-dsr', 'check'])
    parser.add_argument('cnf')
    parser.add_argument('proof')
    parser.add_argument('--dsr-trim', default=str(here / 'dsr-trim'))
    parser.add_argument('--dsr-compress', default=str(here / 'dsr-compress'))
    parser.add_argument('--srcheck', default=str(here / 'srcheck'))
    parser.add_argument('--drat-trim', default=str(here / 'drat-trim'))
    parser.add_argument('--cake-lpr', default=str(here / 'cake_lpr'))
    # No unpinned fallback: callers explicitly select the frozen PB tools.
    parser.add_argument('--veripb')
    parser.add_argument('--cake-pb')
    parser.add_argument('--workdir', default=None)
    parser.add_argument('--every-lemma', action='store_true',
                        help='also elaborate forward, checking every line')
    args = parser.parse_args(argv)
    with tempfile.TemporaryDirectory() as temporary:
        if args.command == 'check':
            verified, stage, output, route = check_routed(
                args.dsr_trim, args.dsr_compress, args.srcheck,
                args.drat_trim, args.cake_lpr, args.cnf, args.proof,
                args.workdir or temporary, veripb=args.veripb,
                cake_pb=args.cake_pb)
            print(f'c route {route}')
        else:
            verified, stage, output = check_dsr(
                args.dsr_trim, args.dsr_compress, args.srcheck, args.cnf,
                args.proof, args.workdir or temporary,
                every_lemma=args.every_lemma)
    if verified:
        print('s VERIFIED UNSAT')
        return 0
    print(f'FAIL: {stage} did not verify {args.proof}')
    print(output[-4000:])
    return 1


if __name__ == '__main__':
    import sys
    sys.exit(_main(sys.argv[1:]))
