# AM407 archived DSR checker-contract controls

Checker-only SLURM job `1721174` ran `run.sbatch` on `cpu-dense-hwulab-q`
(2 CPUs, 8 GiB, 30-minute limit); it completed `0:0` in three seconds.
The script's SHA-256 is
`19bbdbff1afcd5db69009fcf0903db234149bbb33fa7efb80428041d5055b6fc`.
No solver or production checker route was changed. The no-option checker is
the pinned AM369 `dsr-trim`; option mode uses AM404's exact wrapper that adds
only `--delete-implied-units`. Both modes require the repository's complete
backward DSR→LSR→Trestle pipeline and, if that passes, its forward every-lemma
audit against the clause-preserving original-CNF checker spelling.

The eight exact archived CNF/proof inputs (relative paths in the script) are
preserved in `inputs-1721174.tar.gz` (SHA-256
`25122750ff7a05ffea92c2f3bc2be640b22872ad33be24a68960564169d09393`).
The complete 34-file result tree, including pinned and input hashes before
and after, each checker log, LSRs and `results.tsv`, is preserved in
`job-1721174.tar.gz` (SHA-256
`b4c39f571ceca0b7ac2804916391d0c1d0e4fbc155266c90646542c7f6bf5c24`).
The results table itself has SHA-256
`cd02750cf7a3fc6afb9893f0529f988460aae961e2ee1461186de3526b12e936`.
The top-level SLURM stdout and empty stderr are also retained as
`out/1721174.out` and `out/1721174.err`.

All seven predeclared outcomes matched; the eighth, an observation-only
no-option check on the repaired AM312 proof, verified. This is an archived-certificate
contract diagnostic, not a new N2 acceptance gate; see the AM407 report.
