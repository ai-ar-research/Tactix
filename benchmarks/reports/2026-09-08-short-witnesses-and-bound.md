# Native bound confirmation and short B diagnostic witnesses

## Outcome

Native diagnostic 5458 confirms the source-based bound diagnosis on Rank 37:
Kissat hits resolution limit 12587131 at meter 12587157, then advances the bound
from 1 to 2. All nine short-B old/new/native solves in 5462 are independently
checked and clean. Those three cases preserve identical old/new complete work,
proof bytes and slot traces, so they are controls rather than reproducers of
the repaired quota-cut transition. Square's substantial inprocessing work
selects a new short causal diagnostic, not a tuned solver default.

The full Rank 1–40 objective remains at most 1.01x Kissat. Latest complete
5450 solver view remains 1.170282883x with checking pending; production remains
unchanged at checked 1.2787518x. No new solver implementation or promotion
occurred in this checkpoint. Newer 5459/5466 candidates await their own gates.

## Native runtime confirmation

[5458 summary](../runs/20260907-5458-rank37-native-bound/summary.json) validates
one clean controlled stop at conflict 32074. Ordinary verbosity preserves all
7,921,492 recorded decisions against the native predecessor; manifest, binary,
trace and log hashes are checked. The two reported bound transitions are 0→1
and 1→2, with the selected strict resolution overrun confirmed in the log.
The partial certificate was hashed and deleted by its runner, not treated as a
checked complete proof. Job 1670509 is terminal COMPLETED.

This corroborates [5459's lifecycle repair](2026-09-07-bound-lifecycle.md).
The independent repaired-candidate comparison 5463 is now terminal and validated:
[complete streams](../runs/20260907-5463-rank37-bound-prefix/summary.json) agree
on **all 35,346,388 decisions through 100000 conflicts**, versus the preceding
7,918,162-row matching prefix. Both complete prefix digests agree. All
[272 normalized boundaries](../runs/20260907-5463-rank37-bound-prefix/boundary-localization.json)
also agree, closing the observed second-Factor-call split at conflict 32073.
Both controlled stops, immutable pins, full trace hashes and partial-certificate
cleanup pass. This establishes bounded causal closure, not a complete verdict,
checked proof, full-run trajectory identity or speedup. Full campaign 5465 is
running. Rank-31 and Square traces remain the next independent localization work.

## Short B results and their limits

[5462 summary](../runs/20260907-5462-short-bound-witnesses/summary.json) owns all
nine final record hashes and complete stdout/stderr hashes, bound passes,
selected work and comparison results. Each arm solves two SAT cases and one
UNSAT case. Independent model/proof checks pass, no timeout, and all certificate
and derivative cleanup is complete. SAT proof-byte identity is not the SAT
oracle: original-formula model checks are separately required and passed.

| Instance | Old/new Tactix seconds | Native seconds | Bound-path observation |
|---|---|---|---|
| rbsat-v760c43649gyes10 | 6.326 / 6.326 | 7.077 | Normal 0,1,2,4,8 passes; native advances to 16. |
| square.2.0.i.smt2-cvc4 | 5.325 / 5.325 | 3.922 | Two bound-0 passes, then native advances to 1. |
| pyhala-braun-sat-35-4-04 | 4.624 / 4.624 | 4.623 | Normal 0,1,2 passes; native advances to 4. |

No native resolution-cut line appears in these three full logs. Complete
non-timing Tactix statistics, proof hashes and full slot stderr hashes are
identical before/after 5459. They reach productive BVE/Factor and normal bound
advancement but do not distinguish the repaired behavior. Do not count them
as quota-cut coverage; the fixed old/new cut fixture and Rank-37 witness own it.
These one-wave logged times select diagnostics, not performance acceptance.

Square uses 3188 ms inprocessing and 2053 ms outside it in Tactix's timers,
versus native 1.81 s simplify and 1.87 s search. Tactix performs 4734713 BVE
resolutions versus native 2754466, and reports 1380 Sweep equivalences versus
579. It has 17084 conflicts versus 17876 and 40074506 propagations versus
39811062. Timer scopes and transformation mixes differ: these numbers do not
prove a per-resolution or per-propagation cost gap. They do identify a short,
productive case where causal tracing can separate work from implementation cost.

## New short causal trace

Campaign **5471 / job 1670919** uses immutable bound-candidate trace pin 5460
and native 5414 observer, copied into independent campaign pins. The stop at
10000 conflicts is below both observed complete solve counts. Record every
decision and normalized inprocess/macro boundary from zero; compare the earliest
portable difference before choosing any general mechanism repair. The
[manifest](../runs/20260907-5471-square-bound-prefix/manifest.json) owns exact
input identity and hashes. It does not replace Rank-31/37 traces or full gates.

This is a full-width two-task diagnostic, one proof-on solver per 12-logical-CPU/
48000-MiB task, no throttle or node constraint. Partial certificates are hashed
and deleted by the existing runner. No benchmark composition or checker contract
changed. Unit-test consideration found no new solver unit necessary for a
campaign-only change; existing comparator tests and actual-result/hash validation
cover the reused machinery. The new real Square trace is not yet validated.
The boundary helper now permits no mismatch: the actual 5463 result exercises
that branch, while the retained 5453 witness covers the differing-boundary path.
No solver behavior changed and no new solver unit is needed for this analysis.

## Other validation and continuation

[5447 timing](../runs/20260907-5447-replacement-cursor-rank35-runtime/summary.json)
now has all six proofs checked and clean. The three exact-work/proof-byte pairs
retain median 0.976661583x predecessor, range 0.976171498–0.989633235; both
absolute arm spreads remain below 2%. Its local roughly 2.3% gain does not
establish full-run closure. Its B28+C5 checker tail remains separate.

Background Claude owner session 30094 remains live on 5438; only its terminal
response is polled. No fuzz job or artifact was inspected. Its next periodic
checkpoint must include 5445, 5450, 5459 and 5466. Existing proof checking stays
asynchronous; an actual correctness failure pauses dependent work.

Next compare 5470/5471 full causal streams, score full bound campaign 5465
when complete, and evaluate watch-pair timing only with all exact-work pairs.
The scoped validation index and mandatory Andrew TeX/PDF own final audits.
Production's fixed suite passes 77/77 in 20.49 s. The initial report rebuild
used an unavailable module name (`texlive`); the installed `amh-tex/2023`
module supplies the required compiler for the two successful passes.
No manual data deletion, promotion, push retry or separate-checker contract
change occurred. Push remains unapproved. Living docs stay within their limits.
