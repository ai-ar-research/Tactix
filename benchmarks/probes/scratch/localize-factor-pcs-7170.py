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
from perf_self_pc import bind_self_pc
target = root / 'benchmarks/runs/20260910-7170-factor-pc-localization'
assert not target.exists()
hashes = {}
def bind(p, expected=None):
    value = sha(p)
    assert expected is None or value == expected, p
    hashes[str(p.relative_to(root))] = value
def read(p):
    bind(p)
    return json.loads(p.read_text())

manifest_path = root / 'benchmarks/runs/20260910-7152-vivify-compact-profiles/manifest.json'
manifest = read(manifest_path)
native_path = root / 'benchmarks/runs/20260910-7081-rank35-callstack-cost/lifecycle-summary-7082.json'
native = read(native_path)
assessment = read(root / 'benchmarks/reports/2026-09-10-bve-prototype-assessment-7135.json')
assert hashes[str(native_path.relative_to(root))] == assessment['hashes'][str(native_path.relative_to(root))]
schedule_symbol = 'tactix::factor_apply_seeded(tactix::Kernel&, unsigned int)'
specs = []
for wave in range(3):
    directory = root / f'benchmarks/runs/20260910-7153-vivify-compact-samples/wave{wave}'
    summary = read(directory / 'summary.json')
    specs.append(dict(label=f'candidate-wave{wave}', binary=manifest_path.parent / 'pins/candidate',
        stream=directory / 'candidate.samples.txt.xz', expected=summary['arms']['candidate'],
        stream_hash=summary['hashes'][str((directory / 'candidate.samples.txt.xz').relative_to(root))],
        binary_hash=manifest['arms']['candidate']['sha256'],
        entries={manifest['phase_entry']: None}, caller=manifest['phase_caller'],
        sites={int(k): v for k, v in manifest['call_sites']['candidate']['sites'].items()},
        selected=lambda s: schedule_symbol in s))
native_binary = root / 'benchmarks/runs/20260910-7002-largest-gap-release-profiles/pins/kissat'
native_stream = native_path.parent / 'kissat.samples.txt.xz'
specs.append(dict(label='native', binary=native_binary, stream=native_stream,
    expected=native['arms']['kissat'], stream_hash=native['hashes'][str(native_stream.relative_to(root))],
    binary_hash=native['hashes'][str(native_binary.relative_to(root))],
    entries={'kissat_probe_initially': 'initial_probe', 'kissat_probe': 'probe', 'kissat_eliminate': 'eliminate'},
    caller=None, sites=None, selected=lambda s: s == 'kissat_factor'))

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
            symbols[match[3]] = dict(address=address, size=size, command=command, rows=rows, instruction_addresses={pc for pc, _ in rows}, assembly=str(p.relative_to(root)))
        assert symbols
        inventories[str(binary)] = symbols
    symbols = inventories[str(binary)]
    periods = {name: Counter() for name in symbols}
    aliases = {name: Counter() for name in symbols}
    total = count = probe = 0
    with lzma.open(spec['stream'], 'rt') as stream:
        for sample in samples(stream):
            total += sample.period; count += 1
            name, index = phase(sample, str(binary), spec['entries'], spec['caller'], spec['sites'])
            assert name != 'unresolved_phase_caller'
            if name != 'probe': continue
            probe += sample.period
            leaf = sample.frames[0]
            if leaf.dso == str(binary):
                identity = bind_self_pc(leaf.pc, leaf.symbol, symbols, spec['selected'])
                if identity is not None:
                    periods[identity][leaf.pc] += sample.period
                    aliases[identity][leaf.symbol] += sample.period
    expected = spec['expected']
    assert (count, total, probe) == (expected['samples'], expected['total_period'], expected['phase_periods']['probe'])
    # Every selected perf spelling must be accounted for, including demangler
    # aliases absent from nm. Per-symbol equality alone missed those in7137.
    expected_selected = {key[:-(len(binary.name) + 3)]: value
        for key, value in expected['self_by_phase']['probe'].items()
        if key.endswith(' [' + binary.name + ']') and spec['selected'](key[:-(len(binary.name) + 3)])}
    observed_selected = Counter()
    for values in aliases.values(): observed_selected.update(values)
    assert dict(observed_selected) == expected_selected, 'incomplete selected-symbol universe'
    selected = {}
    for name, symbol in symbols.items():
        counts = periods[name]
        assert set(counts) <= {pc for pc, asm in symbol['rows']}, (name, 'sample not on exact instruction')
        wanted = sum(aliases[name].values())
        assert sum(counts.values()) == wanted, name
        rows = [dict(pc=pc, offset=pc - symbol['address'], period=counts[pc], instruction=asm) for pc, asm in symbol['rows']]
        selected[name] = dict(address=symbol['address'], size=symbol['size'], assembly=symbol['assembly'],
            self_period=wanted, perf_symbol_aliases=dict(aliases[name]), instruction_rows=rows,
            hottest=sorted(rows, key=lambda row: -row['period'])[:25])
    out[label] = dict(samples=count, total_period=total, probe_period=probe, symbols=selected)
    print(label, 'complete selected self periods', sum(item['self_period'] for item in selected.values()), flush=True)
for p in (Path(__file__), root / 'tools/bench/perf_callstack.py', root / 'tools/bench/test_perf_callstack.py', root / 'tools/bench/perf_self_pc.py', root / 'tools/bench/test_perf_self_pc.py'): bind(p)
with (target / 'summary.json').open('x') as stream:
    json.dump(dict(hashes=hashes, arms=out,
        scope='All three retained7144 candidate streams and one frozen native7082 stream. Complete selected self-symbol universe is checked against immutable profiles. Localize the large inlined Factor loop before selecting a structural design; no solver edit or integration commit.',
        limitation='Saved199Hz cycles:u samples, exact self instruction PCs, not exact instruction latency or removable cost. Candidate repeats share one binary; native is one frozen cross-window observation. No caller return PC is priced as a self instruction. No new solver run, source change, raw-profile expansion or deletion. Pending proofs remain unverified.'), stream, indent=2)
