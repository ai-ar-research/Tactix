#!/usr/bin/env python3
"""Fixed CLI feedback/replay checks; no campaign or performance claim."""
import hashlib
import gzip
import importlib.util
import json
import lzma
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/bench'))
sys.path.insert(0, str(ROOT / 'tools/fuzz'))
from proof_oracle import check_routed
from modelcheck import check_files

spec = importlib.util.spec_from_file_location('feedback_inspector', ROOT / 'tools/bench/inspect_policy_feedback.py')
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)


def main():
    binary = Path(sys.argv[1]).resolve()
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(('TACTIX_', 'SAT_'))}
    verified = {'proofs': 0, 'models': 0}
    with tempfile.TemporaryDirectory(prefix='tactix-feedback-frontend-') as temporary:
        work = Path(temporary)
        sat = work / 'sat.cnf'
        sat.write_text('p cnf 2 3\n1 2 0\n-1 2 0\n1 -2 0\n')
        unsat = work / 'unsat.cnf'
        unsat.write_text('p cnf 2 4\n1 2 0\n-1 2 0\n1 -2 0\n-1 -2 0\n')

        def run(cnf=sat, options=(), env=None, proof=None):
            args = [str(binary), '-q', '--drat', '--no-binary', *options, str(cnf)]
            if proof is not None:
                args.append(str(proof))
            started = time.monotonic()
            result = subprocess.run(args,
                                    env=environment if env is None else env,
                                    capture_output=True, text=True, timeout=30)
            elapsed = time.monotonic() - started
            if elapsed > 2:
                print(f'slow frontend solve ({elapsed:.2f}s): {options}',
                      file=sys.stderr, flush=True)
            return result

        def oracle(result, cnf, proof, name):
            if result.returncode == 10:
                model = work / f'{name}.model'
                model.write_text(result.stdout)
                ok, message = check_files(str(cnf), str(model))
                assert ok, message
                verified['models'] += 1
            else:
                assert result.returncode == 20, result.stderr
                output = work / name
                output.mkdir()
                ok, stage, message, _ = check_routed(None, None, None,
                    ROOT / 'tools/checkers/bin/drat-trim', ROOT / 'tools/checkers/bin/cake_lpr',
                    cnf, proof, output, timeout=60)
                assert ok, (stage, message)
                verified['proofs'] += 1

        def metadata_fields(value):
            source = value.encode()
            assert source.startswith(b'tactix-frontend-feedback-v1\n')
            source = source.split(b'\n', 1)[1]
            fields = {}
            while source:
                key, source = source.split(b'=', 1)
                length, source = source.split(b':', 1)
                count = int(length)
                fields[key.decode()] = source[:count].decode()
                assert source[count:count + 1] == b'\n'
                source = source[count + 1:]
            return fields

        saved = saved_solver_output = saved_certificate = None
        for cnf, expected in [(sat, 10), (unsat, 20)]:
            name = 'default-' + cnf.stem
            options = []
            records = work / f'{name}.jsonl'
            before = run(cnf, options, proof=work / f'{name}-off.drat')
            recorded = run(cnf, [*options, f'--feedback={records}'], proof=work / f'{name}-on.drat')
            replayed = run(cnf, [*options, f'--feedback-replay={records}'], proof=work / f'{name}-replay.drat')
            assert before.returncode == recorded.returncode == replayed.returncode == expected, (
                name, before.returncode, recorded.returncode, replayed.returncode,
                before.stderr, recorded.stderr, replayed.stderr)
            assert before.stdout == recorded.stdout == replayed.stdout
            certificates = [(work / f'{name}-{arm}.drat').read_bytes() for arm in ('off', 'on', 'replay')]
            assert certificates[0] == certificates[1] == certificates[2]
            summary = inspector.inspect(records)
            assert summary['terminal']['verdict'] == expected
            fields = metadata_fields(summary['metadata'])
            assert fields['build-sha256'] == hashlib.sha256(binary.read_bytes()).hexdigest()
            assert fields['input-sha256'] == hashlib.sha256(cnf.read_bytes()).hexdigest()
            assert fields['policy'] == 'default'
            oracle(recorded, cnf, work / f'{name}-on.drat', name)
            if cnf == sat:
                saved = records.read_text()
                saved_solver_output = recorded.stdout
                saved_certificate = certificates[1]

        records = work / 'negative.jsonl'
        records.write_text(saved)
        mismatch = run(options=[f'--feedback-replay={records}'])
        assert mismatch.returncode == 1 and mismatch.stdout == saved_solver_output
        assert 'feedback verification' in mismatch.stderr  # Proof enablement is provenance.
        changed_env = dict(environment, TACTIX_FORCE_MODE='stable')
        assert run(options=[f'--feedback-replay={records}'], env=changed_env,
                   proof=work / 'different-config.drat').returncode == 1
        for malformed in (saved.rsplit('\n', 2)[0] + '\n', saved + '{}\n'):
            records.write_text(malformed)
            outcome = run(options=[f'--feedback-replay={records}'], proof=work / 'negative.drat')
            assert outcome.returncode == 1 and outcome.stdout == saved_solver_output
            assert (work / 'negative.drat').read_bytes() == saved_certificate
            assert 'feedback verification' in outcome.stderr  # No solver abort/fallback.
        rows = saved.splitlines(keepends=True)
        first_action = next(i for i, row in enumerate(rows) if '"type":"action"' in row)
        rows[first_action] = rows[first_action].replace('"sequence":1', '"sequence":2', 1)
        records.write_text(''.join(rows))
        outcome = run(options=[f'--feedback-replay={records}'], proof=work / 'divergence.drat')
        assert outcome.returncode == 1 and outcome.stdout == saved_solver_output
        assert (work / 'divergence.drat').read_bytes() == saved_certificate
        assert 'divergence' in outcome.stderr
        # Only timing records change; all deterministic fields remain identical.
        rows = saved.splitlines(keepends=True)
        for index, row in enumerate(rows):
            if row.startswith('{"type":"elapsed_nanos"'):
                value = json.loads(row)['value'] + 999
                rows[index] = '{"type":"elapsed_nanos","value":' + str(value) + '}\n'
        records.write_text(''.join(rows))
        assert run(options=[f'--feedback-replay={records}'], proof=work / 'timing.drat').returncode == 10
        relocated = work / 'relocated.cnf'
        relocated.write_bytes(sat.read_bytes())
        assert run(relocated, [f'--feedback-replay={records}'], proof=work / 'relocated.drat').returncode == 10
        records.write_text(saved)
        changed = work / 'changed.cnf'
        changed.write_text(sat.read_text() + 'c content changed\n')
        assert run(changed, [f'--feedback-replay={records}'], proof=work / 'changed.drat').returncode == 1
        for extension, compress in [('gz', lambda raw: gzip.compress(raw, mtime=0)), ('xz', lzma.compress)]:
            cnf = work / f'sat.cnf.{extension}'
            cnf.write_bytes(compress(sat.read_bytes()))
            compressed = work / f'{extension}.jsonl'
            recorded = run(cnf, [f'--feedback={compressed}'])
            replayed = run(cnf, [f'--feedback-replay={compressed}'])
            assert recorded.returncode == replayed.returncode == 10
            assert recorded.stdout == replayed.stdout
            fields = metadata_fields(inspector.inspect(compressed)['metadata'])
            assert fields['input-sha256'] == hashlib.sha256(cnf.read_bytes()).hexdigest()

        # File aliases must be rejected before opening any output.
        alias = work / 'alias.cnf'
        alias.symlink_to(sat)
        hardlink = work / 'hardlink.cnf'
        os.link(sat, hardlink)
        original = sat.read_bytes()
        for target in (sat, alias, hardlink):
            assert run(options=[f'--feedback={target}']).returncode == 1
            assert sat.read_bytes() == original
        same = work / 'same-output'
        assert run(options=[f'--feedback={same}'], proof=same).returncode == 1
        assert not same.exists()
        for options in ([f'--feedback={records}', f'--feedback-replay={records}'],
                        [f'--feedback={records}', '--random-policy=7'],
                        ['--feedback='], ['--feedback-replay=']):
            assert run(options=options).returncode == 1
        unavailable = run(options=[f'--feedback={work / "missing" / "record"}'])
        assert unavailable.returncode == 1 and unavailable.stdout == saved_solver_output
        assert 'feedback verification' in unavailable.stderr
        missing_replay = run(options=[f'--feedback-replay={work / "nonexistent-record"}'])
        assert missing_replay.returncode == 1 and missing_replay.stdout == saved_solver_output

        # Actual frontend interruption decisions are invariant under feedback.
        for period in ('1', '7', '31'):
            env = dict(environment, TACTIX_STOP_FUZZ=period)
            baseline = run(unsat, env=env)
            observed = run(unsat, [f'--feedback={work / "interruption.jsonl"}'], env=env)
            assert baseline.returncode == observed.returncode and baseline.stdout == observed.stdout
            inspector.inspect(work / 'interruption.jsonl')
        assert verified == {'proofs': 1, 'models': 1}
        print('Default frontend purity/replay and rejection controls passed; proof + model checked.')


if __name__ == '__main__':
    main()
