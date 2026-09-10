#!/usr/bin/env python3
"""Map saved self samples to exact instructions; no solver or profile expansion."""
from collections import Counter
import json
import lzma
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_callstack import samples, phase
target = root / 'benchmarks/runs/20260910-7137-vivify-schedule-localization'
assert not target.exists()
hashes = {}
def bind(p, expected=None):
    value = sha(p)
    assert expected is None or value == expected, p
    hashes[str(p.relative_to(root))] = value
def read(p):
    bind(p)
    return json.loads(p.read_text())

manifest_path = root / 'benchmarks/runs/20260910-7128-bve-lifecycle-profiles/manifest.json'
manifest = read(manifest_path)
native_path = root / 'benchmarks/runs/20260910-7081-rank35-callstack-cost/lifecycle-summary-7082.json'
native = read(native_path)
assessment = read(root / 'benchmarks/reports/2026-09-10-bve-prototype-assessment-7135.json')
assert hashes[str(native_path.relative_to(root))] == assessment['hashes'][str(native_path.relative_to(root))]
schedule_symbol = 'tactix::vivify_schedule(tactix::Kernel const&, std::vector<unsigned int, std::allocator<unsigned int> >&, tactix::VivifySchedule*)'
specs = []
for wave in range(3):
    directory = root / f'benchmarks/runs/20260910-7132-bve-lifecycle-samples/wave{wave}'
    summary = read(directory / 'summary.json')
    specs.append(dict(label=f'candidate-wave{wave}', binary=manifest_path.parent / 'pins/candidate',
        stream=directory / 'candidate.samples.txt.xz', expected=summary['arms']['candidate'],
        stream_hash=summary['hashes'][str((directory / 'candidate.samples.txt.xz').relative_to(root))],
        binary_hash=manifest['arms']['candidate']['sha256'],
        entries={manifest['phase_entry']: None}, caller=manifest['phase_caller'],
        sites={int(k): v for k, v in manifest['call_sites']['candidate']['sites'].items()},
        selected=lambda s: s == schedule_symbol or
            s.startswith('void std::__introsort_loop<__gnu_cxx::__normal_iterator<tactix::VivifyCand*') or
            s.startswith(schedule_symbol + '::{lambda(unsigned int, bool)')))
native_binary = root / 'benchmarks/runs/20260910-7002-largest-gap-release-profiles/pins/kissat'
native_stream = native_path.parent / 'kissat.samples.txt.xz'
specs.append(dict(label='native', binary=native_binary, stream=native_stream,
    expected=native['arms']['kissat'], stream_hash=native['hashes'][str(native_stream.relative_to(root))],
    binary_hash=native['hashes'][str(native_binary.relative_to(root))],
    entries={'kissat_probe_initially': 'initial_probe', 'kissat_probe': 'probe', 'kissat_eliminate': 'eliminate'},
    caller=None, sites=None, selected=lambda s: s == 'vivify_round' or s.startswith('vivify_sort_lits_by_counts')))

target.mkdir()
out, inventories = {}, {}
for spec in specs:
    binary = spec['binary']; label = spec['label']
    bind(binary, spec['binary_hash']); bind(spec['stream'], spec['stream_hash'])
    if str(binary) not in inventories:
        text = subprocess.check_output(['nm', '-S', '--defined-only', '--demangle', str(binary)], text=True)
        symbols = {}
        for line in text.splitlines():
            match = re.fullmatch(r'([0-9a-f]+) ([0-9a-f]+) [tT] (.+)', line)
            if not match or not spec['selected'](match[3]): continue
            address, size = int(match[1], 16), int(match[2], 16)
            command = ['objdump', '-d', '-C', '--no-show-raw-insn', f'--start-address={address}', f'--stop-address={address + size}', str(binary)]
            assembly = subprocess.check_output(command, text=True)
            rows = []
            for row in assembly.splitlines():
                instruction = re.fullmatch(r'\s*([0-9a-f]+):\s+(\S.*)', row)
                if instruction:
                    pc = int(instruction[1], 16)
                    assert address <= pc < address + size
                    rows.append((pc, instruction[2]))
            assert rows and rows[0][0] == address and all(a[0] < b[0] for a, b in zip(rows, rows[1:]))
            p = target / f'{label}-symbol{len(symbols)}.asm'
            with p.open('x') as stream: stream.write(assembly)
            bind(p)
            symbols[match[3]] = dict(address=address, size=size, command=command, rows=rows, assembly=str(p.relative_to(root)))
        assert symbols
        inventories[str(binary)] = symbols
    symbols = inventories[str(binary)]
    periods = {name: Counter() for name in symbols}
    total = count = probe = 0
    with lzma.open(spec['stream'], 'rt') as stream:
        for sample in samples(stream):
            total += sample.period; count += 1
            name, index = phase(sample, str(binary), spec['entries'], spec['caller'], spec['sites'])
            assert name != 'unresolved_phase_caller'
            if name != 'probe': continue
            probe += sample.period
            leaf = sample.frames[0]
            if leaf.dso == str(binary) and leaf.symbol in symbols:
                periods[leaf.symbol][leaf.pc] += sample.period
    expected = spec['expected']
    assert (count, total, probe) == (expected['samples'], expected['total_period'], expected['phase_periods']['probe'])
    selected = {}
    for name, symbol in symbols.items():
        counts = periods[name]
        assert set(counts) <= {pc for pc, asm in symbol['rows']}, (name, 'sample not on exact instruction')
        wanted = expected['self_by_phase']['probe'].get(name + ' [' + binary.name + ']', 0)
        assert sum(counts.values()) == wanted, name
        rows = [dict(pc=pc, offset=pc - symbol['address'], period=counts[pc], instruction=asm) for pc, asm in symbol['rows']]
        selected[name] = dict(address=symbol['address'], size=symbol['size'], assembly=symbol['assembly'],
            self_period=wanted, instruction_rows=rows,
            hottest=sorted(rows, key=lambda row: -row['period'])[:25])
    out[label] = dict(samples=count, total_period=total, probe_period=probe, symbols=selected)
    print(label, 'exact selected self periods', {name[:95]: item['self_period'] for name, item in selected.items()}, flush=True)
for p in (Path(__file__), root / 'tools/bench/perf_callstack.py', root / 'tools/bench/test_perf_callstack.py'): bind(p)
with (target / 'summary.json').open('x') as stream:
    json.dump(dict(hashes=hashes, arms=out,
        limitation='Saved199Hz cycles:u samples, exact self instruction PCs, not exact instruction latency or removable cost. Candidate repeats share one binary; native is one frozen cross-window observation. No caller return PC is priced as a self instruction. No new solver run, source change, raw-profile expansion or deletion. Pending proofs remain unverified.'), stream, indent=2)
