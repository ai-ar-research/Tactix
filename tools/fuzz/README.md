# Fuzz harness

## Routine fuzzing bundle

Include DefaultPolicy/public-random legs, retained/generated incremental C API
traces with minimization, and C/Python binding regressions. Full CI runs 16
traces per Debug/Release build; Python coverage is deterministic. `run.sh` is
one leg. Qualification adds change-scoped driver/coverage matrices.

Use shipped `--auto-proof`: DSR first, converting to PBP when required. Keep
explicit `--dsr`/`--pbp` backend and conversion coverage. Check each certificate
through its declared content route; SAT models must satisfy the original CNF.

## Incremental API traces

Each terminal query acquires an owned native result. After all sessions are
released, the client checks its model/core/assumptions again and compares every
retained certificate member byte-for-byte with its publication snapshot. The
harness requires this reach marker as well as independent bundle/model/proof
validation; UNKNOWN and invalidated borrowed views cannot acquire a result.

`ipasir_trace.py` and the `tactix-ipasir-trace` C client exercise persistent
sessions through public headers and `libtactix_ipasir.a`. The maintained
`ipasir-corpus/` is registered as CTest `ipasir_trace_regression`; its oracle
negative controls are `ipasir_trace_oracles`. Existing CLI, v1 C API, public-rule,
strategy, proof and performance tests remain acceptance requirements.

Trace operations are `init ID`, `release ID`, `add ID LITERAL`,
`assume ID LITERAL`, `terminate ID LIMIT`, `learn ID MAX_LENGTH`,
`learn_budget ID TICKS`, `core_budget ID TICKS`, `strategy ID MODE`,
`random_seed ID SEED`, `min_budget ID PROBES`, `min_context ID MASK`,
`min_stop ID LIMIT`, and `solve ID`.
`add ID 0` closes a clause. A termination limit of -1 disconnects, 0 never stops,
and a positive limit stops at that poll. A learning length of -1 disconnects;
zero accepts only an empty clause. Budgets use -1 for UINT64_MAX and zero to
suppress optional extraction/export work. Strategy modes are 0 DefaultPolicy,
1 public CDCL, 2 seeded public branch/phase CDCL, 3 an explicit configured
DefaultPolicy handoff, 4 the named native cruxmiter program, 5 the full
public-random grammar compiled from `public_random_main.cpp`, 6 native DPLL,
7 native CDCL without restarts, 8 native scripted-default and 9 native Chvatal.
Modes 6/7/8 must record their exact native name, with no root-program work or
DefaultPolicy handoff. Chvatal preserves its explicit DefaultPolicy handoff;
its identity and root-program accounting are checked as for Cruxmiter. Mode 5 retains
its RNG, root quota, step count and online revision across same-query
restoration; it rebinds the replacement execution view and never hands off to
DefaultPolicy. Seed values are nonnegative signed 32-bit integers. Per-query
coverage records root/search transitions, partial/full restarts, per-rule
requests/effective applications and all five public application forms.
Native identity
and root requests/eligible/effective actions, completed rounds and explicit
handoffs are checked separately. These clients use the same persistent session.
Whole-line `#` comments are allowed. Session IDs can be reused after release.

Minimization is optional after an UNSAT outer solve. Its budget is -2 to
disable (default), -1 for unlimited probes, or a nonnegative probe count.
The context mask selects raw assumption positions 0–30, then normalizes
duplicates. `min_stop` inherits the outer termination callback at -2 (default);
otherwise it temporarily installs a fresh poll counter and the specified
-1/0/positive callback limit for minimization, then restores the caller's
configuration and result view. This permits UNKNOWN probes after a successful
source query without confusing the source verdict with probe interruption.

Each outer call and hidden minimization probe has a unique artifact index;
native query IDs also count UNKNOWN probes. The client retains terminal probe
handles after the reduction and session close, checks source-view preservation,
and compares every retained member byte-for-byte. Probe records include exact
assumptions, best-core/context binding, certificate identity and SAT deletion
witnesses. The oracle independently reconstructs deletions, checks budgets,
refutation and subset-minimality, licenses learned exports from F alone, and
runs the same model/proof/core, fresh replay and CaDiCaL chain for every probe.
The total owned-result marker must include probes, not only outer solves.

The oracle validates every learned clause against permanent F without A and
rejects private/assumption-only names, checks size/configuration/driver reach,
and independently verifies exact reduced F+C bundles when extraction succeeds.
It also recomputes the permanent-input digest from the original signed words,
including duplicates, tautologies and clause terminators, excluding assumptions.
Fresh replay retains the selected strategy, learning and core configuration but
runs without requested termination. Remaining specialized adapters and the ordinary
DefaultPolicy/public-random acceptance legs remain separate. IPASIR-UP is excluded.

For an immutable cluster campaign, invoke the pinned harness with `--driver
PIN/tests/tactix-ipasir-trace --reference PIN/cadical --n COUNT --seed FIRST
--out NEW_DIRECTORY --root PIN/source`. Add `--corpus PIN/source/tools/fuzz/ipasir-corpus`
on the corpus shard. Use `--strategy 0` and `--strategy 5` for separate generated
DefaultPolicy/public-random legs; retained corpus traces keep their declared
drivers. Add `--minimize` to generate minimization budgets, contexts and probe
interruption; omit fixed strategy selection for mixed-driver switching.
The generator tracks legal states; enumeration checks every
completed query, every completion of a partial model and the failed-assumption
set. Interrupted queries consume assumptions and are checked against a fresh,
uninterrupted solve of the exact original F+A. The
runner verifies exact F+A snapshots, artifact hashes, model/core artifacts and
each UNSAT certificate, compares pinned CaDiCaL and repeats queries in fresh
Tactix sessions. Solver and checker process trees have separate resident-memory
and wall-time limits through `memory_runner.py`, compatible with ASan. Successful
case artifacts are removed; failures/resources retain their original trace,
certificates, outputs and resource records. `summary.json` states reached query
coverage and pending interfaces. Missing coverage never completes the N5 gate.

Use `--shrink FAILED.trace` with the same pinned driver, reference, limits and
oracles to minimize a deterministic failure while preserving legal states and
its failure class. Keep the original bundle and promote the checked minimized
trace into `ipasir-corpus/` with its causal regression. Resource failures retain
their artifacts for the declared recheck workflow; they are not correctness
verdicts and are not automatically shrunk.

## Ordinary policy fuzzing

Both `--nightly` arms default to at least 100,000 independently checked verdicts
and at most 5% nonterminating cases, with zero correctness failures. Explicit
`--minimum-verdicts` and `--max-nonterminating-frac` arguments override these
campaign-wide defaults. Coverage and corpus requirements remain mandatory.
To regrade saved shards after an intentional threshold change, pass those
options to `aggregate.py` together with `--regrade-reason`. It prints the old
and revised thresholds, preserves the saved summaries, and still checks every
other requirement. Retain the regrade output alongside the original result.

- `run.sh --quick --solver 'build/release/examples/tactix-default --auto-proof'` runs the regression corpus plus 10,000 seeded cases through the C default strategy over `tactix.h`; `--nightly` selects 200,000.
- `run.sh --public-random --quick --solver 'build/release/tests/tactix-public-random --auto-proof --random-policy={seed}'` runs the same generated cases through the sole random qualification strategy. It samples the grammar below one public action at a time, re-observes the solver after every action, and varies root work, CDCL intervals, full/partial restarts, application forms and eligible algebraic actions. Each case receives its own reproducible policy seed.
- Both legs use automatic proofs and the content-selected oracle below; explicit DSR/PBP legs cover their backends. Retired private random/stress frontends do not qualify public-random.
- `slurm.sh` reserves 50 physical cores by default on the cluster's SMT-2 nodes: 100 logical CPUs, 400,000 MiB, and 50 fuzz workers. `--physical-cores P` changes all three coherently. A worker count is parallelism within one fuzz campaign, not repeated fuzzing. At submission it creates a read-only `build/pin-fuzz-harness-*` snapshot of the runner, corpus and pinned VeriPB/CakePB binaries. The batch job executes only that snapshot and logs its aggregate SHA-256; it removes the snapshot after success and retains it after failure for Claude-owned diagnosis. A small deduplicated copy under `failures/harnesses/<sha256>/` keeps successful-leg timeout controls replayable after the transient snapshot is gone. Never make a fuzz job execute a live repository script.
- `slurm.sh --cpu-q-wide` maps one campaign over 512 array tasks. Each task requests one physical core and runs one single-threaded worker on one shard; workers are never packed into a larger allocation. Every shard is eligible immediately, and Slurm shares partition capacity among independently submitted campaigns. Shards own disjoint global indices, while shard zero alone runs the corpus and fixed controls. Isolated artifacts and an `afterany` aggregate finalizer retain the shared immutable pin on any failed, canceled or missing shard.
- Every worker predeclares separate resident-memory ceilings for its solver and checker children (6,144 MiB each inside the default 8,000 MiB task). `memory_runner.py` monitors the complete child process tree without an address-space limit, so Debug ASan shadow mappings remain usable. A solver OOM, checker OOM, timeout, or wrapper failure is retained as an incomplete `resource`/`harness` case, never mislabeled as a solver crash or invalid proof. The bundle records peak RSS, the limits, cgroup OOM events, exact artifacts, and SHA-256 hashes. For a checker resource event, `recheck.sh` replays only the retained checker command on disposable copies under a declared larger ceiling; it never reruns the solver or overwrites the retained proof. Verification closes the case only for a terminal checker; a recovered elaboration stage still requires its downstream terminal checker. A resource-incomplete case remains a failed gate until that independent oracle completes.
  Custom layouts use `--shards`, `--physical-cores`, `--threads-per-core` and `--partition`. `--index-offset` preserves unsharded cases; aggregate coverage/reach requirements preserve grading.
- Core clausal checkers (`cake_lpr`, `drat-trim`, `dsr-compress`, `dsr-trim`, `lrat-check`, `srcheck`) default to the mutable `tools/checkers/bin` links. `slurm.sh --checkers DIR` freezes them instead: `DIR` holds exactly those basenames and a `SHA256SUMS` manifest; submission verifies it, installs a read-only content-addressed copy under `failures/checkers/<sha256>/`, and records that path and manifest in the snapshot (so failure `repro.sh` files use the same checkers); the job re-verifies it. For a live checkout, `TACTIX_FUZZ_CHECKERS=DIR` selects the same kind of collection, and `fuzz.py checkers` prints and verifies whichever is in effect. Optional `--dsr-transport FILE` pins the CMake `dsr-transport` helper and its hash beside the oracle; missing helper pins retain Python parsing, never weaker checking.
- **Read a leg by its `coverage:` and `reached:` fields, not by `--n`.**
  Public frontends emit sparse `c public-reach RULE COUNT` observations through
  the installed C statistics API. These do not claim the full CLI dump:
  Propagate/Decide/ConflictAnalysis report their native work counts, BVE reports
  resolution work, and BCE/Substitute require deleted clauses/substituted
  variables. Attempted actions and NoOps do not replace these witnesses; the
  public-random effective-action/transition/form requirements remain separate.
  `coverage:` counts cases that reached a verdict — a case that times out runs
  NO oracle, and the non-terminating population is printed on its own line,
  saved under `<outdir>/nonterminating/` and can be made fatal with
  `--max-nonterminating-frac`. `reached:` gives per-rule fire counts from the
  solver's own counters; `--require-reached BCE,Substitute` makes a rule that
  never fired fatal. A corpus entry that does not terminate is fatal too
  (`--allow-nonterminating N` pre-registers an exception).
  The public-random leg may additionally require source counters with
  `--require-policy-applied` and `--require-policy-effective`; the latter
  rejects a campaign that attempted a rule but never changed solver state.
  `--require-policy-counter` makes transition or application-form coverage
  (for example `full-restarts,semantic,online`) an aggregate gate.
- `fuzz.py` — runner (subcommands `run`, `check-one`, `corpus`); oracles:
  model (`checkmodel`), whole-run PBP (VeriPB elaboration then CakePB against the
  original CNF, both required), witness-free native DSR (validated lossless
  DRAT conversion to backward drat-trim and CakeLPR, plus a forward
  `drat-trim -f` every-lemma audit of that conversion), witnessed DSR
  (no-option `dsr-trim` to LSR to Trestle, plus a forward `dsr-trim -f`
  every-lemma audit). If forward elaboration omits live singleton premises in
  its final empty-clause RUP chain, the harness adds only those active unit
  hint IDs and requires Trestle to verify the completed LSR against the same
  CNF; no solver clause or proof lemma is waived. A reference solver's
  DRAT (`drat-trim`) or LRAT (`cake_lpr`), differential verdict, and crash/I/O
  consistency. Legacy `lrat-check` is diagnostic only: it can
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
  `TACTIX_FUZZ_CHECKERS`.
- `controls/every_lemma_controls.py` — causal controls for both every-lemma
  audits on retained fixtures (`controls/every-lemma/PROVENANCE.md`). They
  require the configured `dsr-trim` to elaborate the F2/F2w proofs forward
  correctly, so they fail with the historical root-clone build 02824056.

## Public-random strategy grammar

The random qualification policy samples the strategy grammar defined for
strategy authors in [`docs/strategies.md`](../../docs/strategies.md#1-general-scheduling-shape).
Applicability guards a grammar-selected terminal; it does not define the trace
language or silently remove an action family.

`public_random_grammar.hpp` classifies every active public action into exactly
one terminal family.  A new public action must extend that classification or
the grammar test fails.  At execution time the policy queries applicability
again after choosing a terminal.  Release treats a missing chosen action as a
defensive failure; assertion-enabled builds additionally assert that the rule
and any exact restart/decision instance are among the freshly offered public
actions before applying them.

The solver under test must speak the competition CLI: `solver CNF [PROOF]`, `s`/`v` lines on stdout, exit 10/20. Every case is reproducible from the recorded seed and each invocation gets a private artifact namespace. Cluster submissions use `failures/runs/slurm-<job-id>/<leg-name>/`; direct invocations use `failures/runs/local-<pid>/{seeded,public-random}/`. This prevents concurrent default and public-random legs with identical indices/seeds from replacing bundles or `nonterminating.json`.
When the pinned Tactix executable is renamed so its basename is not `tactix`,
include a proof flag in the `--solver` command. The harness otherwise assumes an
unknown executable emits DRAT. `--pbp` selects whole-run PBP and its
VeriPB/CakePB oracle route; `--auto-proof` (the command-line default) selects
automatic proofs, checked by their content route, whose one declared
`c proof-route` must agree with the content. Use `--dsr` only for an explicit
native-DSR campaign. The last proof flag wins, as on the command line.
Set `TACTIX_FUZZ_RUN_ID` and `TACTIX_FUZZ_LEG_ID` for a stable custom namespace, or pass `--outdir`; identifiers permit letters, digits, `.`, `_` and `-`.
Failure bundles land in that private directory (untracked) and are promoted to the corpus manually once diagnosed. Immutable checker and replay-harness stores remain shared under `failures/checkers/` and `failures/harnesses/` because their directory names are content hashes.
