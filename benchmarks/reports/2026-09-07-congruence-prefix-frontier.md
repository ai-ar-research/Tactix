# Congruence physical-prefix cleanup and the next runtime checkpoint

## Outcome and scope

An isolated general repair restores Kissat's Congruence forward-cleanup scope:
include redundant clauses interleaved through the last irredundant physical
position. Production previously copied CaDiCaL's irredundant-only filter.
The actual native fixed test distinguishes prefix from suffix; old Tactix
fails that fidelity assertion, repaired Tactix passes. This is not a solver
soundness failure. All 76 trace-build and 77 fresh Release CTests pass.

On Rank 34, the repair removes the 16 extra redundant clauses at the second
Congruence call at conflict 132816. All eight selected Vivify count vectors and
the later queue-bump identities agree. The full decision stream is identical
through conflict 200000: **475,997 common decisions**, versus the prior split
after 328,665. This trace alone establishes neither later decisions nor runtime.

The cumulative candidate remains **UNFUZZED and unpromoted**. Production
source is unchanged at `c2a95a6`; the authoritative Rank 1–40 ratio remains
1.2787518x, not the requested ≤1.01x. The
[runtime review](2026-09-07-runtime-review.md) owns the audit of Claude's work,
retained implementation repairs, and rejection of the unsupported “93% search
quality” attribution. This report does not revive that attribution.

## Earliest-producer evidence

Campaigns 5360–5370 are proof-on stopped diagnostics, not checked verdicts or
timing measurements. Each submitted pair uses full-width 0–1, 12 logical CPUs,
48000 MiB, one bound solver per task, no node constraint or throttle. Every
terminal task exits zero at its requested stop and deletes scratch. Exact
source chains, pin hashes, input hash, runner and submission are campaign-local.

| Campaign / job | Decision-changing observation |
|---|---|
| 5360 / 1668810 | All 38 Factor queue boundaries have identical shared-identity order through 132816. Extra Tactix member 22152 remains. |
| 5361 / 1668812 | First 27,585 queue-bump identities/links/values match; stamps differ uniformly by one. First changed bump is at 133186. There are 53 earlier transient cursor differences, not full H-state equality. |
| 5362 / 1668814 | At 133186 the conflict and final learned clause agree, but the reason for pivot 20398 differs. The first 28 resolution blocks agree; block 28 has equal clause set with different pose. |
| 5363 / 1668816 | First 1,414 post-phase implications agree. At 132832, native assigns 21214 after 21187; Tactix assigns -21188 next. |
| 5364 / 1668818 | The clause `[-21187,-21186,18559,21214]` is watched/scanned differently at 132831, before the implication counter advances to 132832. |
| 5365 / 1668820 | First 3,656 selected-clause watch/pre-post records agree. Four first-dose Vivify candidate schedules agree; second-dose tier1 first swaps order at ordinal 2915. |
| 5366 / 1668822 | First four selected count vectors agree. Second-dose counts differ: -50 is 169/160, -2116 is 13/11. |
| 5367 / 1668824 | No ordinary BVE events at 132816 in either arm. Native Probe has no ordinary BVE slot: missing files are a negative hypothesis, not a solver failure. |
| 5368 / 1668826 | Second Congruence begins with equal counts and leaves Tactix 16 extra redundant clauses; following Substitute removes six. |
| 5369 / 1668828 | Actual prefix repair closes redundant counts, all eight selected count vectors, queue bumps and all 328,670 decisions through the old stop. |
| 5370 / 1668830 | Same repaired pins: all 475,997 decisions match through conflict 200000. No later divergence localized yet. |

5360–5363 each own a retained comparator and summary. The combined
`benchmarks/probes/scratch/seal-congruence-frontier-5364.py` validates 5364–5369,
including byte-identical prior standalone streams for all read-only observer
campaigns. Summary hashes preserve large raw local traces without committing
them. Native 5369 standalone streams are unchanged from 5368.

The source-to-choice chain is:

`Congruence prefix cleanup → retained redundant clauses → Vivify counts/order
→ work and remaining budget → watched pose → implication reasons → queue bumps
→ focused branch choice`.

This chain explains why patching the final queue choice or the apparent watch
cursor would compensate for an earlier defect. In second-dose tier2, native
has 1687 ticks left after candidate 1208; Tactix has 3201. Native stops before
1209, while Tactix processes two more candidates. The next native tier resets
the target cursor while Tactix is still processing the previous tier. The
different cursor is downstream work, not proof of a missing cursor reset.
Pre-scan watch dumps snapshot a whole list before processing; their row
adjacency is not the live per-entry execution order.

## Reference contract and isolated implementation

Native `kissat/src/congruence.c::forward_subsume_matching_clauses` enters dense
mode, scans live arena records through `last_irredundant`, and does **not**
skip redundant records. Its `find_subsuming_clause` skips redundant partners
for irredundant victims. CaDiCaL's `Closure::forward_subsume_matching_clauses`
explicitly skips redundant candidates; its actual forward partner loop has
the same class restriction. A separate generic helper can promote a partner,
but that is not the forward-loop behavior being ported.

The two-file `5369/congruence-prefix.patch` changes candidate scope and adds
the fixed regression. It computes the last irredundant position including
unreclaimed garbage, sorts live references through the existing generation-local
logical-position helper, and stops at that prefix boundary. If no irredundant
record exists, native's null watermark imposes no cutoff. This is a reuse of
existing representation machinery, not a new kernel primitive or strategy
selector. No formula, literal, rank, conflict or identity controls the repair.
The class restriction prevents a forgettable redundant subsumer from deleting
an irredundant victim. Proof deletion stays on the established kernel path;
equivalence binaries are already live before cleanup. Public guards and the
canonical closure instance remain unchanged; no new reconstruction effect.

Fixed native and old/new fixtures are
`benchmarks/probes/scratch/congruence-prefix-5369.{c,cpp}`. The native fixture
calls the real unmodified full rule; the C++ fixture links actual old/new solver
objects with their exact CMake feature definitions and includes. Results,
commands, object/source hashes and binary hashes are in `5369/fixture.json`:
native and repaired delete the redundant prefix and retain the suffix; old
Tactix retains both. The initial native fixture compile had a helper-name
collision with the reference's typedef; it was corrected before execution.

Trace source: `.claude/worktrees/runtime-congruence-prefix-5369`.
Fresh Release directories: `build/pin-5371-congruence-prefix-release` and
`build/pin-5372-congruence-prefix-bc`. Both SHA-256:
`4f406acc56fe6cf8a68d32b8567a845c8cb08cd164286fd838b4398fa9b3a2be`.
They disable event tracing/conflict stops; submitted builds remain immutable.
Existing compiler warnings about SubStep aggregate initialization, a Vivify
unused parameter and C-only `-fno-rtti` are not new correctness failures.

## Checked gates and limits

The preceding empty-Factor candidate's B28+C5 campaign **5357 / 1668740** is
now terminal: **66/66 checked/clean**, no timeout or solve loss. Each arm solves
33 (17 SAT, 16 UNSAT). B candidate/current is 353.206/410.159 s = 0.86114x;
C is 706.933/705.556 s = 1.00195x; combined 1060.139/1115.715 s = **0.950188x**.
The two long UNSAT checks are included only after final validated records
arrived. `seal-empty-factor-gates-5357.py` owns the summary. This is not the
new Congruence candidate's gate, nor a global runtime win: that preceding
candidate's three-witness screen was 1.11539x production.

New checked campaigns, with independently checked proof-on arms:

- **5371 / 1668832:** Ranks 17/21/34, production / preceding cumulative /
  repaired cumulative / pristine Kissat; one diagnostic wave, 12 tasks.
- **5372 / 1668833:** the same predeclared B28+C5 membership, current production
  versus repaired cumulative candidate, separate equal-hash B/C builds; 66 tasks.

5371 is now terminal, **12/12 checked SAT models**, no timeout, clean scratch:

| Rank | Production (s) | Prior cumulative (s) | Repaired (s) | Kissat (s) |
|---|---:|---:|---:|---:|
| 17 | 249.074 | 202.244 | 153.164 | 113.652 |
| 21 | 111.696 | 135.388 | 135.449 | 96.823 |
| 34 | 560.366 | 694.132 | 599.131 | 331.154 |
| Total | 921.136 | 1031.764 | 887.744 | 541.629 |

Candidate/prior is **0.860414x**; candidate/production **0.963749x**;
candidate/Kissat **1.639026x**. The observed gain is on Ranks 17 and 34,
not 21. Rank 34 still regresses against production despite improving against
the exact preceding candidate. Maximum RSS is 4713520 KiB candidate versus
1436392 KiB Kissat. This one wave is a promising screen, not a promotion or
decision-grade effect size. `5371/summary.json` owns exact records and hashes.

5372 remains in flight: **64/66 final checked/clean records**, tasks 64/65
checking UNSAT proofs. No partial record establishes acceptance. Watch the
existing job, then run `seal-congruence-gates-5372.py`; do not resubmit.
A decision-grade targeted runtime claim needs three paired waves; Rank 1–40
acceptance needs the full gate. No new fuzz is launched before consolidation;
Claude must own that campaign and its terminal interpretation.

Independent residual leads remain: transient first-Sweep binary count 1832/1834
reconverges at following Substitute; queue population includes extra 22152;
53 transient assigned-cursor differences precede the old bump split. The repair
does not close those, every Vivify tick category, whole-solve throughput, or
memory competitiveness. Prefix equality is useful localization, not the goal.

## Handoff and evidence hygiene

Retain this general repair in isolation; complete 5372, then extend the
decision/work comparison beyond conflict 200000 or prioritize matched-state
throughput if the cumulative timing remains worse. Do not chase an arbitrary
neutral trace difference without a worse boundary or a localization dependency.

Living docs retain current semantics and open production deviations, within
configured size budgets; source reports and Git retain chronology. No frozen
prior report is edited. Campaign certificates/input/checker scratch remain
job-owned and terminal runners delete them; raw large diagnostic streams and
immutable pins remain local with hashes. No broad or speculative cleanup is
authorized. Push remains unapproved after the earlier auto-review rejection;
this checkpoint is local only.

The [artifact index](2026-09-07-congruence-prefix-artifact-index.json) retains
13 campaign hash inventories and trace/Release test logs. No repository
certificate residue was found; nothing was manually deleted. Final production
validation also passes 77/77 CTests in 5.02 s. The status checker with scheduler
access has zero errors; remaining warnings concern hardware-L3 false positives
and the old uncited August-13 tier window, not the active normal jobs.
Full cached-diff whitespace checking flags unified-patch context, native logs
and CRLF matrices in immutable run artifacts; those exact hashed bytes remain
unchanged. The editable docs, helpers and reports pass whitespace checking.
