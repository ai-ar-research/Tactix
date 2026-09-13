# Progress

[DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) owns stable scope and acceptance
criteria. This file is the compact resume anchor; Git, run manifests and frozen
reports own detailed history.

## Current objective

The online-configurable strategy/N1 refactor is accepted at `a08f9f6a`. All
inprocessing actions, Lucky and Walk operate at level zero; a search-time caller
uses an explicit `Restart` before them. The shared typed executor supports legal
instances and granularities, arbitrary composition, finite or externally
stopped fixpoints, and safe online reconfiguration without restarting the
solver.

The current working-tree increment makes native DSR the canonical whole-solver
proof output. Ordinary CDCL and inprocessing additions/deletions and later
substitution-redundancy transformations share one stream; `dsr-trim` elaborates
it to LSR and the pinned Trestle checker verifies that LSR. DRAT and native LRAT
remain compatibility modes. The pending SR/orbitopal implementation is
preserved in stash `pre-native-dsr-sr-increment-2026-09-13`; its DRAT/LRAT
translation is discarded when restored.

After native DSR passes B27+C5 and Rank 1–40 without a material runtime or
coverage regression, Phase 4.25 resumes the Satsuma symmetry-breaking rule and
then the checked 100/100 WV100 target. No new full Tier-D run is scheduled at
the DSR boundary. Kissat-only tuning, benchmark dispatch and sequential
portfolios remain out of scope; whether MAB scheduling belongs in Tactix is
still open.

## Authority

| Role | Authority |
|---|---|
| Refactor base | `0060215a` (`Keep inprocessing and Walk at root`) |
| Initial closure | `b1165641` |
| Accepted source | `a08f9f6a`; Release SHA `b63f30031229a36b9fea46e37590e892ab4a178a1239357ff3f80557548e1413` |
| Design | [online proposal](docs/design/2026-09-11-online-strategy-proposal.md) and [preservation addendum](docs/design/2026-09-11-strategy-behavior-preservation-addendum.md) |
| Final evidence | [online refactor/N1 closure](benchmarks/reports/2026-09-13-online-refactor-n1-final.md) |
| Claude checkpoint | [job 1687182 report](benchmarks/reports/2026-09-13-structural-resume-claude-fuzz/terminal-report.md) |
| DSR B27+C5 reference | [fixed 32-row vector](benchmarks/baselines/b27c5-tactix-a08f9f6a-v1/manifest.json) |
| DSR Rank1–40 reference | [accepted Tactix vector](benchmarks/baselines/rank1-40-tactix-a08f9f6a-v1/manifest.json) |
| Winner target | [fixed WV100 set](benchmarks/tiers/WV100.tsv): 33 SAT / 67 UNSAT |
| WV100 protocol | [mechanism integration and experiment design](docs/design/2026-09-13-wv100-mechanism-integration.md) |
| Satsuma transition | [57-row structural screen](benchmarks/reports/2026-09-13-wv100-satsuma-transition.md) |
| Orbitopal G0 | [structured SR obligation classification](benchmarks/reports/2026-09-13-wv100-orbitopal-sr-g0.md) |
| Symmetry G2 | [structured automorphism schedule boundary](benchmarks/reports/2026-09-13-wv100-structured-symmetry-g2.md) |

The root checkout is authoritative. `.claude` contains workflow instructions,
not implementation or project-document changes.

## Implemented state

- `DefaultPolicy` owns cohesive typed branching/analysis, restart/mode,
  database, guidance and inprocessing settings while preserving static hot
  dispatch and the reference schedule.
- The C++/C surface covers all 26 registered actions with validated per-call
  parameters, explicit instances, selected batches, passes, rounds, closures,
  reordered and repeated sequences, scoped fixpoints, budgets and stops.
- Complete N1 surfaces include C1/C2 victims, CCE large and duplicate-binary
  victims, HyperBinary/Ternary resolvents, exact Factor grids, and individual
  Congruence equivalences, units and supported clashes.
- Every scheduling, admission, effort, search and heuristic value is strategy
  data. Rule modules retain soundness, representation and diagnostic constants.
  Static inventory, wiring, ownership and reference-option audits pass.
- `--random-policy` generates typed online programs through the shared executor.
  It varies order, repetition, settings, granularity, budgets, phases and
  non-prefix instances; trace v3 is replayable and historical v1/v2 traces
  remain readable. The legacy random scheduler is removed.
- Root propagation closes between independent probes. Sweep and BVE embedded
  solves poll external stops across finite and unlimited budgets without
  consuming interrupted work.
- Lucky 6 and 7 provide bounded, configurable GF(2) and one-hot GF(3) structural
  model search. They publish SAT only after checking the complete live CNF.
- Online model export reconstructs `model_values` without changing trail,
  assignment or unassigned counters. Terminal active-variable assignments are
  reflected by `tactix_status` and remain safe across later API calls.
- Obsolete solver configuration macros, superseded compatibility branches and
  completed diagnostic probe artifacts are removed. Future probe output remains
  ignored under `benchmarks/probes/scratch/`.

## Validation

- Native binary/ASCII DSR serialization, direct typed `add_sr`, CLI/C API
  selection, and the DSR -> LSR -> Trestle checker pipeline are implemented in
  the working tree. Exact proof-byte/API tests and 53 proof-oracle/runtime/fuzz
  protocol tests pass. Slurm job `1688419` passes the complete micro suite in
  binary/ASCII DSR and explicit DRAT/LRAT modes, including a non-vacuous SR
  witness and malformed DSR/LSR controls. Jobs `1688422` and `1688423` pass all
  148 Release and all 147 applicable ASan/UBSan tests.
- The benchmark, anonymous-runtime and fuzz launchers now use the pinned DSR
  pipeline by default; explicit DRAT/LRAT compatibility modes remain covered.
  The increment is not landed yet. Its remaining acceptance work is the
  Claude-owned native-DSR fuzz checkpoint and the paired B27+C5 and Rank 1–40
  DSR/DRAT performance gates.

- Release: **148/148** tests. Sanitizer-enabled Debug: **147/147** relevant
  tests; the explicitly long scripted demo was stopped after 1:48.
- Claude job `1687182`: **PASS**, one full non-array checkpoint, 0 solver
  failures, 0 missing features, 4,612/4,612 identical random replays, 2,833
  DRAT proofs, 2,406 native LRAT proofs, valid models throughout, and clean
  interruption, negative-control and ASan/UBSan lanes.
- The checkpoint closes all S1 one-hot model, S2 propagation and S3 online
  resume failures from job `1687094`; all 646 mode-5 cases and 866 repeated
  online model exports passed.
- B28+C5 `20260913-0825-structuralfix-gate-BC`: **33/33** expected solver
  outcomes, 950.837 seconds versus 948.484, **1.002481x**.
- Rank1–50 `20260913-0825-structuralfix-gate-ranks1-50`: **50/50** expected
  solver outcomes, 8,123.812 seconds versus 8,041.173, **1.010277x**.
- All 83 certificate hashes, non-timing statistics and peak-RSS values match the
  preceding candidate. B28+C5 final checks are complete; remaining Rank proof
  checks continue in the background without blocking independent work.

## Active operations

- Tier-D validation job `1687183` passed SAT/model, UNSAT/DRAT, forced-crash and
  forced-timeout cleanup. Array `1687184` completed all 1,166 instances from
  `a08f9f6a` on `cpu-q`, four CPUs, 32 GiB and a 1,200-second solver timeout:
  407 SAT models and 361 ordinary UNSAT proofs checked, two further trivial
  UNSAT proofs printed `s VERIFIED` under drat-trim's exit-1 convention, nine
  proof checks exhausted the 16,000-second checker allowance, and 387 solver
  runs timed out. Every task deleted its anonymous formula, proof, model and
  checker artifacts. The immutable run is
  [`20260913-0905-tierd-refactor-final-a08f9f6a`](benchmarks/runs/20260913-0905-tierd-refactor-final-a08f9f6a/).
- Existing proof-checking jobs remain unchanged. Do not rerun retained reference
  solvers or wait for proof checking before independent work. Do not submit a
  new full Tier-D run after the native-DSR gate.
- Native-DSR gates `1688424` (Rank 1–40, 80 paired tasks) and `1688455`
  (B27+C5, 64 paired tasks) run at full width on `cpu-dense-hwulab-q`, c12,
  32 GiB and 1,200 solver seconds. Each instance runs the same binary once in
  default DSR mode and once with explicit `--drat`; retained reference solvers
  are not rerun.
- Claude-owned native-DSR fuzz checkpoint `1688465` is one non-array job. It
  covers default and online-random DSR, DRAT/LRAT controls, effective
  transitions, replay, negative controls and interruption accounting; Claude
  owns its terminal audit and report.
- The completed Tier-D run gives current Tactix **17/100** effective WV100
  coverage: 15 independently checked SAT models and the two trivial UNSAT
  proofs at ranks 68 and 74, which solved in 0.367 and 1.319 seconds and printed
  `s VERIFIED` under drat-trim's exit-1 convention. Rank 90 also solved UNSAT,
  but its proof check exhausted the 16,000-second checker allowance and does not
  count. The shared oracle is repaired for future pins; derived scoring recovers
  only the two retained `s VERIFIED` cases from the immutable result records.
- Storage guard PID `1458620` checks the complete repository every ten minutes.
  Above 700 GiB it holds only pending jobs whose Slurm `WorkDir` is this repo;
  it never pauses running jobs and releases only its own holds. The submission
  check was 114 GiB with no holds.
- WV100 permanently freezes the first 100 rows of the all-configuration ranking,
  each solved within 1,200 seconds and 32 GiB by exactly one checked winner:
  34 LymphoSAT, 57 Satsuma+Kissat, seven AE-Kissat-MAB and two kissat-sc2024.
  Its denominator does not change when Tactix solves an entry. The earlier
  10+10 matched run remains a checked pilot, not the active target definition.
- A 20-instance Satsuma pilot reduced every formula to an empty clause in at
  most 50 ms. Independent SR-to-DRAT-to-LRAT checking passed 19; the Johnson
  case exposed a translator failure. This supports a general substitution-
  redundancy/proof study but does not yet establish a Tactix rule.
- The complete 57-row Satsuma transition screen returned normally on every row:
  52 became immediate contradictions, with 51 structured, five Johnson and one
  general priority class. This is causal-attribution evidence only; G0--G2 must
  close per producer before S1 mutates Tactix.
- Satsuma G1 now has an independently derived native-LRAT prototype. The
  official 39-record `R_18_4_4` example expanded to 888,337 exact proof
  operations and 47,445,835 bytes; pinned `cake_lpr` verified it in 3.21 seconds
  and rejected the terminal-empty negative control. This remains evidence that
  the semantics can be translated, but the expansion is no longer the
  implementation route.
- Structured-symmetry G2 is closed for the first producer. Across PHP Sudoku,
  clique colouring and channel routing, every one of 173 formula-derived,
  full-multiset-validated generator-basis entries survives through the second
  `Backbone`; `Factor` is the first breaking boundary. The initial default slot
  must precede Factor. Johnson/general G2 follows their producers.
- Orbitopal G0 is closed for the first structured producer. Its 281 ordered unit
  applications classified 14,212,861 SR image obligations as direct originals,
  prior symmetry units, RUP discharges, or witness-satisfied cases with zero
  unresolved; independent Tactix root propagation confirmed both contradictions.
  Johnson/general G0 remains scoped to those later producers.
- Focused WV100 development defaults to `cpu-dense-hwulab-q`, eight CPUs,
  32 GiB and 300 solver seconds; qualitative `cpu-q` probes are allowed.
  General B28+C5 and Rank 1--50 checks retain the stable
  `cpu-dense-hwulab-q`, 12-CPU, 32-GiB, 1,200-second contract. The native-DSR
  landing gate uses B27+C5 and Rank 1--40 with those same resources.

## Next actions

1. Leave the existing proof checks and storage guard unchanged in the
   background; submit no new Tier-D run.
2. Complete the Claude-owned native-DSR fuzz checkpoint; Codex owns any repair.
3. Run B27+C5 and Rank 1--40 at
   c12/32 GiB/1,200 s with a same-source DRAT control.
4. Land native DSR only after its checker, cleanup, correctness, sanitizer,
   fuzz and performance gates pass.
5. Restore the pending SR/orbitopal work without its DRAT/LRAT translator and
   finish the general Satsuma symmetry-breaking rule using direct DSR.

<!-- track-sched:begin -->
- Kissat creation order remains the production default; `TACTIX_ARENA_ORDER=1`
  is a CaDiCaL diagnostic arm. Unsupported formula-dependent profiles stay removed.
- Retain sound general repairs through downstream regressions; preserve N1–N4.
<!-- track-sched:end -->

## Recent changes

- 2026-09-13: Implemented the native DSR writer and pinned three-stage checker
  pipeline across active launchers; fuzz and fixed performance gates remain.
- 2026-09-13: Preserved B28+C5 and Rank1–50 trajectories within 1.03% aggregate time.
- 2026-09-13: Completed Tier-D array `1687184` with 17/100 effective WV100 coverage and clean anonymous artifacts.
- 2026-09-13: Located the structured symmetry slot before Factor with exact generator survival.
- 2026-09-13: Classified the first orbitopal producer's SR obligations with zero unresolved.
