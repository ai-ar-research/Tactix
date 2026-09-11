#!/usr/bin/env python3
"""Full B28+C5 gate for root-binary7218 after repaired fixed proof witnesses."""
from pathlib import Path
base = Path(__file__).with_name('prepare-factor-arena-BC-7183.py')
code = base.read_text()
for before,after in {
    '20260910-7183-factor-arena-BC':'20260911-7221-root-binary-BC',
    '7183-factor-arena-BC':'7221-root-binary-BC',
    "('7181-factor-arena-proofs', '7182-factor-arena-sanitizers')":"('7224-root-binary-proof-control', '7220-root-binary-sanitizers')",
    "'benchmarks/runs/20260910-' + stem + '/summary.json'":"'benchmarks/runs/20260911-' + stem + '/summary.json'",
    "stem.startswith('7181')":"stem.startswith('7224')",
    "g['checked'] == 288":"g['checked_UNSAT'] == 216 and g['validated_SAT_models'] == 216",
    '20260910-7180-factor-arena-build':'20260911-7218-root-binary-build',
    '7180-factor-arena':'7218-root-binary',
    "'benchmarks/runs/20260910-' + stem + '-build/manifest.json'":"'benchmarks/runs/' + ('20260911-' if arm.startswith('candidate') else '20260910-') + stem + '-build/manifest.json'",
    'No policy/semantic drift accepted for storage change.':'No policy/semantic drift accepted for root-satisfied binary discovery.',
    'After all66 exact solver outcomes, three paired31/35/38 runtime waves plus complete Factor profiles. Judge full Factor cycles, whole runtime and RSS; memory growth from arena holes is an explicit risk. Keep telemetry experiment independent; retained7144 is the control.':
        'After all66 exact solver outcomes, three paired31/35/38 runtime waves plus complete Reduce and nested sparse-collection profiles. Judge whole runtime and RSS; preserve all deletion orientation, multiplicity, advice, proof bytes, ticks and schedules. Retained7144 control; independent of Factor arena and AND fusion.7219/7222 are fixture failures from missing inverse import permutation;7224 fixes only fixture setup and verifies both arms.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
