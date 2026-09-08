#!/usr/bin/env python3
"""Stage explicit audited evidence, excluding large raw traces and fuzz artifacts."""
from pathlib import Path
source=Path(__file__).with_name('stage-propagation-checkpoint-5667.py').read_text()
source=source.replace('2026-09-08-propagation-cursor-validation.json','2026-09-08-score-increment-validation.json')
start,end=source.index('paths = {'),source.index('\ndef include')
source=source[:start]+'''paths = {'PROGRESS.md',str(audit.relative_to(root)),
 'benchmarks/reports/2026-09-08-score-increment-localization.md',
 'benchmarks/reports/2026-09-08-score-increment-main-ctest.log',
 'benchmarks/probes/scratch/claude-periodic-fuzz-5715.txt'}'''+source[end:]
source=source.replace('range(5641, 5668)','range(5694, 5718)')
source=source.replace("if '-5649-' in c.name:","if '-5672-' in c.name:")
source=source.replace("m['review_evidence']","m['predecessor_fixture']")
source=source.replace("('.prefix.tsv', '.boundaries.tsv', '.macro.tsv')",
    "('.prefix.tsv', '.boundaries.tsv', '.macro.tsv', '.state.tsv', '.scores.tsv', '.bumps.tsv')")
anchor="    for name, digest in m.get('pin_sha256', m.get('hashes', {})).items():"
source=source.replace(anchor,"""    if '-5696-' in c.name:
        for name,digest in m['logs'].items():include(c/'logs'/name,digest)
        for arm,entry in m['sources'].items():include(c/(arm+'.patch'),entry['patch_sha256'])
        include(c/'cumulative.patch',m['cumulative_patch_sha256'])
"""+anchor)
exec(compile(source,__file__,'exec'))
