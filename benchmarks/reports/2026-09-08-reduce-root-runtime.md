# Reduce root-guidance runtime and the next connected-occurrence experiment

## Decision

The within-1% goal remains unmet. Independent5900 completes all40 expected
solves (22 SAT/18 UNSAT) in7573.054s against the fixed, fully checked
Kissat6810.066s: **1.112038268x**, versus parent5849's1.136183849x.
This is a cross-window aggregate screen, not an isolated unit-cost measurement.
Keep the tested general repair as investigation material; do not promote it.

Authority: `benchmarks/runs/20260908-5908-reduce-root-ranks1-40/solver-checkpoint.json`
and `next-losses.json`, sealed once by5916. All40 verdicts are expected;
29 certificates are checked/clean at this snapshot. Remaining checks continue
asynchronously. Maximum measured RSS is15,967,668KiB, below16GiB in this wave,
not a separately enforced hard-memory proof.

B28+C5 (`20260908-5906-reduce-root-BC-gate/solver-checkpoint.json`) has66 expected
solves and64 checked at snapshot. B305.257/410.284=0.744014x production;
C658.909/708.623=0.929844x; combined0.861703x. Report both tiers independently.

| Rank | 5900 seconds | Parent5849 | Fixed Kissat | Interpretation |
|---|---:|---:|---:|---|
| 32 | 531.601 | 588.327 | 274.456 | Largest remaining loss; conflicts1.969035x native. |
| 37 | 518.137 | 489.248 | 346.372 | Regresses parent; conflicts1.345489x native. |
| 21 | 247.181 | 136.100 | 96.821 | Material changed-work regression; must remain visible. |
| 27 | 340.949 | 340.965 | 245.726 | Runtime neutral to parent, changed proof/work. |
| 31 | 330.018 | 502.337 | 264.532 | Large improvement; conflicts1.052052x native. |

The previous standalone5870 preconnection branch remains a regression:
full1.165359x and C1.015549x. Its improved bounded prefix never established a
runtime win. No old source, result, pin, denominator or runner was rewritten.

## Causal progress

The earlier `2026-09-08-reduce-root-guidance.md` owns the implementation cause,
actual old/new/native controls, four passing suites and46 checked fixed proofs.
5900 removes newly derived Reduce-preparation roots from policy queue/heap,
using the final trail suffix after backtracking, not the invalid incoming size.
Raw public Reduce remains policy-neutral; no guard or Reorder weight changes.

5907's complete Rank32 comparator checks the early native stream and suffix
with their exact overlapping rows and cumulative hashes:68,091,135 decisions
match, closing conflict225987. The next choice splits at271085/67954350,
Tactix-29407 versus native-29066.

5917 (job1675097) is terminal and replay-neutral: every cached ordinary300K
trace hash and partial proof byte matches its arm. Its last-common heap has
75285 versus75288 entries;1611 shared positions differ and17 members occur in
only one arm. All shared scores, assignment flags and active flags agree.
Both selected variables are free and have score9.1447695215666759e119.
Thus current evidence does not support changing score arithmetic or tie rules.

At conflict271079, both arms enter forward/BVE with88946 active variables,
478330 irredundant clauses,22923 redundant clauses and124717 binaries.
The first forward pass and first BVE exit agree on portable counts. After the
second forward pass, irredundant counts differ517857 versus517854. The second
BVE exit then differs75381 versus75384 active,530633 versus530578 irredundant,
and110826 versus110828 binary clauses. This selects the already tested
incoming-root preconnection cleanup as a discriminating composition experiment;
the count residual alone does not prove identical clause identities or cause.

Separately,5910 (job1675028) validates every cached ordinary1M trace and partial
proof for the regressing5870 Rank31 branch. Both heaps contain the same50730
members with identical scores/assignment/active flags; onlynine zero-score
positions differ before394397/96579679. Preserve that diagnostic, but prioritize
the current candidate's losses rather than extending a regressing branch.

## Fresh isolated composition5920

`benchmarks/runs/20260908-5920-reduce-preclean/manifest.json` binds the new source,
both parent authorities, exact patches, test logs and binaries. It composes
5900's policy cleanup with5870's unchanged preconnection implementation.
The only additional code is a unit test exercising their interaction: Reduce
derives roots after truncating a search trail, removes their guidance, then
forward reconnect deletes incoming-satisfied occurrences. The test checks
physical BVE counts, unaffected guidance, invariants and the original SAT model.
Existing public-neutrality, no-op, eager/lazy-lifetime and refutation tests stay.

Release96/96 twice,trace95/95,journal96/96 pass.46 DRAT/LRAT proofs are
independently checked on the combined binaries, including active-journal
refutations; actual old/new/native Reduce controls pass. No checker acceptance,
rule guard, primitive, proof vocabulary, option, budget or identity-based
selection changes. The calculus and implementation ledger retain these boundaries.

New full-width jobs, all oncpu-dense-hwulab-q,12CPUs/48000MiB:

- 5924 B28+C5: job1675144, separate candidate B/C pins,1200s solver/5h allocation.
- 5925 Rank32 prefix: job1675145,300K stop, cached native comparison,25min allocation.
- 5926 full40: job1675162, candidate-only, fixed reference,1200s solver/5h allocation.

Proof-check budgets remain separate from the1200s solver cap. Current-source
fuzz is blocked by denied external-transfer approval; no retry or artifact
inspection. This blocks promotion, not provisional development. The independent
cpu-q5784 campaign stays4CPUs/32768MiB/1200s solver/16000s checker/5h allocation;
its two known input-empty-clause checker-status false negatives remain unscored.
No push, production promotion or manual deletion.

Next: compare every5925 decision and the271079 boundary; score full5926 and
each BC tier as complete verdicts arrive. Do not bank the composition from a
longer prefix alone. Reopen Rank21/37 regressions and inspect failed certificates
immediately; continue successful pending checks in the background.
