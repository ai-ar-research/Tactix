#!/usr/bin/env python3
"""Three independent Rank50 pairs; never substitute a single favorable run."""
from pathlib import Path
source=Path(__file__).with_name('summarize-collapse-waves-6343.py')
code=source.read_text()
changes={
 '20260909-6332-collapse-three-wave-runtime':'20260909-6592-rank50-material-three-wave-runtime',
 'solver-checkpoint-6343.json':'solver-checkpoint-6597.json',
 'range(18)':'range(6)',
 'range(6*wave, 6*wave+6)':'range(2*wave, 2*wave+2)',
 'matrix[6*wave:6*wave+6]':'matrix[2*wave:2*wave+2]',
 '(32, 33, 40)':'(50,)',
 "('pending_validation', 'checked_UNSAT')":"('pending_validation', 'checked_SAT')",
 't // 6 == wave':'t // 2 == wave',
 'solver_outcomes=18':'solver_outcomes=6',
 'Changed-work candidate6305 versus6269':'Changed-work material6579 versus6424 on Rank50',
 "binding = m['work_oracles'][r['arm']][str(r['rank'])]\n        assert sha(root / binding['path']) == binding['sha256']\n        oracle = json.loads((root / binding['path']).read_text())":
 "if r['arm'] == 'previous':\n            binding = m['previous_oracle']\n            oracle_path = root / binding['path']\n            assert sha(oracle_path) == binding['sha256']\n        else:\n            oracle_path = c / 'results/000.json'\n            if not oracle_path.exists(): oracle_path = c / 'provisional/000.json'\n        oracle = json.loads(oracle_path.read_text())\n        bindings[str(oracle_path.relative_to(root))] = sha(oracle_path)",
 "for rel, digest in m['protected_campaign_hashes'].items():":
 "prior_path = root / 'benchmarks/reports/2026-09-09-material-schedule-audit-6590.json'\nassert sha(prior_path) == '173ea1086462ab317e7937fb476f2b990c79ffdd90eba74ae09821cd9fc624a4'\nbindings[str(prior_path.relative_to(root))] = sha(prior_path)\nprior = json.loads(prior_path.read_text())\nfor rel, digest in prior['protected_campaign_hashes'].items():",
}
for old,new in changes.items():
    assert old in code,old
    code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
