#!/usr/bin/env python3
"""Full B28+C5 gate after exact proofs and classified sanitizer repair."""
from pathlib import Path
base = Path(__file__).with_name('prepare-factor-arena-BC-7183.py')
code = base.read_text()
changes = {
    '20260910-7183-factor-arena-BC': '20260911-7202-congruence-fused-BC',
    '7183-factor-arena-BC': '7202-congruence-fused-BC',
    "('7181-factor-arena-proofs', '7182-factor-arena-sanitizers')": "('7198-congruence-fused-proofs', '7200-congruence-sanitizer-control')",
    "'benchmarks/runs/20260910-' + stem + '/summary.json'": "'benchmarks/runs/20260911-' + stem + '/summary.json'",
    "stem.startswith('7181')": "stem.startswith('7198')",
    '20260910-7180-factor-arena-build': '20260911-7196-congruence-fused-build',
    '7180-factor-arena': '7196-congruence-fused',
    "'benchmarks/runs/20260910-' + stem + '-build/manifest.json'": "'benchmarks/runs/' + ('20260911-' if arm.startswith('candidate') else '20260910-') + stem + '-build/manifest.json'",
    'No policy/semantic drift accepted for storage change.': 'No policy/semantic drift accepted for loop fusion.',
    'After all66 exact solver outcomes, three paired31/35/38 runtime waves plus complete Factor profiles. Judge full Factor cycles, whole runtime and RSS; memory growth from arena holes is an explicit risk. Keep telemetry experiment independent; retained7144 is the control.':
        'After all66 exact solver outcomes, three paired31/35/38 runtime waves plus complete Congruence profiles with current call-site review. Judge whole runtime and RSS as well as complete Congruence cost. No arity/scheduling/telemetry changes; retained7144 is the control. Factor arena remains separate.',
}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
