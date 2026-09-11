#!/usr/bin/env python3
"""Trace allocator syscalls outside an exact solver binary, never its checker."""
import importlib.util
from pathlib import Path
import os
import sys


def install_observer(protocol, campaign, task, binary, tracer):
    task = int(task)
    if task < 0:
        raise ValueError('negative task')
    campaign = Path(campaign).resolve()
    binary = str(Path(binary).resolve())
    tracer = str(Path(tracer).resolve())
    path = campaign / 'logs' / f'{task:03d}.memory-syscalls.txt'
    original = protocol.measured

    def observe(command, stdout, stderr, metrics, timeout):
        words = list(map(str, command))
        matches = [i for i, word in enumerate(words) if word == binary]
        if not matches:
            return original(command, stdout, stderr, metrics, timeout)
        if len(matches) != 1:
            raise ValueError('ambiguous solver command')
        if path.exists():
            raise FileExistsError(path)
        at = matches[0]
        # Insert after taskset/perf options, so only the solver is traced.
        # Do not preload a library or enable any in-process diagnostic flag.
        wrapped = words[:at] + [tracer, '-f', '-qq', '-ttt', '-k',
            '-e', 'trace=brk,mmap,mremap,munmap', '-o', str(path), '--'] + words[at:]
        result = original(wrapped, stdout, stderr, metrics, timeout)
        result['memory_syscalls'] = (
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
    for arm in ('candidate', 'previous'):
        install_observer(protocol, campaign, sys.argv[2], campaign / 'pins' / arm,
                         campaign / 'pins/strace')
    return protocol.main()


if __name__ == '__main__':
    raise SystemExit(main())
