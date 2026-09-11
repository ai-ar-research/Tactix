#!/usr/bin/env python3
"""External /proc RSS observations alongside immutable strace solver runs.

Samples at the existing runtime harness's 100ms cadence. VmHWM is a persistent
high-water mark, not a timestamped allocation event. At a rise retain raw
smaps plus status before/after so races cannot be mistaken for atomic state.
"""
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import threading
import time


FIELDS = ('VmHWM', 'VmRSS', 'RssAnon', 'RssFile', 'RssShmem', 'VmData', 'VmSize')


def status_values(text):
    values = {}
    for line in text.splitlines():
        key, sep, rest = line.partition(':')
        if sep and key in FIELDS:
            match = re.fullmatch(r'\s*(\d+) kB', rest)
            if key in values or not match:
                raise ValueError('invalid status field: ' + line)
            values[key] = int(match[1])
    if set(values) != set(FIELDS):
        raise ValueError('incomplete live-process status')
    return values


def process_start(stat):
    # comm can contain spaces and parentheses; fields after final ')' start
    # with state (field3). starttime is field22, i.e. index19 in that suffix.
    suffix = stat.rsplit(')', 1)[1].split()
    return int(suffix[19])


def sample_process(pid, binary, proc=Path('/proc'), expected_start=None, maps=False):
    base = proc / str(pid)
    start = process_start((base / 'stat').read_text())
    if expected_start is not None and start != expected_start:
        raise ValueError('PID identity changed')
    if (base / 'exe').resolve(strict=True) != Path(binary).resolve(strict=True):
        raise ValueError('PID is not the selected solver executable')
    before = time.time_ns()
    raw = (base / 'status').read_text()
    result = dict(pid=pid, starttime_ticks=start, before_ns=before,
                  after_ns=time.time_ns(), status=raw, kib=status_values(raw))
    if maps:
        result['smaps_before_ns'] = time.time_ns()
        result['smaps'] = (base / 'smaps').read_text()
        result['smaps_after_ns'] = time.time_ns()
        result['status_after_smaps'] = (base / 'status').read_text()
        result['kib_after_smaps'] = status_values(result['status_after_smaps'])
    if process_start((base / 'stat').read_text()) != start:
        raise ValueError('PID identity changed during sample')
    return result


def monitor(trace, binary, output, stop):
    identity, high = None, -1
    pid = None
    with output.open('x') as stream:
        def emit(record):
            stream.write(json.dumps(record) + '\n')
            stream.flush()
        while not stop.is_set():
            try:
                if pid is None:
                    with trace.open() as source:
                        first = source.readline()
                    match = re.match(r'^(\d+) \d+\.\d+ brk\(NULL\)', first)
                    if not match:
                        raise ValueError('await initial solver brk record')
                    pid = int(match[1])
                row = sample_process(pid, binary, expected_start=identity)
                identity = row['starttime_ticks']
                if row['kib']['VmHWM'] > high:
                    high = row['kib']['VmHWM']
                    # Retain both the triggering sample and later non-atomic
                    # map read, never label smaps as the historical HWM state.
                    try:
                        row['high_water_snapshot'] = sample_process(pid, binary,
                            expected_start=identity, maps=True)
                    except (OSError, ValueError, IndexError) as error:
                        row['snapshot_error'] = repr(error)
                emit(dict(kind='sample', **row))
            except (OSError, ValueError, IndexError) as error:
                emit(dict(kind='gap', observed_ns=time.time_ns(), pid=pid,
                          starttime_ticks=identity, error=repr(error)))
            stop.wait(0.1)
        emit(dict(kind='observer_stop', observed_ns=time.time_ns(), pid=pid,
                  starttime_ticks=identity, maximum_sampled_hwm_kib=high if high >= 0 else None))


def install_proc_observer(protocol, campaign, task, binary):
    task = int(task)
    if task < 0:
        raise ValueError('negative task')
    campaign = Path(campaign).resolve()
    binary = str(Path(binary).resolve())
    output = campaign / 'logs' / f'{task:03d}.proc-memory.jsonl'
    trace = campaign / 'logs' / f'{task:03d}.memory-syscalls.txt'
    original = protocol.measured

    def observe(command, *args):
        matches = list(map(str, command)).count(binary)
        if not matches:
            return original(command, *args)
        if matches != 1:
            raise ValueError('ambiguous solver command')
        if output.exists():
            raise FileExistsError(output)
        stop, errors = threading.Event(), []
        def worker():
            try:
                monitor(trace, binary, output, stop)
            except Exception as error:
                errors.append(repr(error))
        observer = threading.Thread(target=worker, daemon=True)
        observer.start()
        try:
            result = original(command, *args)
        finally:
            stop.set()
            observer.join()
        result['proc_memory'] = (dict(path=str(output.relative_to(campaign)),
            sha256=protocol.sha256(output), bytes=output.stat().st_size, errors=errors)
            if output.is_file() else dict(missing=True, errors=errors))
        return result
    protocol.measured = observe


def main():
    if not os.environ.get('SLURM_JOB_ID'):
        raise RuntimeError('real solver execution requires SLURM')
    campaign = Path(sys.argv[1]).resolve()
    def load(name, path):
        spec = importlib.util.spec_from_file_location(name, path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    protocol = load('runtime_protocol', campaign / 'pins/runtime_task.py')
    external = load('external_syscalls', campaign / 'pins/syscall_memory_task.py')
    for arm in ('candidate', 'previous'):
        binary = campaign / 'pins' / arm
        install_proc_observer(protocol, campaign, sys.argv[2], binary)
        external.install_observer(protocol, campaign, sys.argv[2], binary, campaign / 'pins/strace')
    return protocol.main()


if __name__ == '__main__':
    raise SystemExit(main())
