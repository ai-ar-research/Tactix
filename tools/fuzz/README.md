# Fuzz harness

Soundness pipeline per DEVELOPMENT_PLAN.md §8.2 (workflow: the `fuzz` skill).

- `run.sh --quick --solver CMD` — per-commit gate: regression corpus + ~1k
  seeded cases; `--nightly` = ~10k cases + LRAT chain.
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
  runs in CI on every push.

- `retention.py` — which tracked per-job artifact is still cited as evidence
  (job id, id range, or name — never the basename), for this directory's
  `slurm-*.log` and for `benchmarks/probes/*.out`. Report-only unless
  `--delete`. Uncited job logs belong in `$CLAUDE_JOB_DIR/tmp`, not in git.

The solver under test must speak the competition CLI: `solver CNF [PROOF]`,
`s`/`v` lines on stdout, exit 10/20. Every case is reproducible from the
recorded seed; failure bundles land in `failures/` (untracked) and are
promoted to the corpus manually once diagnosed.
