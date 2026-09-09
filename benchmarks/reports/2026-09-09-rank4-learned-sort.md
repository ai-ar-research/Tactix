# Rank 4 Release cost and isolated learned-sort shortcut

## Decision

Investigate a private stable-sort identity shortcut, not a scheduler change.
Accepted production remains c2a95a6; no runtime improvement or promotion is
claimed. The separate FlatLists allocation redesign is still unapproved and
unimplemented. Memory, Rank32 and the complete Rank1–50 target remain open.

## Evidence

[Profile6166](../runs/20260909-6166-rank4-release-profile/summary.json), job1677048,
uses ordinary Release candidate6107 and clean current Kissat, with DRAT enabled,
12 logical CPUs, 48000MiB and one bound solver thread per independent task.
Both full UNSAT proofs are independently checked and all checker/input residue
is deleted. Within each arm, certificate bytes/hash and work counters exactly
replay the manifest-bound unprofiled solve; every candidate non-timing statistic
also matches. Both perf reports have zero lost samples.

Current Tactix self samples: propagation49.60%, learned-level radix sort6.33%,
block shrinking5.37%. Kissat's separate propagation/analysis functions have
different scopes; sample percentages are not cross-solver operation prices.
The approximately eight-second profile is diagnostic, not repeated timing
acceptance. The preceding6160 trace agrees on907045 decisions and380 search
boundaries through650000 conflicts; full solves end at651764. This is strong
support for a local engineering experiment, not whole-trajectory identity.

## Change and invariants

[Isolated6169](../runs/20260909-6169-learned-sort-build/manifest.json) descends
from frozen6154, whose Release binary is identical to6107. It moves the private
learned-level helper into a same-rule header so the fixed fixture exercises
the actual implementation. Each byte histogram detects the single-bucket
case; that stable radix pass is the identity and skips prefix sums/scattering.
The existing final copy handles an odd number of nonconstant passes.

Kissat `sort_deduced_clause` requires descending decision-level groups with
stable deduction order; the neighboring Tactix bump-stamp radix sort already
uses constant-byte skipping. CaDiCaL's trail-rank sort has a different ordering
contract and is not substituted. No threshold or benchmark-specific parameter
was added. UIP position, stable ties, logical state, proof order, scheduling,
budgets and public M3/N1 capability are unchanged. OTFS clears this dead scratch
before its next resolvent assembly. Both-side reconstruction and inherited
Reduce/Substitute root closures are retained.

## Validation and next gate

Both independent Release builds pass102/102 tests and share SHA256
`fa6986b17c70b9213ccad0c28cf741266174745e6d05c44d4049c1da4a55936d`.
The new actual-helper test compares against `std::stable_sort` on433 fixed
cases: all16 constant/varying-byte masks; odd/even scatter counts; byte
boundaries through UINT32_MAX; distinct literals tied at the same level;
unchanged UIP, idempotence and oversized/undersized reused scratch. These are
equivalence tests, not a claim that the previously correct sort should fail.
Existing M3 proof/model/API tests remain; the calculus and guards did not change.
All39 benchmark and13 watcher unit tests pass.

[BC6171](../runs/20260909-6171-learned-sort-BC-gate/manifest.json), job1677050,
is a new full-width66-task unchanged B28+C5 campaign. It owns fresh independent
pins and the original1200-second solver/16000-second checker budgets.
[The checkpoint](2026-09-09-learned-sort-validation.json) binds observed records
and exact candidate predecessor proof/non-timing replays. Pending certificates
allow development, not gate closure. After all expected BC outcomes and replay,
run three separate paired unprofiled Rank4 waves against6107, then full50 and
the required Claude-owned fuzz gate. No source is promoted while these remain
open; denied fuzz-transfer/push requests have not been retried.

## Matched fifty and hygiene

Ranks41–50 already ran as paired6108/job1676783 on the original twelve-CPU,
48000MiB contract, not cpu-q. All ten added Kissat proofs/models are checked;
one Tactix Rank43 certificate remains pending at this checkpoint. The fixed
Kissat50 sum is7239.010s, preserving original40's6810.066s. Fresh accepted
production/current-Kissat6121 has100 solver records and98 independently
checked records, with the aggregate suppressed until complete. The cumulative
6107 full campaign has49/50 checked; its8559.858-second total is not a final
acceptance result. All protected old scripts/manifests/pins are hash-unchanged.
No files were manually deleted. Reports are local, not sent to Andrew.
