# Compact canonical Vivify: isolated implementation and correctness checkpoint

The approved7135/7140 representation pilot is implemented as7144, forked from
frozen7030 rather than rejected7116. No production cutover or performance
success is claimed. The current checked50 score remains1.123176512x Kissat;
Rank31 and the full50 memory/consolidation/fuzz obligations remain open.

## Structure and unchanged behavior

Canonical homogeneous reuse-enabled schedules without prefix flushing collect
only references during the existing two root-rewrite passes. This retains
pass-time membership even when root rewriting lowers a clause into another
glue band. Occurrence counting still happens between the two passes.

Redundant schedules sort literals directly in the final arena records, then
sort four-byte references by the existing priority/count-vector/size/final
literal/logical-position keys. They no longer materialize the literal pool or
sort forty-byte candidates. Irredundant schedules leave clause pose unchanged
and construct compact count/size/logical-position records. Stable radix passes
rank ordinary schedules; the native skip-sort regime ranks logical position
alone. Reverse emission within priority partitions preserves native pop order.
Scratch retains capacity only, not previous count snapshots. Real memory and
cache effects still require measurement; representation size is not an RSS claim.

Const public queries, mixed scopes, disabled reuse, and prefix flushing retain
the generic implementation. No rule/guard, kernel primitive, proof vocabulary,
policy cadence, budget, threshold, or public API was changed. The existing
calculus and implementation contracts therefore remain unchanged. No raw arena
pointer survives a root rewrite. The frozen source, complete file inventory,
incremental/cumulative patches, binaries, and full logs are bound by the
[7144 build manifest](../runs/20260910-7144-vivify-compact-build/manifest.json).

## Correctness evidence and limits

- Independent Release/B-C/trace suites passed115/115,115/115,114/114.
- The existing96-case ordering guard retains digest`c67723e59482dad7`.
- A new1728-case actual transaction regression covers all scopes, reuse/flush
  combinations, priorities, root-false rewriting, reversed import/literal
  order, logical-position replacements, interrupted/repeated rounds, public
  queries, exhaustive eight-variable models, and DRAT/LRAT byte digests.
  All three candidate builds match frozen7030 Release/trace controls exactly:
  digest`11603ba2e1d600bb`,39624 proof bytes,1872 subsumed clauses.
  [7146 control](../runs/20260910-7146-vivify-compact-controls/summary.json)
  reproduces each existing control executable byte-for-byte before relinking
  only the new fixture. Existing432-case count-cache regression also passes.
- [7147 sanitizers](../runs/20260910-7147-vivify-compact-sanitizers/summary.json):
  six actual Debug ASan/UBSan/LSan Vivify fixtures pass twice each, outside ptrace.
- [7148 proof controls](../runs/20260910-7148-vivify-compact-proofs/summary.json):
  96 independently checked final DRAT/LRAT proofs, exact across candidate and
  parent, plus192 empty/truncated negative controls. Only these completed fixed
  controls'179234 bytes of temporary inputs/certificates were removed; hashes
  and checker receipts remain. This is not current-source fuzz coverage.

The frozen7144 build manifest's prose mistakenly calls the new transaction
matrix3456 cases. The actual loop, executable output, control evidence and
successor gate prove1728, not3456. Preserve the original manifest and this
explicit correction; do not inflate coverage. Compiler warnings inherited
from7030 remain; no warning-free claim. The first watcher invocation used an
obsolete positional CLI form and exited before watching; the corrected
`--job`/`--since` invocation monitors the same submitted job, not a new array.

## Experiment and next decision

[7149 B28+C5](../runs/20260910-7149-vivify-compact-BC-gate/manifest.json)
uses unchanged full membership, isolated7030/7144 pins, and the frozen checker
runner. Job1680134 was submitted at2026-09-10T21:19:24 local time with66 tasks,
12 CPUs,48000MiB, no throttle or node constraint, cpu-dense-hwulab-q. Solver
budget remains1200s, with separate checker/cleanup allocation time.

7150 prepares three paired31/35/38 runtime waves;7152 prepares three paired
Rank35 complete-Vivify profiles after the first exact unprofiled Rank35 pair.
Proceed on expected outcomes and exact work/certificate fingerprints while
proof checks continue. Any invalidity pauses dependent work. The exact current
published outcomes, launch state and active next step belong to PROGRESS and
the7151 audit, not this implementation narrative.

Gates remain at least15% lower complete-Vivify sampled cycles on35 and at
least1% lower whole-runtime paired median on both31 and35, with no whole-runtime
or RSS regression31/35/38. Preserve all three repetitions and the unchanged
variance rule; do not select favorable repeats or lower thresholds. Failed
premises trigger evidence-backed reassessment within scope, without another
routine approval pause under the updated `revamp` workflow.

No old campaign, frozen reference, or source pin was modified. No new Kissat
baseline run, Claude debugging delegation, fuzz-artifact access, denied
transfer, or push retry. Current-source Claude-owned fuzz remains unavailable,
not waived. Andrew's report is local and unsent.
