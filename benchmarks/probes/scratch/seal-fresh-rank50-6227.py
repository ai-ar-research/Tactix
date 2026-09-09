#!/usr/bin/env python3
"""Seal the complete checked comparison; preserve interim-labelled snapshots."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-fresh-rank50-6123.py')
code = source.read_text()
changes = {
    '2026-09-08-fresh-rank50-6123': '2026-09-09-fresh-rank50-final-6227',
    "cmd=['sacct','-n','-X','--array','-j',job,'-o','JobID,State,ExitCode','-P']":
    "cmd=['sacct','-n','-X','--array','-j',job,'-S','2026-09-08T22:37:05','-o','JobID,State%40,ExitCode,Submit','-P']",
    'summary=cc.load(c,states)':
    "summary=cc.load(c,states)\nassert summary['final_records'] == 100 and summary['zero_residue']\nassert summary['ratio'] is not None\nassert summary['classes'] == {a: {'checked_SAT':25,'checked_UNSAT':25} for a in ('tactix','kissat')}\nassert all(s == 'COMPLETED' for s in states.values())",
    'interim;not a final accepted reevaluation': 'final;all100 certificates checked and cleanup verified;production unchanged',
    '# Fresh accepted-binary Rank1–50 comparison (interim)': '# Fresh accepted-binary Rank1–50 comparison (final)',
    'Every rank is retained. Blank timings mean no solver record at this snapshot. Ratios require both certificates checked and cleanup verified; no partial aggregate is reported.':
    'All100 certificates checked;50 solves per arm (25 SAT,25 UNSAT),zero residue,no solve losses. One paired wave,not a causal comparison with separately measured candidates. Resources:12 logical CPUs,48000MiB,1200s solver-only,16000s checker. Aggregate and exact provenance are in summary.json.',
}
for old, new in changes.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
