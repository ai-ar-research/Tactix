#!/usr/bin/env python3
"""Attribute Factor using the already checked profile, without solver work."""
from pathlib import Path

source = Path(__file__).with_name('annotate-retention-6530.py')
code = source.read_text()
for old, new in {
    '20260909-6530-rank32-retention-annotation': '20260909-6544-rank32-factor-annotation',
    "('src/kernel/kernel.cpp', 'src/kernel/watch.hpp')": "('src/rules/factor/factor.cpp',)",
    "enumerate(('candidate', 'kissat'))": "enumerate(('candidate',))",
    'source_kernel_watch_exact_current': 'source_factor_exact_current',
    'PASS: eight annotations, exact inputs, owned temporary expansions deleted':
        'PASS: four annotations, detailed call graph, exact inputs, temporary expansion deleted',
}.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
begin, end = code.index('symbols = {'), code.index('\noutputs = {}')
code = code[:begin] + '''symbols = {'candidate': {
    'apply': 'tactix::factor_apply_seeded(tactix::Kernel&, unsigned int)',
    'index': 'tactix::(anonymous namespace)::build_factor_index(tactix::Kernel const&, tactix::(anonymous namespace)::FactorIndex&, unsigned int)',
}}''' + code[end:]
anchor = '    assert not Path(directory).exists()'
assert code.count(anchor) == 1
extra = '''        command = ['perf', 'report', '--stdio', '--stdio-color', 'never',
                   '-i', str(data), '--children', '--percent-limit', '0',
                   '--call-graph', 'graph,0.05,caller']
        run = subprocess.run(command, cwd=root, capture_output=True, text=True)
        (target / 'callpaths.txt').write_text(run.stdout)
        (target / 'callpaths.stderr').write_text(run.stderr)
        assert run.returncode == 0 and run.stdout.strip(), run.stderr
        outputs['callpaths'] = dict(command=command, exit=run.returncode,
            stdout_sha256=sha(target / 'callpaths.txt'),
            stderr_sha256=sha(target / 'callpaths.stderr'))
'''
code = code.replace(anchor, extra + anchor)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
