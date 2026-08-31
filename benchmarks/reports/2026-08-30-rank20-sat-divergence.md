# Rank 20 SAT divergence — 2026-08-30

## Resume anchor

The final homogeneous candidate solves Rank 20 in 594.433 seconds versus
pinned Kissat's 140.142 seconds (4.24165x). Tactix reaches 555,407 conflicts,
6,831,181 decisions, and 4.85B propagations; Kissat reaches 81,993 conflicts,
1,229,783 decisions, and 0.88B propagations. Direct Factor time is 45.260
versus 7.320 seconds, so direct cost explains only a small part of the gap.

Both solvers perform a similar number of factorizations (537,579 versus
493,961), but the downstream states differ sharply: Tactix later records
866,939 BVE eliminations and 181.9M resolutions, while Kissat records 21,227
eliminated variables. This motivates a first-common-state comparison rather
than a Factor-off arm or a formula-specific strategy.

## Active diagnostic

Campaign `20260830-2540-tierd-rank20-prefactor-state`, job `1657022`, contains
exactly two tasks: current Tactix trace and pinned Kissat trace. Each exits at
the first pre-Factor boundary without running Factor or making a SAT/UNSAT
claim. It records canonical external-literal multisets for live large clauses
and binary copies plus a compact boundary trace. No node constraint, rank gate,
production option, comparator rerun, or proof artifact is involved.

Decision rule:

- exact pre-Factor state moves the owner into Factor selection/application;
- unequal pre-Factor state moves the search to the earliest preceding common
  preprocessing boundary;
- a count/hash mismatch is expanded only at that boundary, without launching
  a broad trace or homogeneous campaign.

Kissat completed its boundary task in 20.01 seconds. Its pre-Factor state has
1,011,733 live large clauses (canonical content SHA
`72a6d1ab...19d3a25`) and 12,553,619 live binary copies (canonical SHA
`1d1d3f72...5b9a66`). The Tactix task hit the 600-second observer cap while
exporting the full 13.5-million-row surfaces. Scheduler accounting shows
09:55 CPU and 644,064 KiB RSS, so this is observer cost, not a launch failure;
it makes no semantic or production-runtime claim. The replacement extends the
already symmetric compile-time formula macro with constant-output commutative
binary-copy and root-assignment fingerprints, then reruns one paired boundary.

That replacement is campaign `20260830-2550-tierd-rank20-prefactor-summary`,
job `1657024`. Focused observer/overlay/zero-overhead/hygiene tests pass 5/5;
the Release binary remains byte-identical at SHA `d304e8e0...a691f0e5`.

Job `1657024` also hit the 600-second Tactix observer cap: the compact macro
was still evaluated at every retained preprocessing boundary. Its completed
Kissat trace is valid but unpaired. A symmetric boundary-match selector now
limits the linear scan to exactly one requested snapshot. Selective campaign
`20260830-2560-tierd-rank20-prefactor-selected`, job `1657026`, is the paired
replacement; no broader or production run is active.

Source audit then proved that `factor_begin` in Kissat precedes the internal
`factordelay` admission, while Tactix's temporary exit was inside
`factor_apply`, after its scheduler had admitted the call. Rank 20 declines the
startup call, so the two boundaries were not comparable; only the Tactix task
was canceled. A generic tested exit-after-matched-event hook now stops at the
Tactix slot boundary before admission. Campaign
`20260830-2570-tierd-rank20-pre-factor-boundary`, job `1657028`, runs exactly
one Tactix task against the preserved valid Kissat trace.

## Exact startup boundary result

Job `1657028` completed in 32.58 seconds with 890,420 KiB maximum RSS. At the
common pre-admission Factor boundary, the two solvers match exactly on the
semantic clause population:

- 1,011,733 irredundant and zero redundant large clauses;
- 12,553,619 binary copies;
- irredundant hash `295fd0162dec85d7`, redundant hash
  `0000000000000000`, and binary hash `93d775c687214058`;
- 1,591,244 unassigned variables, zero conflicts and decisions, and 3,888,736
  propagations.

The root counters are not normalized to the same physical representation.
Tactix retains all 1,009 root assignments, while Kissat has compacted away
1,008 parse-fixed variables and retains one internal root value. The exact
external clause/binary hashes and equal unassigned population rule out a
semantic formula mismatch here.

Both solvers then decline this startup Factor offer. Pinned Kissat admits when
`floor(log10(active)) <= eliminations + 4`; Tactix's equivalent integer
predicate admits below `10^(bve_rounds + 5)`. At roughly 1.59M active variables
and zero elimination rounds, both reject. Startup preprocessing is therefore
cleared. The next owner is the first admitted Factor call and its resulting
state, not an earlier preprocessing rule or a formula-specific admission gate.

The generic matched-event exit now owns boundary termination. The temporary
solver-specific `TACTIX_EXIT_PREFACTOR` and `KISSAT_EXIT_PREFACTOR` hooks were
removed so this investigation does not accumulate one-off diagnostic paths.

Campaign `20260830-2580-tierd-rank20-first-factor-admitted`, job `1657029`,
contains exactly two tasks and snapshots the first call that passes both
`factordelay` and the cumulative marked-material watermark. Each exits before
Factor mutates formula or representation state. The observer is symmetric,
compile-time-only, and Release remains byte-identical; this is the narrow test
of whether search/elimination scheduling has already separated the solvers by
the first admitted call.

That first attempt was canceled as observer-invalid before producing a usable
row. Kissat's retained initialization stream exhausted the row cap before the
later target; the old matched-exit check lived after the cap and could never
fire. The shared trace layer now caches the requested selector, offers a
target-only fast path before the ordinary filter chain, and guarantees that a
terminal target bypasses the row cap. A fork regression exercises Tactix's
public slot-boundary path with a zero cap. Replacement job `1657031`, campaign
`20260830-2590-tierd-rank20-first-factor-admitted-target`, repeats only the two
necessary tasks with this corrected contract.

Both replacement tasks remained CPU-active but hit the 1,200-second solver cap
without reaching the late admission row. This is observer overhead, not a
Factor absence or semantic result: the production solvers finish in 594.433
and 140.142 seconds. No trace was retained. Campaign
`20260830-2600-tierd-rank20-factor-schedule-journal`, job `1657033`, therefore
uses exactly two Factor-filtered rule-journal tasks with no event trace. A
shared compile-time diagnostic arm skips transaction digest construction but
preserves offered/admitted conflicts, decline reason, effort, budget, and
result. Its only purpose is to locate the late admission schedule cheaply;
zero digest fields make no state-equality claim.

Job `1657033` completed both SAT runs. The offered Factor cadence is identical
at conflicts 0, 4,547, and 14,005. The next offer is Tactix 28,690 versus
Kissat 28,694, followed by Tactix 48,921 versus Kissat 48,925. Tactix first
admits Factor at 48,921 and spends 197,034,943 Factor ticks. Thus the search
trajectory has already separated by four conflicts before the first admitted
Factor transformation; Factor cannot own the first divergence.

The Kissat journal's effort column exposed a diagnostic wiring error: the
generic Probe wrapper measured Factor in `probing_ticks`, although Factor owns
`factor_ticks`. Its zero-effort/no-op labels are invalid, while its call
conflicts remain valid. The overlay now maps Factor to its own meter and leaves
every other Probe rule on probing ticks. Single-task replacement job `1657035`
reruns only Kissat; the valid Tactix schedule is not repeated.

Job `1657035` confirms Kissat's first nonzero Factor call at conflict 48,925
with 197,079,650 ticks. Tactix's first call at conflict 48,921 spends
197,034,943 ticks, a difference of only 44,707 ticks (0.023% of Kissat).
Admission structure and first-call work are therefore essentially aligned,
while the four-conflict trajectory offset already exists. The next common
state is the conflict-14,005 pre-Factor boundary after Probe phase 2, where
both calls still decline Factor; do not expand first-call Factor internals
unless that earlier state remains exact.

Campaign `20260830-2620-tierd-rank20-factor-boundary-c14005`, job `1657036`,
contains the two exact common-state tasks. A shared conflict-qualified terminal
selector ignores earlier Factor offers and computes one semantic formula macro
at conflict 14,005, then exits before both solvers decline the call. This tests
the post-Probe-phase-2 state directly without tracing the intervening search.

Both full-trace tasks in job `1657036` hit the 1,200-second solver cap without
a row. This execution mode is retired for Rank 20. The late-open observer keeps
the trace entirely unopened through search—Tactix checks only slot boundaries
and Kissat checks only `kissat_factor` entry—then opens the ordinary trace at
the selected conflict and reuses the same fingerprint implementation. Two-task
replacement job `1657038`, campaign
`20260830-2630-tierd-rank20-factor-boundary-c14005-late`, has a 600-second
solver cap and no hot-path trace output.

Job `1657038` completed both selected exits in 33.85 and 54.57 seconds. The
conflict-14,005 state is not equal. Tactix has 318,296 decisions and
215,663,148 propagations versus Kissat's 284,845 and 233,109,528. Tactix also
has 1,011,730 irredundant, 8,514 redundant, and 12,552,625 binary clauses;
Kissat has 1,011,729, 9,104, and 12,552,430. All three formula hashes and the
top-32 order differ. The known 1,008-variable root-compaction representation
difference remains visible but does not explain those semantic formula and
trajectory differences. Factor therefore cannot own the first divergence.
The next and only coarse narrowing point is the preceding common Factor offer
at conflict 4,547; if it already differs, localization moves to the first
post-startup inprocessing/search interval rather than deeper into Factor.
Two-task campaign `20260830-2640-tierd-rank20-factor-boundary-c4547-late`, job
`1657040`, reuses the validated immutable late-open pins for that comparison.

Job `1657040` completed in 23.43 and 40.46 seconds, and conflict 4,547 is also
unequal. Tactix has 233,628 decisions and 169,397,772 propagations versus
Kissat's 210,133 and 193,984,768. Its irredundant/redundant/binary counts are
1,011,730/3,571/12,552,839 versus 1,011,729/3,492/12,552,626, and formula
hashes and top-32 order differ. The next boundary is the same Probe phase's
Congruence entry, before any inprocessing rule runs. Equality there would put
the discrepancy inside that Probe pipeline; inequality would put it in search
after the exact startup state and before the first search-time Probe.
Two-task campaign `20260830-2650-tierd-rank20-probe-entry-c4547-late`, job
`1657042`, performs exactly that comparison.

Job `1657042` completed in 21.16 and 36.64 seconds. Before Congruence, both
irredundant formulas are still identical at 1,011,733 clauses and hash
`295fd0162dec85d7`, but search state has already separated. Tactix/Kissat have
233,628/210,133 decisions, 146,300,146/162,513,175 propagations,
3,680/3,591 redundant clauses, and 12,553,621/12,553,620 binaries. Therefore
no rule in the conflict-4,547 Probe phase owns the first divergence. The next
experiment is a bounded low-volume trajectory prefix from the exact startup
state through conflict 50. Two-task campaign
`20260830-2660-tierd-rank20-search-prefix-c0-50`, job `1657044`, runs that
observer and no production or scoring task.

Job `1657044` reports no portable trajectory divergence through conflict 50:
both solvers reach the same 10,214 decisions and 10,353,769 propagations at the
terminal analysis, with identical conflicts, learned raw/sorted/shrunk/final
hashes, and backtrack levels throughout. The comparator's overall length flag
is observer lifecycle only: Tactix catches the requested stop and emits a
`terminate` row, while Kissat terminates directly by signal. The next paired
trace starts at conflict 50 and runs to the already-known unequal Probe entry
at 4,547, so it should expose the first real trajectory mismatch in one pass.
That run is two-task campaign `20260830-2670-tierd-rank20-search-c50-4547`,
job `1657046`.

Job `1657046` is not a paired verdict. Kissat reached the requested boundary
in 900.01 seconds and retained an untruncated 519,202-row trace with SHA-256
`dcb97c0c...89f19c`. Tactix exceeded the 900-second observer cap and retained
no trace. Keep the valid Kissat artifact; rerun only the missing Tactix arm
with a longer cap rather than repeating Kissat or comparing partial data.
Single-task campaign `20260830-2680-tierd-rank20-tactix-search-c50-4547`, job
`1657048`, is that replacement.

Job `1657048` reached its 1,800-second observer cap at conflict 1,006, not the
requested conflict 4,547; the harness accepted graceful termination without
checking the target and is therefore insufficient for a completion claim.
The untruncated 774,533-row prefix is nevertheless enough: comparison against
the retained Kissat authority finds the first real divergence at conflict 502,
decision 62,811, level 289. Both have 54,392,443 propagations and draw the same
RNG word `2607034833`, producing the same modulus slot 490,347. Tactix maps it
to external variable 573,525, while Kissat maps it to 2,745. All earlier
portable events match. The cause class is thus the compact random-index
permutation: Tactix currently stores first-activation order, while Kissat's
post-compact internal indices preserve first-import order filtered to its live
activated population. Activation order should seed queue/scores only; random
indexing and `all_variables` consumers require compact import order.

The repair now constructs that compact image by filtering permanent
first-import order through the activated population; first-activation order
continues to seed only the queue and initial scores. A focused fixture makes
the two orders deliberately different and pins both consumers. Trace/Release
focused suites pass 3/3 and 5/5. Single-task late-open job `1657049`, campaign
`20260830-2690-tierd-rank20-randec-slot-repair`, checks that the exact common
word/slot now maps to external 2,745 without paying the full trace overhead.

Job `1657049` completed in 10.70 seconds and emitted exactly one row. At
conflict 502 and Tactix's pre-increment decision counter 62,810, it records
54,392,443 propagations and `raw=2607034833|slot=490347|external=2745`, exactly
matching the pinned Kissat choice for decision 62,811. The first causal
trajectory divergence is closed. Measure one production Rank 20 solve next;
do not infer the downstream runtime from this checkpoint alone.

Single-task campaign
`20260830-2700-tierd-rank20-randec-import-order-production`, job `1657050`,
pins Release SHA `1d85a694...cb4f7`, requires SAT exit 10 and an independently
checked model, and is the only production measurement of this repair.

Job `1657050` is model-checked SAT in 123.02 wall seconds (121.20 user), versus
the preceding homogeneous candidate's 594.433 seconds and pinned Kissat's
140.142. Peak RSS falls from 2,204,660 to 1,981,224 KiB, still 2.09x Kissat's
946,684 KiB. This is mechanism convergence, not timing luck: Tactix now exactly
matches Kissat's 81,993 conflicts, 1,229,783 decisions, 493,961 Factor
factorings, and 21,227 BVE eliminations. Tactix records 840,278,325
propagations versus Kissat's previously measured 877.9M, so counter semantics
or remaining per-propagation representation differences still warrant the
ordinary parity audit; they do not reopen the closed random-slot cause.

Homogeneous campaign
`20260830-2710-tierd-ranks1-40-randec-import-order` freezes the same Release
SHA and applies it uniformly to all 40 ranks under job `1657091`. Its first
submission, job `1657051`, failed in atomic claim creation because the empty
`claims/` directory was missing; no solver launched. The repaired submission
adds only that harness directory. It publishes UNSAT timing provisionally,
keeps independent proof checking alive, and reruns neither the previous
candidate nor Kissat.

The solver phase completed 40/40 expected verdicts with no timeout. All 22 SAT
models verify. Total time is 9,305.953 seconds versus 9,987.021 for the previous
candidate (0.9318x total, 0.9463x geometric mean). The SAT subset falls from
4,757.895 to 4,083.055 seconds and is now within 0.62% of Kissat's 4,057.914
seconds by total; its geometric mean is 1.1069x Kissat. Rank 20 improves
594.433→124.566 seconds, Rank 21 230.719→131.230, and Rank 28
270.435→138.092. The UNSAT subset remains essentially flat by total
(5,229.126→5,222.898 seconds) and is now the dominant reference gap. At the
solver-complete snapshot, 7/18 UNSAT proofs have independently verified with
zero invalid results; the other checkers remain scheduled.
