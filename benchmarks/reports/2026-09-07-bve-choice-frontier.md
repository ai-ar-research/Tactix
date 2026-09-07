# Rank-34 decision split: elimination relevance precedes BVE

## Outcome and scope

No production solver change or new speedup is claimed. Four paired stopped
diagnostics localize the next isolated-candidate divergence upstream of BVE's
candidate loop: initial occurrence counts and candidate admission agree, but
numeric relevance values already differ. Trace their stamping/compaction
producer next; do not patch decision-heap selection or dynamic BVE updates to
hide this input discrepancy.

The objective remains Rank 1–40 aggregate runtime at most 1.01x Kissat, with
B28+C5 regression gates and N1–N4. Final production gate
[`5314`](../runs/20260907-5314-pose-r2-ranks1-40/summary.json) remains
80/80 independently checked and clean, 40 solves per arm (22 SAT/18 UNSAT),
8643.178/6759.074 seconds = **1.2787518x**. The goal is unmet.
[`5315`](../runs/20260907-5315-pose-r2-BC-gate/summary.json) remains 66/66
checked/clean; final-r2/starting aggregate time is 1.01897x.

The earlier [runtime review](2026-09-07-runtime-review.md) owns the assessment
of Claude's work, withdrawn mixed-proof-mode attribution, and accepted fixes.
[Original-core convergence](2026-09-07-original-core-convergence.md) owns
5316–5336; [duplicates and retirement](2026-09-07-reference-duplicates-and-retirement.md)
owns 5337–5347. Their isolated candidates remain unfuzzed, not promoted.

## Provenance and diagnostic contract

| Campaign | Job | Question / result |
|---|---|---|
| [5348 choice state](../runs/20260907-5348-choice-state/summary.json) | 1668712 | Is 5416 assigned or unavailable when Tactix skips it? Unassigned, but missing from its heap. |
| [5349 deactivation](../runs/20260907-5349-choice-deactivation/summary.json) | 1668714 | Which producer removes it? Tactix retires it during Eliminate at conflict 95799. |
| [5350 BVE candidates](../runs/20260907-5350-bve-frontier/summary.json) | 1668716 | First candidate difference? Tenth pop, after nine exact candidates/outcomes/ticks. |
| [5351 initial schedule](../runs/20260907-5351-bve-schedule/summary.json) | 1668718 | Does the phase-entry heap already differ? All scores and 262 positions differ; counts and insertion order match. |

Each full-width array has two tasks: one proof-on stopped solver per task,
12 logical CPUs, 48000 MiB, one bound thread, partition
`cpu-dense-hwulab-q`, no throttle, node constraint or address-space limit.
Both arms use the same compressed Rank-34 input (SHA-256
`ae23202ebf6de47a653fb74430a7603e155a321f2826929864e9bfb69c41ebbd`).
Manifests freeze input, patches, binaries, runner hashes and exact commands.

All four campaigns reuse the same immutable observer binaries:

- Tactix `2cfc46e5475a44124914875b0956fa15d88bfc797a74e9264183c89bd66e7c9a`;
- Kissat `117835ad5b358aa4b145d95e252859e091db678555489137b9320b09971f0e52`.

Tactix source is the cumulative 5343/5347 ablation plus read-only selected-state
observation in `.claude/worktrees/runtime-choice-state-5348`; its fresh build
is `build/pin-5348-choice-state`. Kissat's fresh source is
`tools/solvers/variants/kissat-choice-state-5348`, based on pinned native
`8af8e56f174b778aef3aa45af9f739b2a5f492c2` and retained observer overlays.
The old manifest `release_sha256` describes ancestral bridge Release, not
these trace binaries: `hashes.pins/*` and the patch chain are machine identity.
No submitted source or build was mutated.

These are deliberately stopped diagnostics, **not SAT/UNSAT verdicts or
performance gates**. 5348–5350 stop at the first checkpoint beyond conflict
95801; 5351 exits before the first candidate of BVE ordinal zero at 95799.
All eight tasks have exit 0 and verified runner scratch deletion. Partial
proofs and decompressed inputs are deleted on exit. There is no new fuzz leg;
Claude's prior terminal report remains the sole fuzz authority.

## Decision state and removal producer

5347 had matched 254416 decisions from startup (253197 ordinary) before the
first split at conflict 95801, ordinary decision 253198, level 672:
Tactix `18849`, Kissat `-5416`. Native Kissat next chooses `18849` one position
later. Cumulative FNV verification in 5347 validates the complete streams;
legacy `start_decisions` metadata is unsuitable for alignment because of
warm-up-inclusive count subtraction.

5348 observes variable 5416 **after each chosen decision is assigned, before
that decision propagates**. Before the split both arms have it unassigned and
EVSIDS score 9.25. Its position is absent (`UINT32_MAX`) in Tactix and 6926 in
Kissat. At the split Kissat assigns `-5416` as a decision, while Tactix still
leaves it unassigned. Preceding propagation did not force it out of the choice.
Both scalar decision streams exactly equal 5347's prefix through 95801.

5349 leaves decision, state and prefix files byte-identical. The in-heap
deactivation streams match for their first 1554 records. At 95799, after
19272 and 2982, Tactix next removes 3166 while Kissat first removes 22171.
Later Tactix alone records
`DEACT 95799 5416 elim=1 fixed=0 inheap=1 trail=4294967295`.
Here Tactix's `elim` is kernel retirement, not a lazy heap pop. The BVE
candidate journal confirms elimination is the producing mechanism; observing
22171 later in both streams also rules out simply missing that candidate.

## Earliest BVE boundary

5350 preserves every prior decision/state/prefix/deactivation file byte for
byte. Compare only symmetric **begin/end** stages: Kissat emits `postflush`
after a NoOp whereas Tactix emits it only after an applied elimination.
Treating this observer-coverage difference as semantic divergence is wrong.

The first nine candidates, outcomes and exact entry/exit ticks agree.
Candidate 9 (one-based) is 8394, ending at 1351960 ticks. Candidate 10 begins
at those same ticks and the same 13142131 limit, but chooses Tactix 4387
versus Kissat 16887. There are 15447/15386 complete observed attempts across
the selected conflict; later mismatched work cannot diagnose the first cause.

5351 freezes the schedule before **any** candidate. Both have bound 4,
13003 active scheduled variables, 252421 ordinary decisions, 83620 irredundant
clauses, 16444 redundant clauses and 2445 binaries. Every scheduled identity,
positive/negative occurrence count and insertion-order hash matches. However,
all 13003 scores and 262 physical positions differ. Equal aggregate clause
counts do not establish complete formula/pose identity; physical variable and
root-trail counts are representation-specific and are not aligned here.

Both sources compute the same max-heap score, with occurrence limit 2000:

`score = 2000² - (min(pos,2000)*min(neg,2000)-min(pos,2000)-min(neg,2000)) - relevance`.

The comparator inverts that formula, without rounding, to recover relevance.
All 13003 variables have the same **relative relevance order**, but unequal
numeric spacing. The native log's last mode transition is `{` at 83003;
`mode.c::switch_to_focused_mode` identifies that as focused entry, and
`eliminate.c::variable_score` uses `LINK(idx).stamp` in focused mode.

| Variable | Positive / negative | Tactix relevance | Kissat relevance | Tactix / Kissat heap score |
|---|---:|---:|---:|---:|
| 18548 | 4 / 5 | 13 | 1 | 3999976 / 3999988 |
| 4387 | 8 / 8 | 34 | 5 | 3999918 / 3999947 |
| 16887 | 6 / 6 | 65 | 28 | 3999911 / 3999948 |
| 5416 | 8 / 6 | 5843 | 2778 | 3994123 / 3997188 |

Numeric gaps reverse the relative BVE priority of 4387 and 16887 even though
their relevance ordering agrees. This is stronger evidence than a later
decision tie, but the **upstream stamp producer remains an inference**.
Do not declare a specific missing reset repaired without a common-boundary
trace and differential fixture.

## Next action and limits

Capture mode, queue order, numeric stamps and cursor at startup, admitted
Reduce compaction and restamping boundaries. Native
`compact.c::compact_queue` renumbers surviving queue stamps and maps its search
cursor; Tactix `heuristics_compact_vars` also renumbers and maps, while its
startup `heuristics_compact_random_order_fixed` filters the random index and
phase counts without restamping. This is a concrete source-backed lead, not
proof that startup alone explains the later state. Trace the first unequal
producer, including admission and root/deactivation timing.

A general fix must preserve the numeric relevance consumer as well as VMTF
order and cursor. Do not inject native values or benchmark-conditioned resets;
do not modify BVE's occurrence formula or heap updates to compensate. Fresh
semantic candidates need fixed negative/positive tests, immutable builds,
checked cumulative gates and Claude-owned fuzz before promotion.

## Validation, documentation and hygiene

Each campaign's assertion-bearing `compare.py` reproduces its `summary.json`.
The scoped 5351 artifact audit verifies pin/runner/patch hashes, terminal
cleanup, prior-observer neutrality and absence of certificate residue for
5348–5351 only. Large raw streams and binaries remain local with recorded
hashes; patches, manifests, comparators, logs and small witnesses are retained.
No manual deletion is warranted: every campaign carries unique evidence.

Production remains unchanged. The complete Release test suite and document
checks are rerun at this checkpoint; their terminal results are recorded in
5351's validation record. Explicitly requested compaction replaces PROGRESS's
campaign chronology with primary-evidence links and the exact next action.
No new archive duplicates evidence already owned by reports, manifests and Git;
pre-compaction text is recoverable from `6e2d609:PROGRESS.md`.

All normal campaigns are terminal. Only user interactive allocation 1668242
remains. Changes are committed locally; pushing shared `origin/main` still
requires explicit user approval after the previous auto-review rejection.
