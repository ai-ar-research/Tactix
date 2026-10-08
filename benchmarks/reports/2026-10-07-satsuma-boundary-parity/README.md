# Satsuma-boundary parity on fixed44 rows 896, 754, 453, 1144

This report applies the Rank899 procedure to four rows. For each row it
compares the actual Satsuma residual against Tactix candidate c2 at
`preprocess_begin`, with normal structured startup. It then ports the owner
mechanism at the first divergence. That port is the diagnostic variant
`port2`, and [`proposed-port.diff`](proposed-port.diff) holds it. No `src/`
file was edited.

`evidence.json` records every hash and job ID. The run directories are
`benchmarks/runs/20261007-satsuma-boundary-*`.

**Headline.**
- **453.** c2 already equals the Satsuma residual exactly.
- **896.** Satsuma does nothing here: 0 generators and 0 iterations. Modulo
  pure-literal depth, c2 equals the residual. The row stays open in both pair
  arms and belongs to M2.
- **754 and 1144.** Here the first divergence is the same pure-literal depth,
  followed by three gaps in the large-domain chain: missing AMO completion
  (contrary to plan §9.5 (C)), duplicate clauses, and the choice of
  representative unit.
- **port2 result.** port2 makes three changes: one pure round per pass,
  repetition earned only by output (§9.5 (A)), and the complete large-domain
  chain (§9.5 (C)). With it, **754 is SAT in 240.9 s with a checked model,
  where c2 is UNKNOWN at 5000 s.** This is one same-CPU draw per arm.
  Conflicts per second are equal; profiled below.
- **Not a verdict.** This is a diagnostic variant. It is not a
  default-policy verdict and needs the standing gates.

## Method

- **Reference.** The reference is the SC2026 binary
  `winners/satsuma-iter-kissat-sc2026/anders/satsuma-iter-kissat/src/solver/satsuma-dev/satsuma`
  (`0680c895…`, byte-identical to the frozen extractor pin). It runs the
  exact `run_base.sh` command:
  `satsuma fix <cnf> --silent --full-skip-limit 100000000 --add-reduced-as-unit --bsr --proof-file … --out-file …`.
  A second run drops `--silent` to get Satsuma's own log. Its residual and
  proof match the silent run byte-for-byte on every row.
- **Extractor.** The frozen 1754883 extractor is reused byte-identically
  (`run.py` `275565df…`, `compare.py` `311d7a8e…`).
- **Tactix builds.**
  - c2 runs from an EVENT_TRACE Release pin built from the c2 archive
    `4cdf01d9…` (job 1756333).
  - The supplement adds a `--default-graph-labeling=on` arm, which uses the
    fixed44 arguments.
  - Per-pass time and output come from the feedback stream:
    `stage_actions.py` records actions, applications, root assignments and
    wall time per stage, step and pass.
- **Modulo comparison.** `pure_modulo.py` compares both canonical multisets
  after closing pure literals on each side. This separates
  pure-literal-depth differences from the rest. These scripts read trace
  files only; the large-row ones ran under SLURM, and the sub-second ones
  for 896 and the stage summaries ran on the login node.
- **Not run through the runner.** The trace and extractor runs cannot go
  through `runtime_task.py`. Each arm is bounded to 300 s, with a 64 MiB file
  cap, or 4 GiB for 453 because of its 254k-action feedback stream. The
  formula copies, proofs and oversized feedback streams were deleted; their
  hashes are in each run's `CLEANUP.json`.

## Per-row first divergence (c2)

| Row | Satsuma `fix` (own log) | c2 at `preprocess_begin` | First divergence |
|---|---|---|---|
| 896 pcmax m19, 10,198 vars | 0 DejaVu generators, 0 symmetry output, **0 iterations**, 89 propagated units plus 4 pure units; 58 ms | 0 generators accepted, 0 SR applications at the boundary. Pure closure eliminated 64 variables (no units) | **Pure-literal depth.** Satsuma assigns one round of pure literals as units (4). Tactix closes pure literals to fixpoint by BVE (64 variables, 665 clauses). Modulo pure closure, the two formulas are **identical** (60,682 clauses). The symmetry state is equal: neither side has any symmetry |
| 453 lockchart g2, 24,928 vars | 3 generators, 1 unit and 1 cut, 254,448 ULC AMO binaries, **1 iteration**; 662 ms | **Exact equality.** Canonical multiset (298,326), 16 roots and the full activation queue (18,224) all equal Satsuma | None. The boundary is equal |
| 754 Timetable E48, 258,984 vars | 4 generators, 4 units by `binary_fixing`, 605 ULC AMO binaries, 15,789 pure units, **1 iteration**; 1.79 s | 5 generators and 5 units; pure closure eliminated 18,949 variables; 0 AMO binaries; 17,820 duplicate clauses kept | Same pure-depth divergence (2,590 pure literals that Satsuma leaves are eliminated by Tactix). Then, after closing pure literals: 595 ULC AMO binaries missing; 17,820 duplicates that Satsuma's clause set removes; units on the other endpoint of each pair (Satsuma −146844/−146887/−146890/−146897, Tactix −146845/−146888/−146892/−146898); and one extra Tactix unit (−146849). The remaining difference is 548 + 434 clauses on the swapped unit variables, plus 398 original clauses that Satsuma keeps next to Tactix's extra unit |
| 1144 Timetable E50, 260,757 vars | 5 generators, 5 units, 949 ULC AMO binaries, **1 iteration**; 1.60 s | 8 SR applications, 9 roots; 0 AMO binaries; 17,820 duplicates | Same structure as 754 |

On the rows with symmetry, the c2 boundary is at least as strong as
Satsuma's in units. On 896 Satsuma does nothing, so it cannot explain the
owner's 2,378 s: 896 is a Kissat 4.0.4 search row (M2), not an M1 row. The
planning report's "8 SR applications, 0 applied" on 896 (the d170 binary) is
not a boundary effect. c2 makes no SR application at startup, and Satsuma
emits no symmetry output. Where those 8 applications occur in d170 is an open
item.

## Source explanation

### Pure literals: the first divergence on 896, 754 and 1144

- **Satsuma.**
  - `satsuma.h:698-725` collects the literals pure *now*, assigns them in
    "one round of pure literals", and propagates once.
  - `--add-reduced-as-unit` writes them as units (`satsuma.h:750-756`, and
    `drat_clause` at `:830`).
  - Another round happens only on an earned iteration.
- **Tactix.**
  - `default_policy.cpp:692-693` selects `BveCandidateScope::PureClosure`
    with the comment "the owner closes pure literals". That comment misstates
    the owner.
  - Step 94 (`structured_root_strategy.cpp:20-26`) inherits that scope.

### Repetition: plan §9.5 (A), `SP-SR-REPEAT-OUTPUT`

- **Satsuma** iterates only while `sbp.n_clauses() >= 1`
  (`satsuma.h:809`). ULC AMO binaries are additions to `formula_db`
  (`cnf.h:144`), not `sbp` clauses.
- **Tactix** repeats a stage on any rule progress:
  - `online_sequence.hpp:289` sets `progress_ |= made_rule_progress`, and
    `:329` repeats on it;
  - `structured_root_strategy.cpp:124` and `default_policy.cpp:825` select
    that repetition.
- **Observed.**
  - On 896, c2 stage 1 runs a second pass, about 58 ms with zero output,
    earned only by pure closure and compaction.
  - On 453, stage 1 repeats after AMO-only output.
- **Why the scope change alone fails.** With `PureOnly` but no output gate
  (diagnostic `port`, job 1756744), 896 stage 1 repeats **20 times**. Each
  pass peels one pure layer and finds no symmetry, for 2.68 s of zero-output
  work. The two mechanisms are coupled.

### The 20,000-variable condition is dead code in the owner

The coordinator asked about the `formula.n_variables() < 20000` term in the
iteration condition (`satsuma.h:810`). That term reads `cnf2wl formula` after
`formula.clear()` at `satsuma.h:777`, and `clear()` sets
`number_of_variables = 0` (`cnf2wl.h:443`). The term is therefore always
true. The logs confirm it: Satsuma performs **1 outer iteration** at 24,928
variables (453), 258,984 (754) and 260,757 (1144).

The effective gates are:
- `iteration < iteration_max`, where the iteration ceiling is about 7 for
  754 and about 32 for 453;
- `sbp.n_clauses() >= 1`;
- `!reached_limits`.

`reached_limits` must have been false after iteration 0 on 453, 754 and
1144, since each iterated. On 896 the iteration was blocked by
`sbp.n_clauses() == 0`, so whether limits were reached does not matter there.

| Row | Satsuma outer passes | c2 Tactix passes | port2 Tactix passes |
|---|---|---|---|
| 896 | 1 (0 iterations, no sbp) | stage 1 ×2 (the second earned only by pure/compaction), stage 2 ×1, stage 3 ×1 | stage 1 ×1, stage 2 ×1, stage 3 ×1 |
| 453 | 2 (iteration 0 then 1) | stage 1 ×2 (the second earned by AMO-only output), stage 2 ×2, stage 3 ×1 | stage 1 ×1, stage 2 ×2, stage 3 ×1 |
| 754 | 2 | large-domain stage ×2 | stage ×3 |
| 1144 | 2 | large-domain stage ×2 | stage ×3 |

How these repetitions relate to the owner and the parity-ledger items:
- **Outer iteration.** A Tactix *stage repetition* corresponds to Satsuma's
  outer iteration.
- **Inner loop.** Satsuma's inner fix-and-redetect loop has no repetition
  counterpart in Tactix. That loop is `fix_and_cut` (Schreier levels,
  `symmetries.h:3060`), or for the large domain the generator loop of
  `binary_fixing` (`symmetries.h:1600`). Tactix carries it inside one graph
  step.
- **Extra stage entries.** Tactix's split into 3 stages adds stage entries
  that the owner does not have. Each stage starts with its own pure step.
- **No violation of the 20,000-variable term.** It is vacuous in the owner,
  as shown above.
- **`SP-SR-REPEAT-OUTPUT`: violated by c2.** See 896 and 453 above.
- **`SP-SR-REPEAT-LIMIT`: not exercised.** Every observed stop is a stop for
  lack of output.

### Large domain: plan §9.5 (C)

- **Satsuma** runs `ulc_add_amo` (`satsuma.h:773`, `cnf.h:144-165`) and the
  row, row-column and Johnson detectors at every size. Only the fixing pass
  switches above 50,000 variables (`satsuma.h:217`, `:227`).
- **c2** erases every step but 94/84/85 from the large-domain program
  (`structured_root_strategy.cpp:247-261`, "stay out (parity ledger)"). The
  parity ledger (§13, line 530) and plan §9.5 (C) say this exception was
  removed by user decision, so **the code contradicts the plan**.

### Duplicates and the choice of representative (754/1144, not ported)

- **Duplicates.**
  - Satsuma's clause store is a set (`cnf.h:17`, with
    `++removed_duplicate_clauses` at `:289`).
  - Tactix keeps duplicates: 17,820 on 754, the gap between `physical` and
    `distinct`. Each duplicate pair is also a clause-vertex automorphism, so
    DejaVu returns 17,825 callbacks for 5 kept generators.
- **Representative.**
  - Satsuma's `binary_fixing` (`symmetries.h:1631-1652`) scans
    `aw.supp()` in the order of `stored_automorphism::load`. That order is
    the cycle order of the hook support (`symmetries.h:73-89`,
    `dejavu/groups.h:779-800` and `:852-888`), and each cycle starts at the
    image of its first-seen element. The unit is `map_lit`.
  - Tactix's port (`symmetry.cpp:587-728`) picks `target` the same way. But
    it scans `image.moved`, which `sparse_permutation.hpp:68` sorts, so it
    lands on the other endpoint of every pair. Each choice is the image of
    the other under the generator, so the residuals differ only by renaming.

## Proposed faithful port (`proposed-port.diff`)

The diff is against the c2 source. It contains three coupled pieces:

1. **Pure literals.** `PureOnly` (one round) replaces `PureClosure` in the
   reference pure step, and step 94 inherits it. This mirrors
   `satsuma.h:715`.
2. **Output-earned repetition (plan §9.5 (A)).** The diff adds a typed,
   public `RepeatMode::UntilSizeScaledOutputFixpoint`
   (`TACTIX_ONLINE_REPEAT_SIZE_SCALED_OUTPUT_FIXPOINT 4`, with serializer and
   API mapping). Its stage repetition is earned only by
   `sr_applied - sr_unique_literal_amo_binaries` growing during the round.
   This mirrors `satsuma.h:809` `sbp.n_clauses() >= 1`.
   - Both the structured programs and the reference program use it.
   - Both executors honour it: `OnlineStrategySequence` drives the startup
     stages and `OnlineStrategyExecutor` drives the public programs.
3. **Complete large-domain chain (plan §9.5 (C)).** The large-domain program
   keeps the whole regular graph stage: pure, AMO completion, the graph step
   with generator fixing, compaction, rows and product. This mirrors
   `satsuma.h:773` and `:217`/`:227`.

**Status of the diff:**
- It builds cleanly with gcc 13.1 Release, with no new warnings (job
  1756753).
- No Debug or test run has been done.
- The `calculus` and `rule-dev` updates, the `random_online` draw for the
  new mode, the ledger entry and the parity-ledger edit are not included.

### What port2 changes at the boundary (jobs 1756754-57)

- **896.**
  - The stage-1 repeat is gone, leaving one pass of each stage.
  - The pure step eliminates 4 variables in stage 1, which equals Satsuma's 4
    pure units.
  - Stage 2 starts with its own pure round and eliminates 4 more
    second-layer variables (35 clauses). This is the only remaining
    difference, apart from units represented as eliminations.
  - Structured time fell from 292 ms to 234 ms.
- **453.** The boundary is still exactly equal, and stage 1 now runs once.
  The cost of 8.0 s for the 254,448 AMO binaries is unchanged; it is a unit
  cost of about 31 µs per binary (Satsuma takes 0.66 s in total). It is
  output that gets used, not a dead pass.
- **754 / 1144.**
  - AMO completion now runs: 396+105 binaries against Satsuma's 605 on 754,
    and 726+111 against 949 on 1144.
  - **Pass 2 now fixes 179 additional units by generator fixing**, where
    Satsuma's iteration 1 fixes none. Pass 3 then stops.
  - Structured time rose from 1.2 s to 7.3 s. About 3.9 s of that is
    row/product refinement (steps 86/82), which produces nothing in all three
    passes. The new user rule names exactly this kind of pass.
  - **These are the next divergences on the large-domain rows:** the AMO
    count, the pass-2 units, the duplicates and the representative choice.

## Same-CPU pairs (c2 vs port2, 5000 s, runner)

Both campaigns were generated by `make_pair.py`: one allocation per row,
8 CPUs and 32 GiB on `cpu-dense-hwulab-q`, both arms run in sequence on the
same CPU through `runtime_task.py`, with
`--auto-proof --default-graph-labeling=on`.

**754** (`20261007-satsuma-boundary-pair-r754`, job 1756771, node018; one
draw per arm):

| Arm | Binary | Result | Wall | Conflicts | Decisions | SR applied | conf/s |
|---|---|---|---:|---:|---:|---|---:|
| c2 | `pin-candidate-c2-rel` | UNKNOWN (timeout) | 5000.0 s | 43,231,922 | 756,912,798 | 5 (5 units) | 8.6k |
| port2 | `d4a33c42…` | **SAT**, exit 10, checkmodel `OK — 829494 clauses satisfied, 258984 vars assigned` | **240.9 s** | 2,071,622 | 68,970,753 | 684 (501 AMO binaries, 183 units) | 8.6k |

The SLURM state reads FAILED only because the runner returns 1 for the
timed-out c2 task; both records are complete.

For reference, Satsuma+Kissat solved this row in 424 s and Kissat 4.0.4 in
1,508 s, both in a different window and not paired with this run.

**Profile (plan §8.3).** Job 1756821 (node015, one CPU, both arms 300 s,
perf `-F 199 -g`) used the same pins and arguments under the documented
direct-execution profiling exception. Its reports are in
`benchmarks/runs/20261007-satsuma-boundary-perf-r754/results/`.
- **Determinism.** port2 is SAT again with the identical 2,071,622 conflicts
  and 68,970,753 decisions. c2 reaches 2,387,696 conflicts by 300 s.
- **Same unit cost.** The flat profiles have the same shape:
  `kernel_propagate` is 27.0% / 25.7%, and analysis and minimization
  functions take a similar share. Throughput is 7.96k vs 8.27k conflicts/s.
- **No dead inprocessing pass in either arm.** The slot share is 32.2% /
  37.5%; Vivify and BVE are about 9% each in both. The only zero-output time
  is port2's 5.8 s structured startup, about 3.9 s of which is
  row/product refinement (an open item).
- **Conclusion.** The gap is search work to the solution: 2.07M conflicts
  against more than 43M. It is not unit cost. The pair does not separate the
  three pieces of the port (the extra pass-2 units, the AMO completion, the
  pure depth).

**896** (`20261007-satsuma-boundary-pair-r896`, job 1756772, node017):

| Arm | Result | Wall | Conflicts | Propagations | Decisions | SR |
|---|---|---:|---:|---:|---:|---|
| c2 | UNKNOWN (timeout) | 5000.0 s | 97,888,150 | 6.96 G | 182.3 M | 0 applications |
| port2 | UNKNOWN (timeout) | 5000.0 s | 89,695,988 | 6.36 G | 157.8 M | 0 applications |

- **Result.** Neither arm closes 896. This is expected: Satsuma itself does
  nothing on this row. port2 changes only the pure depth before startup
  FastEliminate (fastel eliminates 7,525 vs 7,595 variables).
- **Not attributed.** port2 has an 8% lower conflict count. No perf profile
  was run for this row, so under plan §8.3 that difference is not attributed.
- **Per-pass time.** It is similar in both arms (inprocessing slot 9.9% /
  10.2%, Vivify 424 s / 441 s).
- **Status.** It is an open item and does not decide anything here.

## Open items

- **Before port2 can be promoted.** The 754 closure is one instance and one
  draw (see `chaotic-instance-veto`). Promotion needs:
  - the shuffle probe on 754 and 1144;
  - the `calculus`/`rule-dev` work for the new repeat mode;
  - full Debug and effective fuzz;
  - the standing gates: the 57 Satsuma-owned rows, B28+C5, Rank 1-50, WV100
    and WV2-89.

  Pass 2's 179 extra units on 754 and 1144, and port2's 3.9 s of zero-output
  row/product refinement, both need a general explanation first.

- **896 belongs to M2 (Kissat 4.0.4 search parity).** Boundary parity cannot
  close it, because Satsuma itself does nothing on this row.
- **Pure round at stage entry.** Stages 2 and 3 each begin with an extra pure
  round. For fidelity, the pure step should run only at the start of an
  earned owner pass, which would remove the last 896 difference.
- **754/1144 large domain.**
  - Explain why port2's pass 2 fixes 179 units when Satsuma's iteration 1
    fixes none. Candidates: the AMO count gap (Tactix's completion criterion
    against `cnf.h:144-165`), and DejaVu limits in iteration 1.
  - The duplicates do **not** explain the AMO gap. Job 1756784 recounted
    Satsuma's ULC rule on the c2 snapshot, both with physical copies and
    deduplicated; both counts are equal (754: 535 ULC clauses, 716 binaries;
    1144: 536, 1,057). The gap must come from the state the completion sees,
    meaning pure depth and units, and from pass placement.
  - Port the duplicate-free clause set (`cnf.h:250-290`).
  - Port the owner's support-order scan (keep the hook support order next to
    the sorted `moved`).
- **Unexercised parity item.** `SP-SR-REPEAT-LIMIT` (`reached_limits`) still
  has no Tactix counterpart; none of these rows exercised it.
- **d170's 8 SR applications on 896.** Where these happen, outside startup,
  has not been traced.
