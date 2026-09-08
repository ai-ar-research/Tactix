# Root-entry completion and watch metadata ablation

## Decisions and current evidence

The [Rephase repair](2026-09-07-rephase-root-closure.md) remains isolated,
unfuzzed and unpromoted. This continuation adds two independently pinned
candidates rather than conflating a representation optimization with a semantic
phase-entry correction. The Rank 1–40 ≤1.01x objective is not achieved.
Production remains `c2a95a6`, checked 1.2787518x Kissat.

The preceding coupled full view
[`5395/summary.json`](../runs/20260907-5395-eliminate-entry-ranks1-40/summary.json)
is now **80/80 checked and clean**: both arms solve 40 (22 SAT/18 UNSAT), no
timeouts, 8,378.399/6,762.780 seconds = **1.238898648x**. Its last Kissat Rank-18
checker took 8,545.163 seconds, distinct from solver time. Continued work did not
wait for that certificate.

At this review the newer Rephase full view 5423 has only 38/40 paired solver
results. It confirms remaining large gaps: Rank 37 is 621.930/344.429 seconds,
Rank 32 455.035/274.665, Rank 31 421.190/264.148. Rank 33 improves to
357.743/310.681 and Rank 35 to 363.524/304.929. These are individual observations
from a partial full-view run, not a complete aggregate or paired cross-version
effect estimate. No failed proof/model is reported at the checkpoint.

## 5426: unchanged watch metadata need not be rewritten

`WatchHead` packs the exact binary count and a conservative binaries-first
flag into one word, using watch-list size parity for the count's low bit.
Appending or dropping a large entry changes size by two, preserving parity,
binary population and order flag. Nevertheless `truncate_large` reloaded and
rewrote the same encoded word after every full literal scan; delayed
`push_large` also reconstructed it after appending both words.

The immutable baseline assembly actually contains the redundant load/store,
at `kernel_propagate+0x360` and `+0x37e`; this is not inferred solely from C++
source. Candidate 5426 removes that metadata traffic from large-only append
and truncate. Truncate asserts an even removal count and enough retained words
for every binary. Arena growth changes placement/capacity, not binary metadata.
No layout, traversal, clause ordering, cursor, proof, guard or schedule changes.
Kissat and CaDiCaL likewise update their scan-end positions without this extra
Tactix-specific packed-count reconstruction; their layouts are not identical.

The exact old/new fixture exercises 392 actual-object cases: odd/even binary
counts, interleaving, every large-removal count, empty/no-op cases and repeated
arena growth. Encoded metadata, retained entries, size, holes, offset and
capacity agree. Both independently built Release binaries pass **78/78 tests**
and have SHA-256 `bb4e3fbc2cc59cb2fc9164c204c2497c0c6cacf518a218d0ab719a42015acab0`.
Their core propagation symbol shrinks from 3,786 to 3,581 bytes; machine-code
size is not a measured runtime improvement.

Source `.claude/worktrees/runtime-watch-truncate-5426`, patches, fixture outputs,
build/test logs and old/new assembly are owned by
[`5426/manifest.json`](../runs/20260907-5426-watch-truncate/manifest.json).
Runtime 5428 requires three matched candidate/exact-Rephase-predecessor pairs,
identical non-timing statistics and proof hashes, paired median/full range and
the unchanged 2% spread criterion. Gate 5429 is unchanged B28+C5 against
production. Neither has a runtime outcome yet; the optimization is not accepted.

## 5430: root closure is also missing at simplifier entry

Reviewing the other root-clearing boundaries exposed the same omitted operation
in `DefaultInprocessScheduler::run_phase`. Probe and Eliminate backtracked,
cleared `unflushed_root`, and constructed dense occurrences before propagating
the retained root suffix. This can change rule admission, watch pose, root
retirement and the effort clocks.

Kissat `probe.c::probe` and `eliminate.c::eliminate` explicitly call
`kissat_backtrack_propagate_and_flush_trail` first. Probe already has
`solver->probing` set, so entry propagation suppresses saved phases and search
ticks. Eliminate still uses ordinary search propagation before entering its
simplifier. CaDiCaL `Internal::elim` likewise backtracks and propagates root
before elimination; it has a different surrounding architecture.

The actual old scheduler fixture creates `(-1,3,4)`, temporarily assigns -1
above root, then assigns -3/-4 at root. A legal true blocker defers the large
scan. After the scheduler backtracks, -1 should be exposed as a root consequence
before any simplifier slot. Both old pipelines perform **zero scans** and leave
-1 unassigned. Probe visibly leaves propagation pending; empty Eliminate resets
the exit cursor past that suffix and reports a fixpoint without scanning it.
The fixture returns no SAT/UNSAT solve verdict and is not a reproduced invalid
model/proof or solver crash. Successful old-negative assertions retain the
mechanism-fidelity witness without weakening an oracle.

The repair restores existing Propagate after the root backtrack and before
dense construction/admission. A temporary Probe scope preserves its phase/tick
semantics; the common simplifier scope starts after root preparation so Eliminate
credits search ticks and saves newly exposed root phases. No public rule effect,
guard, instance surface, kernel primitive or proof vocabulary changes. Existing
Propagate emits the root-refutation proof and handles early UNSAT; both scope
flags are restored and no dense attachment remains on that exit.

The new actual scheduler fixture derives -1 at root after **three scans**:
Probe adds zero search ticks and preserves its saved phase; Eliminate adds six
search ticks and saves the new phase. Restart cadence is unchanged. Root-conflict
exit fixtures pass in both groups. Full tests pass **77/77 trace** and **78/78
in each independent Release build** (8.27/5.09 seconds for Release, 5.37 trace).
Release SHA-256 is `44cac3efc60149ab78dca2a5e6b17e552f2f860d4797226d6a0889ae93c1fe26`.

Source `.claude/worktrees/runtime-inprocess-root-5430`, old fixture, patches and
all logs are owned by
[`5430/manifest.json`](../runs/20260907-5430-inprocess-root/manifest.json).
Its parent is Rephase-only 5417: metadata 5426 is deliberately **not** included.
It is **UNFUZZED investigation material**, not a promoted or timed improvement.
The calculus and production guard ledger remain unchanged because the abstract
rules and production source did not change; the new composition is policy-owned.

## Active measurements and next decisions

| Campaign | Job | Question |
|---|---|---|
| 5421 | 1669878 | Finish checking all nine Rephase Rank-35 replicas. |
| 5422 | 1669812 | Finish Rephase B28+C5 checking; 64/66 at checkpoint. |
| 5423 | 1669887 | Finish Rephase full-view solver/checker array. |
| 5424 | 1669919 | Locate the next Rank-35 split through 100,000 conflicts. |
| 5425 | 1669961 | Clean Rephase/Kissat Rank-35 cycle profiles, same work per arm. |
| 5428 | 1669968 | Three exact-work metadata timing pairs. |
| 5429 | 1669969 | Metadata B28+C5, fixed membership. |
| 5433 | 1669971 | Phase-entry Rank-35 prefix versus native and Rephase-only. |
| 5434 | 1669972 | Phase-entry full Rank 1–40 versus pristine Kissat. |
| 5435 | 1669973 | Phase-entry B28+C5, fixed membership. |

All submitted sources, pins, runners and manifests are immutable. Arrays are
full-width with no throttle/node constraint, one bound solver per task,
12 logical CPUs/48,000 MiB, no address-space limit or whole-node reservation.
The profile wrapper samples user cycles at 199 Hz with DWARF stacks; sampled
times do not become acceptance timing. Ordinary campaigns independently check
every certificate/model; stopped prefixes are not checked final solves.

Use pending expected verdicts as provisional passes and revisit checks at
checkpoints, as requested. Rank 37/32/31 remain priorities if the full phase-entry
view retains their worse work. Use 5425 to select an execution-cost ablation,
not historical normalized-conflict claims of core parity. Inspect 5424/5433
prefixes with their explicit observers; never align the wrapping legacy
`start_decisions` field. Do not merge metadata until its exact-work comparison
supports it. Consolidation still needs one Claude-owned fuzz campaign; none was
launched or inspected here.

The queue is itself limiting iteration: long checks retain whole solver
allocations although all 36 preceding full-view proof checks peaked below
5,418,936 KiB (about 5.2 GiB). A non-blocking approval request proposes separate,
conservatively sized checker jobs for future measurements, with identical final
oracles and existing jobs untouched. `revise-plan` requires explicit approval
before changing the current single-task contract. No such change is made here.

The accompanying validation snapshot owns queue mapping, file hashes, result
status, residue scan, documentation checks and Andrew's twice-compiled report.
All retained campaigns/sources are needed by active work or decision-changing
evidence. No manual deletion is justified or performed; checker scratch remains
job-owned, and user allocation 1668242 is untouched. Living docs stay within
their budgets. Push remains unapproved after the earlier rejection; local
checkpointing does not bypass it.
