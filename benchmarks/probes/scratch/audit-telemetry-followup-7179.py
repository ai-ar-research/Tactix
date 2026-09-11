#!/usr/bin/env python3
"""Reuse frozen checkpoint checks for wave1 and the new Factor localization."""
from pathlib import Path
base = Path(__file__).with_name('audit-runtime-telemetry-7177.py')
code = base.read_text()
changes = {
    "target = root / 'benchmarks/reports/2026-09-10-runtime-telemetry-checkpoint-7177.json'":
        "target = root / 'benchmarks/reports/2026-09-10-telemetry-followup-audit-7179.json'",
    "old_path = root / 'benchmarks/reports/2026-09-10-vivify-revalidation-audit-7169.json'":
        "old_path = root / 'benchmarks/reports/2026-09-10-runtime-telemetry-checkpoint-7177.json'",
    "report-to-andrew/20260910-2257-runtime-evaluation":
        "report-to-andrew/20260910-2312-telemetry-followup",
    "commands = [[sys.executable, '-m', 'unittest'":
        "for rel in ('benchmarks/probes/scratch/localize-factor-children-7178.py', 'benchmarks/probes/scratch/audit-telemetry-followup-7179.py', 'benchmarks/reports/2026-09-10-factor-child-sites-7178.json', 'benchmarks/reports/2026-09-10-telemetry-wave0-factor-followup-7179.md'):\n    bind(root / rel, own=True)\ncommands = [[sys.executable, '-m', 'unittest'",
    "checks = {}":
        "commands += [[sys.executable, 'benchmarks/probes/scratch/localize-factor-children-7178.py', '--self-test']]\nchecks = {}",
    "first targeted wave launched without proof wait":
        "first targeted wave all6 exact with mixed provisional timing; second wave1680423 launched without proof wait. Factor7178 localizes dominant push cost to first-seen cofactor tracking, not grid copying, and independently allocated index construction/destruction",
    "Validate7175 wave0 with strict telemetry contract; advance waves1/2 only after exact expected own/cross-arm solver outcomes, never wait for proofs.":
        "Validate7175 wave1 with strict telemetry contract; advance wave2 only after all12 exact expected own/cross-arm solver outcomes, never wait for proofs.",
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
