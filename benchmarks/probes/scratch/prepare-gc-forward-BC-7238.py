#!/usr/bin/env python3
"""Unchanged B28+C5 gate for the independently based fused sparse collector."""
from pathlib import Path
base = Path(__file__).with_name('prepare-factor-arena-BC-7183.py')
code = base.read_text()
for before,after in {
    '20260910-7183-factor-arena-BC':'20260911-7238-gc-forward-BC',
    '7183-factor-arena-BC':'7238-gc-forward-BC',
    'tactix-factor-arena-BC-v1':'tactix-gc-forward-BC-v1',
    "('7181-factor-arena-proofs', '7182-factor-arena-sanitizers')":
        "('7235-gc-forward-proofs', '7234-gc-forward-sanitizers', '7237-gc-forward-root-lock')",
    "'benchmarks/runs/20260910-' + stem + '/summary.json'":"'benchmarks/runs/20260911-' + stem + '/summary.json'",
    "stem.startswith('7181')":"stem.startswith('7235')",
    "g['checked'] == 288":"g['checked_UNSAT'] == 216 and g['validated_SAT_models'] == 216",
    '20260910-7180-factor-arena-build':'20260911-7233-gc-forward-build',
    '7180-factor-arena':'7233-gc-forward',
    "('release', 116), ('bc', 116), ('trace', 115)":"('release', 117), ('bc', 117), ('trace', 116)",
    "'benchmarks/runs/20260910-' + stem + '-build/manifest.json'":"'benchmarks/runs/' + ('20260911-' if arm.startswith('candidate') else '20260910-') + stem + '-build/manifest.json'",
    'No policy/semantic drift accepted for storage change.':'No policy/semantic drift accepted for fused sparse watch filtering/forwarding.',
    'After all66 exact solver outcomes, three paired31/35/38 runtime waves plus complete Factor profiles. Judge full Factor cycles, whole runtime and RSS; memory growth from arena holes is an explicit risk. Keep telemetry experiment independent; retained7144 is the control.':
        'After all66 exact solver outcomes, three paired31/35/38 runtime waves plus complete Reduce/sparse/GC profiles. Judge whole runtime and RSS, preserving all watches, aliases, protected reasons, proof bytes, ticks and schedules. Retained7144 control; independent of root-binary, Factor arena and AND fusion. No proof wait for provisional continuation; no final acceptance before checks, memory and current-source fuzz gates.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
