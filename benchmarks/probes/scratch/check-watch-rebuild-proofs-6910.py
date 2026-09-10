#!/usr/bin/env python3
"""Fresh stronger gate after legacy LRAT checker failed a SAT negative control."""
from pathlib import Path
source = Path(__file__).with_name('check-watch-rebuild-proofs-6906.py')
code = source.read_text()
for before,after in {
    '20260910-6906-watch-rebuild-proofs':'20260910-6910-watch-rebuild-proofs',
    'from fixed_runtime import sha':'from fixed_runtime import sha\nfrom proof_oracle import proof_check_status',
    "('lrat-check' if lrat else 'drat-trim')":"('cake_lpr' if lrat else 'drat-trim')",
    "assert p.returncode != 0 and ('c VERIFIED' if lrat else 's VERIFIED') not in p.stdout":
        "assert proof_check_status(checker.name,p.returncode,p.stdout,p.stderr) == 'rejected'",
    "assert q.returncode == 0 and ('c VERIFIED' if lrat else 's VERIFIED') in q.stdout and 'not verified' not in q.stdout.lower(),(name,q.stdout,q.stderr)":
        "assert proof_check_status(checker.name,q.returncode,q.stdout,q.stderr) == 'verified',(name,q.stdout,q.stderr)",
    'for p in (Path(__file__),fixture,manifest)':
        "for p in (Path(__file__),fixture,manifest,root/'tools/bench/proof_oracle.py',root/'tools/bench/test_proof_oracle.py')",
    'Two SAT negative controls reject empty proofs.':'Two SAT negative controls reject empty proofs under strict drat-trim/cake_lpr status checks. Legacy lrat-check false-accepted the same SAT LRAT control in6906 and is not used for acceptance here.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
