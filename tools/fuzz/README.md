# Fuzz harness

Soundness pipeline per DEVELOPMENT_PLAN.md §8.2 (workflow: the `fuzz` skill).

- `run.sh --quick --solver CMD` — per-commit gate: regression corpus + ~1k
  seeded cases; `--nightly` = ~10k cases + LRAT chain.
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

The solver under test must speak the competition CLI: `solver CNF [PROOF]`,
`s`/`v` lines on stdout, exit 10/20. Every case is reproducible from the
recorded seed; failure bundles land in `failures/` (untracked) and are
promoted to the corpus manually once diagnosed.
