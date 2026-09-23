"""Strict statuses for the independent fixed-proof development oracles.

The legacy lrat-check executable is deliberately not an acceptance oracle:
6906 demonstrated false acceptance of an empty lemma on a satisfiable input.
Use cake_lpr for native LRAT, drat-trim for native DRAT, and the exact
dsr-trim -> binary LSR -> Trestle chain for native DSR.

Acceptance of a native DSR certificate routes by its content: DSR is a
superset of DRAT, so a certificate without any witnessed line is a DRAT proof
and is checked with the DRAT owner's pipeline (drat-trim -> cake_lpr), reached
by rewriting the binary line tags; a certificate with a witnessed line takes
the Satsuma+Kissat DSR pipeline with Trestle as the authority. The solver's
output does not change; only the checker side reads it differently.
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
    """Binary DSR marks every addition with 'a'; ASCII DSR never contains it."""
    try:
        with open(proof, 'rb') as source:
            head = source.read(4096)
    except OSError:
        return False
    return any(byte not in b'0123456789- \n\rd' for byte in head)


# Binary DSR line tags (dsr-trim's encoding) and their binary DRAT equivalents.
DSR_BINARY_ADD, DSR_BINARY_DELETE = 1, 2
DRAT_BINARY_ADD, DRAT_BINARY_DELETE = ord('a'), ord('d')


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
                raise ValueError(f'invalid binary DSR tag {tag} at {index - 1}')
            tokens = []
            while True:
                value, shift = 0, 0
                while True:
                    byte = data[index]
                    index += 1
                    value |= (byte & 127) << shift
                    shift += 7
                    if not byte & 128:
                        break
                if not value:
                    break
                tokens.append(value)
            yield kind, tokens
        return
    with open(proof, 'r', errors='replace') as source:
        for line in source:
            fields = line.split()
            if not fields:
                continue
            if fields[0] == 'd':
                kind, fields = 'd', fields[1:]
            else:
                kind = 'a'
            if not fields or fields[-1] != '0':
                raise ValueError('invalid ASCII DSR line')
            yield kind, [int(field) for field in fields[:-1]]


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
            raise ValueError(f'invalid binary DSR tag {tag} at {index}')
        index += 1
        first, count = None, 0
        while True:
            value, shift = 0, 0
            while True:
                byte = data[index]
                index += 1
                value |= (byte & 127) << shift
                shift += 7
                if not byte & 128:
                    break
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


def stage_environment(name):
    """Extra environment for one pipeline stage, merged over os.environ."""
    import os
    environment = dict(os.environ)
    if name == 'cake_lpr':
        environment.update(CAKE_LPR_ENVIRONMENT)
    return environment


def drat_stages(drat_trim, cake_lpr, cnf, drat, lrat):
    """The DRAT owner's pipeline, as the baselines and the fuzz harness run
    it: drat-trim elaborates to LRAT, cake_lpr (under CAKE_LPR_ENVIRONMENT)
    is the authority."""
    return [('drat-trim', [drat_trim, cnf, drat, '-L', lrat]),
            ('cake_lpr', [cake_lpr, cnf, lrat])]


def certificate_route(proof) -> Literal['drat', 'dsr']:
    """'drat' for a witness-free certificate, 'dsr' otherwise."""
    return 'dsr' if dsr_proof_has_witness(proof) else 'drat'


def reference_dsr_stages(dsr_trim, dsr_compress, srcheck, cnf, proof,
                         binary_lsr, text_lsr):
    """The (name, command) stages of the reference DSR pipeline, in order.

    dsr-trim mirrors its input encoding: the canonical binary DSR yields a
    binary LSR, which needs the expansion stage; a diagnostic ASCII DSR yields
    the text LSR directly, so there is nothing to expand."""
    if not dsr_proof_is_binary(proof):
        return [('dsr-trim', [dsr_trim, cnf, proof, text_lsr]),
                ('srcheck', [srcheck, cnf, text_lsr])]
    return [
        ('dsr-trim', [dsr_trim, cnf, proof, binary_lsr]),
        ('dsr-compress', [dsr_compress, '-d', binary_lsr, text_lsr]),
        ('srcheck', [srcheck, cnf, text_lsr]),
    ]


def every_lemma_dsr_stages(dsr_trim, dsr_compress, srcheck, cnf, proof,
                           binary_lsr, text_lsr):
    """The audit pipeline: the reference stages with forward elaboration.

    Backward dsr-trim elaborates only the lemmas the refutation uses, so an
    invalid lemma outside that core is never examined.  Forward elaboration
    (`-f`) checks every line.  It is far too slow for benchmark certificates
    and is not the acceptance authority; the fuzz harness and the proof suites
    run it in addition to the reference pipeline, where proofs are small."""
    stages = reference_dsr_stages(dsr_trim, dsr_compress, srcheck, cnf, proof,
                                  binary_lsr, text_lsr)
    name, command = stages[0]
    assert name == 'dsr-trim'
    stages[0] = (name, [command[0], '-f'] + command[1:])
    return stages


def dsr_compress_status(returncode, output) -> Literal['verified', 'error']:
    """dsr-compress converts a format; it is not an acceptance oracle."""
    return ('verified' if returncode == 0 and 'ERROR' not in output and
            'not supported' not in output else 'error')


def check_routed(dsr_trim, dsr_compress, srcheck, drat_trim, cake_lpr, cnf,
                 proof, workdir, timeout=None):
    """Acceptance by content: (verified, failing stage, output, route)."""
    import subprocess
    route = certificate_route(proof)
    if route == 'dsr':
        verified, stage, output = check_dsr(
            dsr_trim, dsr_compress, srcheck, cnf, proof, workdir, timeout)
        return verified, stage, output, route
    workdir = Path(workdir)
    drat, lrat = workdir / 'proof.drat', workdir / 'proof.lrat'
    for stale in (drat, lrat):
        if stale.exists():
            stale.unlink()
    if not dsr_proof_as_drat(proof, drat):
        return False, 'route', 'witnessed line met during rewrite', route
    for name, command in drat_stages(drat_trim, cake_lpr, cnf, drat, lrat):
        try:
            done = subprocess.run([str(part) for part in command],
                                  capture_output=True, text=True,
                                  errors='replace', timeout=timeout,
                                  env=stage_environment(name))
        except subprocess.TimeoutExpired:
            return False, name, 'timed out', route
        if proof_check_status(name, done.returncode, done.stdout,
                              done.stderr) != 'verified':
            return False, name, done.stdout + '\n' + done.stderr, route
        if name == 'drat-trim' and not (lrat.is_file() and lrat.stat().st_size):
            return False, name, 'no LRAT produced', route
    return True, None, '', route


def check_dsr(dsr_trim, dsr_compress, srcheck, cnf, proof, workdir,
              timeout=None, every_lemma=False):
    """Run the reference pipeline; return (verified, failing stage, output).

    With `every_lemma` the forward audit pipeline runs afterwards and must
    verify too; its failing stage is reported with an `every-lemma:` prefix."""
    verified, stage, output = _run_dsr_stages(
        reference_dsr_stages, dsr_trim, dsr_compress, srcheck, cnf, proof,
        workdir, timeout)
    if verified and every_lemma:
        verified, stage, output = _run_dsr_stages(
            every_lemma_dsr_stages, dsr_trim, dsr_compress, srcheck, cnf,
            proof, workdir, timeout)
        if not verified:
            stage = 'every-lemma:' + stage
    return verified, stage, output


def _run_dsr_stages(stages, dsr_trim, dsr_compress, srcheck, cnf, proof,
                    workdir, timeout):
    import subprocess
    workdir = Path(workdir)
    binary_lsr, text_lsr = workdir / 'proof.binary.lsr', workdir / 'proof.lsr'
    for stale in (binary_lsr, text_lsr):
        if stale.exists():
            stale.unlink()
    for name, command in stages(
            dsr_trim, dsr_compress, srcheck, cnf, proof, binary_lsr, text_lsr):
        try:
            done = subprocess.run([str(part) for part in command],
                                  capture_output=True, text=True,
                                  errors='replace', timeout=timeout)
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
    parser.add_argument('--workdir', default=None)
    parser.add_argument('--every-lemma', action='store_true',
                        help='also elaborate forward, checking every line')
    args = parser.parse_args(argv)
    with tempfile.TemporaryDirectory() as temporary:
        if args.command == 'check':
            verified, stage, output, route = check_routed(
                args.dsr_trim, args.dsr_compress, args.srcheck,
                args.drat_trim, args.cake_lpr, args.cnf, args.proof,
                args.workdir or temporary)
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
