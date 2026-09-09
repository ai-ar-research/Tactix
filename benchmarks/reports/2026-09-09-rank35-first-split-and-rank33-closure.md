# Rank35 first decision split; Rank33 three-arm early closure

## Outcome and next decision

The Rank35 decision split is localized after the previously validated3M prefix.
Investigate its preceding late Vivify invocation with a neutral replay, not an
assumed heap or TransitiveReduce repair. Separately, current Tactix6424,
parent6383 and pinned native Kissat agree through Rank33's first100K conflicts;
extend that three-way comparison to1M. No solver source changes this checkpoint.

The full50 runtime goal remains unmet: frozen6441 is1.206194355x fixedKissat,
with no new aggregate measurement. Keep every rank, adverse result and memory
gap in scope. New6475's temporary allocation reduction is not a whole-solver
runtime or RSS claim, and neither6475 nor6448 is promoted.

## Rank35: validated first split

Validator6469 successfully sealed
`benchmarks/runs/20260909-6467-rank35-near-final-prefix/summary.json`.
Job1678073, submitted2026-09-09T09:14:12, completed both arms with zero exits
and their own scratch deleted. The unchanged current6424 and earlier6305 pins
ran through4,400,000 conflicts.

The validator rechecked the3M cumulative hash `4bfd01ec4333149f` and the entire
one-row threshold overlap. It validated29,390,016 current suffix rows and
26,319,870 earlier suffix rows, finding2,326,212 matching suffix rows including
that overlap: **161,899,386 matching decision rows from the beginning**.

| First differing row field | Current6424 | Earlier6305 |
|---|---:|---:|
| Conflicts | 3,074,183 | 3,074,183 |
| Decision counter | 161,631,566 | 161,631,566 |
| Level | 1,122 | 1,122 |
| External chosen literal | -482,577 | -486,307 |
| Final diagnostic column | 0 | 0 |

This compares two Tactix sources, not native Kissat. Cumulative observer rows
and the solver decision counter differ because preprocessing can reset the
latter; the derived modular start counter is checked, not assumed to precede
the first observed row. No full-state equivalence is inferred from decision
equality. Both partial proofs were hashed and deleted, not accepted as final
UNSAT certificates.

Projected inprocessing counts remain equal through Probe at3,069,160; the next
projected count mismatch is a Strengthen begin at3,120,176, after the decision
split. Inspection of the fully bound raw boundaries reveals a more useful
pre-split lead: the second Vivify invocation at3,069,160 uses **one additional
propagation and52 additional ticks** in6424. Its beginning and ending active,
irredundant, redundant and binary counts agree with6305. The first Vivify call
at that conflict has equal per-invocation propagation and tick deltas.

Earlier cumulative offsets remain visible: current enters that Probe with nine
more propagations and nine fewer ticks/search ticks. Those offsets are not
discarded or called equivalent work. The second Vivify's incremental difference
is a selected lead, not proof that it causes the later decision split. A Reorder
at3,074,062 also precedes the split; it is not declared faulty from proximity.

## Focused Vivify replay6483

[Manifest6483](../runs/20260909-6483-rank35-vivify-replay/manifest.json) binds
the validated split and both exact Vivify begin/end pairs. It copies the same
immutable6424/6305 pins and bounded6467 runner, adding only the already-available
standalone candidate and round traces selected at conflict3,069,160. Both
invocations and all scopes are retained. The broad event tracer stays disabled.

Job **1678093** was submitted **2026-09-09T09:51:28 EDT**. Validator6485 first
requires every ordinary trace and the partial-proof hash/size to replay each
arm's own6467 result exactly through4.4M. Only then does it interpret complete
Vivify records, separating semantic candidate/pose fields, effort/budget fields,
and physical metadata. Reference offsets and proof identities are not portable
alignment keys. Partial-proof replay is a neutrality check, not final acceptance.

Test consideration: the new maintained `tools/bench/vivify_trace.py` reader
has11 tests covering schema, row width/truncation, sequence, selected conflict,
round framing, repeated scope/reset ordinals, empty rounds before an immediate
budget cut, candidate identity, clause/pose multisets, effort, invalid stops,
extra/missing rows and termination without a budget-stop row. All76 benchmark
tests pass. No observer, solver, proof rule or checker source changes.

## Rank33 closure and extension

Validator6472 successfully sealed
`benchmarks/runs/20260909-6470-rank33-activation-prefix/summary.json`.
Job1678077, submitted2026-09-09T09:17:44, completed all three arms and the
tool-owned watcher returned COMPLETED. All three complete pairwise comparisons
match **31,494,584 five-column decision rows through100,000 conflicts**, with
hash `5c263e00aa882466`. This is bounded decision equality, not formula identity
or proof of equivalent work after100K.

[Extension6486](../runs/20260909-6486-rank33-later-prefix/manifest.json) copies
the same three pins and runner, extends to1M conflicts and streams from100K.
Job **1678096** was submitted **2026-09-09T09:56:36 EDT**. Validator6488 must
validate each inherited hash and complete threshold overlap, cumulative row and
counter accounting, then every suffix row in all three pairs. The native
diagnostic invocation does not refresh the fixed timing denominator.

## Timing, gates and handoff

Shared-blocker6454 now has all twelve preceding expected solver outcomes with
exact parent work and certificate identity. Its final wave **1678095** was
submitted **2026-09-09T09:54:33 EDT**, without waiting for proof checks. Thus
6443,6464 and6454 all have their three predeclared waves submitted. Run their
summary writers6445,6466 and6456 once each after18 expected outcomes; retain
every ratio and arm spread, including losses. No selective repeats.

Position-key6475 B28+C5 job1678087 since09:40:04 remains separate.6480 requires
all66 expected outcomes and33 exact6424 work/proof comparisons before matched
full-instance memory evidence. Production, all old pins, the cpu-q campaign,
checked matched Rank41–50 reference and fresh100-result checked/clean reevaluation
remain unchanged. Runtime tasks retain12CPU/48000MiB/1200solver/16000checker/5h;
bounded diagnostics use25-minute allocations. No hard nodes or throttles.

Audit6489 owns later accounting, integrity, certificate counts,76 benchmark and
13SLURM tests, six documentation checks and cleanup. Successful summaries6469
and6472 are frozen and must not be rerun. No new solver/proof/model failure is
known, no manual deletion or live checker-scratch inspection occurs, and no
fuzz inspection or denied transfer/push retry is performed. Current-source fuzz
remains Claude-owned/unavailable; FlatLists and Rank50tracker remain unapproved.

Andrew's local deliverable is
`report-to-andrew/20260909-1000-rank35-split-rank33-closure/report.pdf`, with its
TeX source. Its initial PDF had0.48296pt and15.9062pt overfull lines, caught
before audit; the original compiler log is retained and the paragraphs reflowed
before two clean compilations. It is not externally sent. The goal remains unmet.
