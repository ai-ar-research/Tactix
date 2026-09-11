#!/usr/bin/env python3
"""Preserve failed7199 cleanup assumption; recognize both proof serializers."""
from pathlib import Path
base = Path(__file__).with_name('check-congruence-sanitizer-control-7199.py')
code = base.read_text()
changes = {
    '20260911-7199-congruence-sanitizer-control': '20260911-7200-congruence-sanitizer-control',
    "assert data.startswith(b'd ') and data.endswith(b' 0\\n'), residue":
        "assert data == b'3 d 1 0\\n3 d 2 0\\n', (residue, data)",
    "bind(Path(__file__)); bind(parent_object); bind(c/'minimal')":
        """# These exact leftovers were created by the two terminal failed
# controls, not an active checker. Their full bytes identify the first LRAT
# growth case. Keep hashes/receipts; the fixed fixtures regenerate them.
for name in ('tactix-congruence-index-auXUBS', 'tactix-congruence-index-RE4lqV'):
    residue = Path('/tmp') / name
    assert residue.read_bytes() == b'3 d 1 0\\n3 d 2 0\\n'
    removed.append(dict(path=str(residue), sha256=sha(residue), bytes=16, producer='terminal7197/7199'))
    residue.unlink()
bind(root/'benchmarks/probes/scratch/check-congruence-sanitizer-control-7199.py')
for p in (root/'benchmarks/runs/20260911-7199-congruence-sanitizer-control').glob('*.json'): bind(p)
bind(Path(__file__)); bind(parent_object); bind(c/'minimal')""",
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
