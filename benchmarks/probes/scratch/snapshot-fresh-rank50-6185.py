#!/usr/bin/env python3
"""Retain all fifty ranks as the fresh reference checking reaches completion."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-fresh-rank50-6123.py')
code = source.read_text()
changes = {
    '2026-09-08-fresh-rank50-6123': '2026-09-09-fresh-rank50-6185',
    "cmd=['sacct','-n','-X','--array','-j',job,'-o','JobID,State,ExitCode','-P']":
    "cmd=['sacct','-n','-X','--array','-j',job,'-S','2026-09-08T22:30:00','-o','JobID,State%40,ExitCode,Submit','-P']",
    'Nine new classification tests plus24 existing benchmark tests. Initially absent SLURM state caused an IndexError;fixed before use and covered by no-state tests. Reject empty/partial matrices,oracle/identity/cleanup errors;retain clean timeout penalties and phase-qualified resource failures.':
    '39 benchmark tests and13 timestamp-aware watcher tests. Same fail-closed classifier and full50 membership; bounded accounting excludes earlier recycled IDs. Pending checks suppress aggregate; no new checker protocol.'
}
for old, new in changes.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
