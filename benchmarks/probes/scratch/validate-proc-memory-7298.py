#!/usr/bin/env python3
"""Reuse strict exact-Release/trace gate and bind completed RSS observations."""
import json
from pathlib import Path
import sys
base = Path(__file__).with_name('validate-memory-syscalls-7295.py')
code = base.read_text()
assert code.count('20260911-7295-memory-syscalls-rank31') == 1
code = code.replace('20260911-7295-memory-syscalls-rank31', '20260911-7298-proc-memory-rank31')
needle = "        records[task], states[task] = r, state"
assert code.count(needle) == 1
code = code.replace(needle, '''        proc = r['solve']['proc_memory']
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
''' + needle)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
