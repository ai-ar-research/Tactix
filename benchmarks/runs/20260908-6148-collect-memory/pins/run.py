#!/usr/bin/env python3
"""Select a per-task read-only observer around an immutable checked runner."""
import importlib.util
import os
from pathlib import Path
import sys

VARIABLE = 'TACTIX_SLOT_MEMORY_TRACE'


def install_observer(protocol, campaign, task, binary):
    """Observe only the solver, preserving the runner's oracles and metrics."""
    task = int(task)
    if task < 0:
        raise ValueError('negative task')
    campaign = Path(campaign).resolve()
    binary = str(Path(binary).resolve())
    path = campaign / 'logs' / f'{task:03d}.slot-memory.tsv'
    original = protocol.measured

    def observe(command, stdout, stderr, metrics, timeout):
        if binary not in set(map(str, command)):
            return original(command, stdout, stderr, metrics, timeout)
        if path.exists():
            raise FileExistsError(path)
        previous = os.environ.get(VARIABLE)
        os.environ[VARIABLE] = str(path)
        try:
            result = original(command, stdout, stderr, metrics, timeout)
        finally:
            if previous is None:
                os.environ.pop(VARIABLE, None)
            else:
                os.environ[VARIABLE] = previous
        # Preserve failures that precede the first slot (e.g. solver OOM).
        result['memory_trace'] = (
            dict(path=str(path.relative_to(campaign)), sha256=protocol.sha256(path),
                 bytes=path.stat().st_size) if path.is_file() else dict(missing=True))
        return result

    protocol.measured = observe


def main():
    if not os.environ.get('SLURM_JOB_ID'):
        raise RuntimeError('real solver execution requires SLURM')
    campaign = Path(sys.argv[1]).resolve()
    spec = importlib.util.spec_from_file_location('runtime_protocol', campaign / 'pins/runtime_task.py')
    protocol = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(protocol)
    install_observer(protocol, campaign, sys.argv[2], campaign / 'pins/tactix')
    return protocol.main()


if __name__ == '__main__':
    raise SystemExit(main())
