# Fuzz harness

Soundness pipeline per DEVELOPMENT_PLAN.md §8.2 (workflow: the `fuzz` skill).
Codex operates and inspects campaigns directly; Claude Code is not required.

- `run.sh --quick --solver CMD` — per-commit gate: regression corpus + 10,000
  seeded cases; `--nightly` = 200,000 cases + LRAT chain. These counts are
  owned by `run.sh`; keep this summary synchronized with its defaults.
- `slurm.sh` reserves 50 physical cores by default on the cluster's SMT-2
  nodes: 100 logical CPUs, 400,000 MiB, and 50 fuzz workers.  `--physical-cores
  P` changes all three coherently.  A worker count is parallelism within one
  fuzz campaign, not repeated fuzzing.  At submission it also creates a
  read-only `build/pin-fuzz-harness-*` snapshot of `run.sh`, the Python harness,
  stress scenarios, and the regression corpus.  The batch job executes only
  that snapshot and logs its aggregate SHA-256; it removes the snapshot after
  success and retains it after failure for direct diagnosis.  Never make
  a fuzz job execute a live repository script.
- **Read a leg by its `coverage:` and `reached:` fields, not by `--n`.**
  `coverage:` counts cases that reached a verdict — a case that times out runs
  NO oracle, and the non-terminating population is printed on its own line,
  saved under `<outdir>/nonterminating/` and can be made fatal with
  `--max-nonterminating-frac`. `reached:` gives per-rule fire counts from the
  solver's own counters; `--require-reached BCE,Substitute` makes a rule that
  never fired fatal. A corpus entry that does not terminate is fatal too
  (`--allow-nonterminating N` pre-registers an exception).
- `fuzz.py` — runner (subcommands `run`, `check-one`, `corpus`); oracles:
  model (`checkmodel`), proof (drat-trim / lrat-check), differential vs the
  oracle solver, crash/io consistency.
- `checkmodel` — independent model checker (never trusts solver bookkeeping).
- `ddmin` — delta-debugging shrinker (clause- then literal-level).
- `broken/` — deliberately broken solver wrappers used by `selftest.sh` to
  prove the harness catches lies (wrong verdict, corrupted model, truncated
  proof).
- `corpus/regressions/` — tracked, monotonically growing regression corpus.
- `selftest.sh` — end-to-end validation against Kissat/CaDiCaL stand-ins;
  runs in the weekly and manually dispatched CI workflow.

- `retention.py` — which tracked per-job artifact is still cited as evidence
  (job id, id range, or name — never the basename), for this directory's
  `slurm-*.log` and for `benchmarks/probes/*.out`. Report-only unless
  `--delete`. Uncited job logs belong in `$CLAUDE_JOB_DIR/tmp`, not in git.

The solver under test must speak the competition CLI: `solver CNF [PROOF]`,
`s`/`v` lines on stdout, exit 10/20. Every case is reproducible from the
recorded seed; failure bundles land in `failures/` (untracked) and are
promoted to the corpus manually once diagnosed.
