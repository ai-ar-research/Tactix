#!/usr/bin/env python3
"""Validate both exact own-work SAT outcomes before reading completed traces."""
import importlib.util
from pathlib import Path
base = Path(__file__).with_name('validate-memory-syscalls-7295.py')
code = base.read_text()
changes = {
    '20260911-7295-memory-syscalls-rank31': '20260911-7315-rank25-native-memory',
    "'occurrence-fingerprint-7256.py'": "'rank25-memory-contract-7315.py'",
    "r['rank'] == 31": "r['rank'] == 25",
    "('pending_validation', 'checked_UNSAT')": "('pending_validation', 'checked_SAT')",
    'h.raw_fingerprint(r) == h.raw_fingerprint(records[task])':
        "h.fingerprint(r, m['arms'][r['arm']]['solver']) == h.fingerprint(records[task], m['arms'][r['arm']]['solver'])",
    'h.raw_fingerprint(r) == h.raw_fingerprint(json.loads(q.read_text()))':
        "h.fingerprint(r, oracle['solver']) == h.fingerprint(json.loads(q.read_text()), oracle['solver'])",
    "        records[task], states[task] = r, state":
        """        proc = r['solve']['proc_memory']
        assert not proc['errors']
        q = c / f'logs/{task:03d}.proc-memory.jsonl'
        assert proc['path'] == str(q.relative_to(c)) and proc['bytes'] == q.stat().st_size
        bind(q, proc['sha256'])
        samples = [json.loads(line) for line in q.read_text().splitlines()]
        assert samples[-1]['kind'] == 'observer_stop'
        live = [s for s in samples if s['kind'] == 'sample']
        assert live and len({(s['pid'], s['starttime_ticks']) for s in live}) == 1
        assert any('smaps' in s.get('high_water_snapshot', {}) for s in live)
        print(r['arm'], 'solver HWM', max(s['kib']['VmHWM'] for s in live),
              'samples', len(live), 'gaps', sum(s['kind'] == 'gap' for s in samples))
        records[task], states[task] = r, state""",
    'External Rank31:': 'External Rank25:',
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
