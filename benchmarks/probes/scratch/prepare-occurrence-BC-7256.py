#!/usr/bin/env python3
"""Unchanged B28+C5/runner with a predeclared physical-counter comparison."""
from pathlib import Path
base = Path(__file__).with_name('prepare-factor-arena-BC-7183.py')
code = base.read_text()
for before, after in {
    '20260910-7183-factor-arena-BC':'20260911-7256-occurrence-BC',
    '7183-factor-arena-BC':'7256-occurrence-BC',
    'tactix-factor-arena-BC-v1':'tactix-occurrence-cutover-BC-v1',
    "('7181-factor-arena-proofs', '7182-factor-arena-sanitizers')":
        "('7255-occurrence-proofs', '7254-occurrence-cutover-sanitizers')",
    "'benchmarks/runs/20260910-' + stem + '/summary.json'":"'benchmarks/runs/20260911-' + stem + '/summary.json'",
    "stem.startswith('7181')":"stem.startswith('7255')",
    "g['checked'] == 288":"g['checked_UNSAT'] == 156 and g['validated_SAT_models'] == 156",
    '20260910-7180-factor-arena-build':'20260911-7253-occurrence-cutover-build',
    '7180-factor-arena':'7253-occurrence-cutover',
    "('release', 116), ('bc', 116), ('trace', 115)":"('release', 117), ('bc', 117), ('trace', 116)",
    "'benchmarks/runs/20260910-' + stem + '-build/manifest.json'":"'benchmarks/runs/' + ('20260911-' if arm.startswith('candidate') else '20260910-') + stem + '-build/manifest.json'",
    'Exact complete non-timing statistics and certificate bytes/hash across arms.':
        'Exact complete non-timing statistics and certificate bytes/hash across arms except the strictly parsed two c denseocc physical move/defrag counters. Preserve raw counters and require exact same-arm repetitions. These fields have no policy consumer; owner removal changes their meaning. No other diagnostic or work exclusion.',
    'After all66 exact solver outcomes, three paired31/35/38 runtime waves plus complete Factor profiles. Judge full Factor cycles, whole runtime and RSS; memory growth from arena holes is an explicit risk. Keep telemetry experiment independent; retained7144 is the control.':
        'After all66 exact solver outcomes, three paired31/35/38 runtime waves and complete Eliminate profiles/RSS. Filtered binary scanning is the main risk. Retained7144 control. Proofs continue asynchronously; no promotion before independent checking and remaining gates.',
    'bind(Path(__file__))':
        "bind(Path(__file__))\nbind(Path(__file__).with_name('occurrence-fingerprint-7256.py'))\nbind(Path(__file__).with_name('check-submit-occurrence-BC-7256.py'))",
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
