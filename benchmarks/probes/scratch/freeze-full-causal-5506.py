#!/usr/bin/env python3
"""Audit named normal campaigns; never inspect the Claude-owned fuzz campaign."""
from pathlib import Path
source = Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start = source.index('jobs={')
end = source.index('\ndef sha', start)
source = source[:start] + '''jobs={5454:1670292,5465:1670638,5468:1670699,5469:1670700,
      5473:None,5475:1671162,5476:1671163,5485:1671708,5487:1671745,
      5490:1671777,5496:1671973,5497:1671994,5498:1672011,
      5502:1672064,5505:1672156}''' + source[end:]
source = source.replace("f'20260907-{number}-*'", "f'{20260908 if number >= 5472 else 20260907}-{number}-*'")
source = source.replace('if number in (5437,5449,5453):',
                        'if number in (5487,5490,5496,5497,5498,5502,5505):')
start = source.index('def run(args):')
end = source.index('\nqueue=', start)
source = source[:start] + """accounting_probe = subprocess.run(
    ['sacct','-j','1670292','-X','--format=JobID,State,ExitCode','--parsable2'],
    cwd=root,capture_output=True,text=True)
def run(args):
    if args[0] == 'sacct' and accounting_probe.returncode:
        controller = subprocess.run(['scontrol','show','job',args[2]],
            cwd=root,capture_output=True,text=True)
        return json.dumps(dict(accounting_available=False,
            accounting_error=accounting_probe.stderr.strip(),
            controller_exit_code=controller.returncode,
            controller_stdout=controller.stdout,
            controller_stderr=controller.stderr,
            limitation='Missing controller jobs have unknown scheduler state; no completion inferred. Artifact oracles remain strict.'))
    return subprocess.run(args,cwd=root,check=True,capture_output=True,text=True).stdout
""" + source[end:]
source = source.replace('2026-09-07-eliminate-budget-main-ctest.log', '2026-09-08-full-causal-main-ctest.log')
source = source.replace('20260907-2255-eliminate-budget', '20260908-0212-full-causal')
source = source.replace('2026-09-07-eliminate-budget-validation.json', '2026-09-08-full-causal-validation.json')
source = source.replace('seconds=5.32', 'seconds=6.18')
source = source.replace('result=dict(observed_at=', 'result=dict(accounting_query=dict(exit_code=accounting_probe.returncode,stdout=accounting_probe.stdout,stderr=accounting_probe.stderr),observed_at=')
start = source.index('    source_status=')
end = source.index('\n    manual_removed_bytes=', start)
source = source[:start] + """    source_status='Production unchanged. Full5485=1.165336889, all40 candidate non-timing statistics/certificates unchanged from5459. Square first Factor extension mismatch943. Rank32 first14 Sweep seeds agree then Tactix alone processes836;5505 pending. Thirty new validator fixtures pass; goal unmet.',
    fuzz_owner='5492 resumed Claude session34566 pending; only terminal response polled. Original9171 returned submission-only, not a verdict. No fuzz artifacts inspected.',
""" .rstrip() + source[end:]
source = source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
                        "reviewer_cleanup='No manual deletion; preserve failed observer expectations and all cited campaign authorities.',")
exec(compile(source, __file__, 'exec'))
