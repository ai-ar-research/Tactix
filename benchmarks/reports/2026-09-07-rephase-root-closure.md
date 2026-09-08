# Root closure before Rephase: a causal Rank-35 repair

## Outcome and authority

The isolated cumulative candidate now propagates retained root assignments
before DefaultPolicy changes phases or starts Walk warm-up. This is a general
policy-composition repair, not an instance selector or tuned constant. It closes
the previously measured first Rank-35 choice divergence. Production remains
`c2a95a6`; the candidate is **UNFUZZED and unpromoted**. The overall runtime
objective remains open.

Source, patch, build and fixed-test authority:
[`5417/manifest.json`](../runs/20260907-5417-rephase-root/manifest.json).
The isolated source is `.claude/worktrees/runtime-rephase-root-5417`.
Independent Release builds `build/pin-5419-rephase-root-release` and
`build/pin-5420-rephase-root-bc` have identical SHA-256
`f86419c4763ea5a8372661648d552c6b47c7b3375441ebc879fe9088f171c10c`.
Trace, conflict-stop, shadow and journal instrumentation are disabled in both.
The authority retains the small repair relative to observer 5414 and the
cumulative source patch relative to production; submitted sources remain frozen.

The [previous checkpoint](2026-09-07-async-check-warmup.md) owns the common
warm-up entry heap and original divergence. The
[runtime review](2026-09-07-runtime-review.md) owns the audit of earlier work;
the [dense-entry report](2026-09-07-dense-entry-cost.md) owns the retained
memory improvements and their mixed downstream runtime evidence.

## Causal chain, not a heap tie-break patch

At conflict 13,563, both solvers begin warm-up with exactly the same complete
heap. Campaign 5415 selects implications of the first warm-up decision, 145:

| Order | Previous Tactix | Kissat |
|---|---|---|
| 1 | -109796, ternary reason | -109815, binary reason |
| 2 | -109815, binary reason | -109877, binary reason |
| 3 | -109877, binary reason | -109796, ternary reason |

The literals, reasons and assignment levels agree; their insertion order does
not. Subsequent warm-up backtracking reinserts heap variables in that differing
order. This is not evidence that the heap implementation should sort ties.

Campaign 5416 rejects the hypothesis of an already-different selected watch
list at Rephase entry. At conflicts 2,002, 8,033 and 13,563, every emitted large
watch on -145 has identical ordinal, blocker, size, searched position, clause
set and pose: respectively 20, 1,824 and 129 rows. The target ternary is not
yet watching -145 at any of those entries.

The source discrepancy is earlier root closure. Kissat's `kissat_rephase`
calls `kissat_backtrack_propagate_and_flush_trail` before phase changes.
CaDiCaL's `Internal::walk` also backtracks and propagates root assignments
before walking. Tactix previously backtracked and cleared `unflushed_root`
without propagating the retained root suffix. A root assignment scanned while
a transient higher-level blocker was true needs rescanning after backtracking.
Here, scanning retained -109797 only after decision 145 directly implies the
ternary's -109796 before scanning 145's binaries. Propagating root first moves
the ternary watch before that decision, restoring the reference order.

The repair composes the existing Restart and Propagate services in
`DefaultPolicy::prepare_rephase_root`. A root conflict follows the existing
Propagate proof path; successful preparation asserts a full propagation
fixpoint. No kernel primitive, proof vocabulary, public guard or abstract rule
effect changes. Public Rephase remains heuristic-only and available mid-trail;
the scheduling prerequisite belongs to DefaultPolicy, not the public rule.
Consequently the calculus and production guard ledger need no semantic change.

## Independent confirmation and tests

The actual-object regression in `tests/test_rephase.cpp` constructs a retained
root assignment with a legal cached transient blocker. The old composition
produces `[-2,-4,-5]`; the repaired policy produces `[-4,-5,-2]`, reaches a root
fixpoint, and leaves the canonical restart counter unchanged. Both paths are
successful executable assertions, not an intentionally crashing negative test.
A separate assertion preserves public Rephase's mid-trail capability.

[`5418/summary.json`](../runs/20260907-5418-rank35-rephase-confirmation/summary.json)
confirms all six selected implication rows and every portable field of all
429,257 post-warm-up heap entries now match Kissat. The complete decision prefix
matches through conflict 13,833: 18,516,218 event rows, hash
`a6eb1df4f4e0e591`, including the former first split at conflict 13,832.
The narrow decision stream is byte-identical, and the native prefix reproduces
the independent earlier authority. This closes this witness, not the entire
trajectory.

All fixed tests pass: 76/76 in the repaired trace build, and 77/77 in each
independent Release build. Observer builds 5412 and 5414 each pass 76/76.
The first 5412 test run failed only a static textual expectation for an existing
observer guard; retaining that guard and adding a separate early return fixed
the expectation without weakening the test. The new implication-only observer
fixture verifies selected-window output and exclusion of unrelated rows.
No soundness defect or solver crash was discovered, and no new fuzz campaign
was launched or its artifacts inspected. Consolidation fuzz remains mandatory.

## Measurement and asynchronous validation

Campaign 5421 (`1669878`) measures three independent matched Rank-35 replica
blocks: candidate, exact preceding coupled candidate, and pristine Kissat.
The [frozen solver checkpoint](../runs/20260907-5421-rephase-root-rank35-runtime/solver-checkpoint.json)
owns all nine raw-result hashes, paired medians/ranges, per-arm spread, node
placement, work and memory. Pending
proofs are explicitly provisional; final gate closure requires checked/clean
records. Replica blocks are all scheduler-eligible concurrently, not serialized.

All nine solvers returned expected UNSAT, without timeout. At the solver
checkpoint all nine certificates were still checking. Median paired
candidate/previous ratio is **0.658243** (range 0.652884–0.659718), a provisional
**34.2% reduction**. Median times are 364.633 / 553.949 / 308.005 seconds for
candidate / previous / Kissat. Within-arm ranges are 0.196%, 0.848% and 0.212%,
each below 2%. Median paired candidate/Kissat is 1.183228 (1.181526–1.185729).
Candidate conflicts are 4,297,466 versus Kissat's 4,751,990; decisions are
194,380,570 versus 210,767,815. This is a downstream work/trajectory improvement,
not proof that BCP became cheaper or all logical work is comparable. Candidate
peak RSS is 589,544 KiB versus Kissat's 269,360 KiB. The remaining execution-cost
and memory gaps are substantial. Final affinity/service-rate context will
publish with checked records; provisional records expose only node placement.

Campaign 5422 (`1669812`) runs the unchanged B28+C5 matrix against production.
Campaign 5423 (`1669887`) runs the unchanged complete Rank 1–40 matrix against
pristine Kissat. Both use immutable Release pins, proof-on solver timing and
independent certificate checking. All tasks request 12 logical CPUs and
48,000 MiB, with one bound solver, no array throttle, node constraint, whole-node
reservation or address-space limit. Check time is separate from solver time.
Memory competitiveness is not inferred from a runtime improvement.

Campaign 5424 (`1669919`) extends the repaired Rank-35 decision comparison to
100,000 conflicts, using narrow decision streams and macro boundaries without
the implication/heap payload. It runs while proof checks remain pending; this
implements the user's asynchronous continuation request. It is a stopped
diagnostic, not a checked final solve or acceptance timing.

The preceding coupled full view 5395 has all 80 expected solver verdicts,
aggregate 8,378.399 / 6,762.780 seconds = 1.238899x; its remaining certificate
does not block investigation. The final checked production ratio remains
1.2787518x. Do not compare those different windows as a causal paired test.

## Observer cost, cleanup and handoff

5411 (`1669628`) was deliberately cancelled because output-only implication
filtering retained costly unrelated payload construction. Both task handlers
reported scratch deletion; the exact node-local paths were independently
checked absent. 5413 (`1669643`) timed out at 600 seconds before the requested
window. Its exit 124 is a timeout, not a solver crash or accepted result.
Both temporary directories were deleted automatically, including partial
certificates and decompressed inputs. Their known removed total is 86,366,503
bytes; retained result records preserve partial-proof hashes and sizes.
The corrected 5414 observer rejects unrelated heap helper calls early;
5415 and 5416 then reach their intended stops and delete scratch normally.

The 5415 comparator normalizes one additional native analysis-boundary row
caused by the observer inventory; it asserts that exact extra row and retains
the raw traces. Complete decision/prefix identity, not coarse boundary row
counts, validates trajectory neutrality. No ordinary propagation-call overhead
repair is claimed: inspection did not establish that proposed cost source.

Preserve all submitted pins, sources, runners, partial traces and manifests.
No additional manual deletion is needed; active checker scratch remains owned
by its job. The user allocation `1668242` is untouched. Current artifact hashes,
SLURM mapping, tests and residue inventory belong to the accompanying validation
snapshot. Large raw streams remain local with their hashes; small source patches,
launchers, comparators and witnesses are durable.

Next: compare the full new runtime view, inspect 5424's next divergence, and
prioritize remaining worse-side work/cost gaps (including Rank 37). Revisit
certificate outcomes at checkpoints without waiting for them to begin dependent
work. A rejected proof/model or actual soundness/crash failure pauses affected
work under plan §8.2. Consolidate only supported general repairs and then run
one Claude-owned fuzz campaign. No production promotion or push is authorized
by a provisional result; the earlier push rejection remains in force.
