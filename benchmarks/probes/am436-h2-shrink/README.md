# AM436 H2 one-reason shrink

`run.sbatch` ran `shrink.py` as SLURM job 1723448 on the pinned AM423
public-API driver and AM369 checkers. The script SHA is fixed in the batch
file; `shrink.py` checks the external input and binary hashes before any
trial. `result.json` records the 88-test deletion shrink, and
`minimal.cnf`/`minimal.dsr` are the final original-CNF/proof pair.

`verify.py` rechecks no-option and diagnostic-option DSR in both directions,
decodes each successful LSR, runs Trestle, and asserts an incomplete proof
does not claim `VERIFIED UNSAT`. Its exact results are in
`verify-result.json`, with checker stdout/stderr in the adjacent logs (the
raw forward verbose trace is gzip-compressed to preserve its whitespace). This
does **not** change the production checker contract.

Interpretation and remaining work:
`benchmarks/reports/2026-09-25-am436-h2-one-reason-shrink.md`.
