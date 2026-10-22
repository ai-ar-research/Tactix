# Fuzz harness

Soundness pipeline per DEVELOPMENT_PLAN.md §8.2 (workflow: the `fuzz` skill).

- `run.sh --quick --solver CMD` — per-commit regression corpus plus 10,000 seeded cases; `--nightly` selects 200,000. Default Tactix uses whole-run PBP, and every UNSAT result must pass VeriPB and CakePB. These counts are owned by `run.sh`.
- `run.sh --controller-v5 --quick --solver CMD` — 10,000 deterministic structure-preserving GF(3)/Direct CSP mutations across SAT/model and UNSAT/proof oracles. Every typed payload must make an effective transition, and seeded trace/replay runs first. Ordinary random CNFs cannot supply these payloads. The historical `v5` fixture currently binds action-space v8; nightly selects 200,000.
- `run.sh --forced-ttf --quick --solver CMD` — 10,000 deterministic fixed-row positives, near misses, mutations, poison cases and tight budgets, with checked reach, replay and interruption. Nightly selects 200,000. The large campaign gates changes to balanced-signature/TTF recognition, scheduling, guards, harness or oracles; otherwise compact `balanced_signature_model` and `forced_ttf_fuzz` remain mandatory and compatible immutable campaign evidence may be reused. This leg is not part of the universal fresh-PBP seed count.
- `slurm.sh` reserves 50 physical cores by default on the cluster's SMT-2 nodes: 100 logical CPUs, 400,000 MiB, and 50 fuzz workers.  `--physical-cores P` changes all three coherently.  A worker count is parallelism within one fuzz campaign, not repeated fuzzing.  At submission it also creates a read-only `build/pin-fuzz-harness-*` snapshot of `run.sh`, the Python harness, stress scenarios, the regression corpus, and pinned VeriPB/CakePB binaries. The batch job executes only that snapshot and logs its aggregate SHA-256; it removes the snapshot after success and retains it after failure for Claude-owned diagnosis. A small deduplicated copy under `failures/harnesses/<sha256>/` keeps successful-leg timeout controls replayable after the transient snapshot is gone. Never make a fuzz job execute a live repository script.
- `slurm.sh --cpu-q-wide` maps one campaign over 64 array tasks × 13 single-threaded workers and throttles it to 16 concurrent tasks (208 CPUs per campaign). Submit independent campaigns together; Slurm shares partition capacity. Shards own disjoint global indices, while shard zero alone runs the corpus and fixed controls. Isolated artifacts and an `afterany` aggregate finalizer retain the shared immutable pin on any failed, canceled or missing shard.
  Custom layouts use `--shards`, `--physical-cores`, `--threads-per-core`, `--array-concurrency` and `--partition`. `--index-offset` preserves unsharded cases; aggregate coverage/reach requirements preserve grading.
- Core clausal checkers (`cake_lpr`, `drat-trim`, `dsr-compress`, `dsr-trim`, `lrat-check`, `srcheck`) default to the mutable `tools/checkers/bin` links. `slurm.sh --checkers DIR` freezes them instead: `DIR` holds exactly those basenames and a `SHA256SUMS` manifest; submission verifies it, installs a read-only content-addressed copy under `failures/checkers/<sha256>/`, and records that path and manifest in the snapshot (so failure `repro.sh` files use the same checkers); the job re-verifies it. For a live checkout, `TACTIX_FUZZ_CHECKERS=DIR` selects the same kind of collection, and `fuzz.py checkers` prints and verifies whichever is in effect. Optional `--dsr-transport FILE` pins the CMake `dsr-transport` helper and its hash beside the oracle; missing helper pins retain Python parsing, never weaker checking.
- **Read a leg by its `coverage:` and `reached:` fields, not by `--n`.**
  `coverage:` counts cases that reached a verdict — a case that times out runs
  NO oracle, and the non-terminating population is printed on its own line,
  saved under `<outdir>/nonterminating/` and can be made fatal with
  `--max-nonterminating-frac`. `reached:` gives per-rule fire counts from the
  solver's own counters; `--require-reached BCE,Substitute` makes a rule that
  never fired fatal. A corpus entry that does not terminate is fatal too
  (`--allow-nonterminating N` pre-registers an exception).
  Controller legs may additionally require nonzero source counters with
  `--require-controller-applied` and `--require-controller-effective`; the
  latter rejects attempted structured actions that published nothing.
- `fuzz.py` — runner (subcommands `run`, `check-one`, `corpus`); oracles:
  model (`checkmodel`), whole-run PBP (VeriPB elaboration then CakePB against the
  original CNF, both required), witness-free native DSR (validated lossless
  DRAT conversion to backward drat-trim and CakeLPR, plus a forward
  `drat-trim -f` every-lemma audit of that conversion), witnessed DSR
  (no-option `dsr-trim` to LSR to Trestle, plus a forward `dsr-trim -f`
  every-lemma audit). If forward elaboration omits live singleton premises in
  its final empty-clause RUP chain, the harness adds only those active unit
  hint IDs and requires Trestle to verify the completed LSR against the same
  CNF; no solver clause or proof lemma is waived. Explicit
  DRAT (`drat-trim`), explicit native LRAT (`cake_lpr`), differential verdict,
  and crash/I/O consistency. Legacy `lrat-check` is diagnostic only: it can
  accept an invalid empty lemma on a satisfiable input. Failed cases retain the
  original proof and exact checker commands/stdout/stderr.
- `checkmodel` — independent model checker (never trusts solver bookkeeping).
- `ddmin` — delta-debugging shrinker (clause- then literal-level).
- `broken/` — deliberately broken solver wrappers used by `selftest.sh` to
  prove the harness catches lies (wrong verdict, corrupted model, truncated
  proof).
- `corpus/regressions/` — tracked, monotonically growing regression corpus.
- `selftest.sh` — end-to-end validation against Kissat/CaDiCaL stand-ins;
  runs in the weekly and manually dispatched CI workflow. It honours
  `TACTIX_FUZZ_CHECKERS` and `TACTIX_FUZZ_SELFTEST_TACTIX` (the pinned build
  for its native-LRAT control, otherwise `build/debug/tactix`).
- `controls/every_lemma_controls.py` — causal controls for both every-lemma
  audits on retained fixtures (`controls/every-lemma/PROVENANCE.md`). They
  require the configured `dsr-trim` to elaborate the F2/F2w proofs forward
  correctly, so they fail with the historical root-clone build 02824056.
- `retention.py` — reports which tracked per-job artifact is still cited as evidence (job id, id range, or name, never the basename), for this directory's `slurm-*.log` and `benchmarks/probes/*.out`. Report-only unless `--delete`; uncited job logs belong in `$CLAUDE_JOB_DIR/tmp`, not in git.

The solver under test must speak the competition CLI: `solver CNF [PROOF]`, `s`/`v` lines on stdout, exit 10/20. Every case is reproducible from the recorded seed and each invocation gets a private artifact namespace. Cluster submissions use `failures/runs/slurm-<job-id>/<leg-name>/`; direct invocations use `failures/runs/local-<pid>/{seeded,controller-v5}/`. This prevents concurrent default, random-policy and typed legs with identical indices/seeds from replacing bundles or `nonterminating.json`.
When the pinned Tactix executable is renamed so its basename is not `tactix`,
include `--pbp` in the `--solver` command. The harness otherwise assumes an
unknown executable emits DRAT; `--pbp` selects Tactix's default whole-run PBP
mode and its VeriPB/CakePB oracle route. Use `--auto-proof` only for the
retained startup-PBP/DSR compatibility gate.
Set `TACTIX_FUZZ_RUN_ID` and `TACTIX_FUZZ_LEG_ID` for a stable custom namespace, or pass `--outdir`; identifiers permit letters, digits, `.`, `_` and `-`.
Failure bundles land in that private directory (untracked) and are promoted to the corpus manually once diagnosed. Immutable checker and replay-harness stores remain shared under `failures/checkers/` and `failures/harnesses/` because their directory names are content hashes.
