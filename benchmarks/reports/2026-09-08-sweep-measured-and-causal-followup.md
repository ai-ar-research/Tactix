# Measured Sweep cursor gain and causal follow-up

## Measured implementation gain; full successor submitted

Campaign [5475](../runs/20260908-5475-sweep-cursor-rank35-runtime/solver-checkpoint.json)
has six expected UNSAT solver results. All three pairs have identical complete
non-timing statistics and proof SHA-256. Median candidate/predecessor runtime
is **0.984562853x**, range **0.983792262–0.989553381**, about a **1.54%** local
reduction. All ratios improve; candidate spread is 0.4278%, predecessor 0.3429%.

| Pair | Candidate s | Predecessor s | Ratio |
|---|---|---|---|
| 0 | 345.177 | 348.821 | 0.989553381 |
| 1 | 344.345 | 350.018 | 0.983792262 |
| 2 | 343.704 | 349.093 | 0.984562853 |

All proof hashes are `c6d148443294bda8c11ee6dba357fbbf02145447671b6b7d4d547f1d6df81398`.
**0/6 were independently checked at the frozen solver snapshot.** Byte identity
is not a proof check. The source-backed private membership optimization is
retained provisionally, not promoted. [Implementation evidence](2026-09-08-sweep-membership-cursor.md)
owns the 588 old/new fixtures and both 84/84 candidate suites.

New [5485](../runs/20260908-5485-sweep-cursor-ranks1-40/manifest.json), job
**1671708**, evaluates current source **5473** against native Kissat over the
unchanged full Rank 1–40 matrix: 80 independent proof-on tasks, identical
runner/checkers/resources to 5465. Candidate SHA is
`305b2285fdda58345bdb020efe41584b3e8a4025df8aa8f4c44a06fa18f3f594`.
It includes watch append and Sweep cursors on the bound repair, not independent
metadata5426. Both B28 arms had all expected solves before submission; C5 and
certificate checks continue provisionally. Gate5476 is now64/66 checked.
Do not multiply separate local timing ratios to predict the aggregate.
The latest complete full view remains **1.173817099x Kissat**, not the required
**at most1.01x**; 5465 is77/80 checked at this report's solver checkpoint.

## Short Square: saved history, not a different phase priority

[5482 phase replay](../runs/20260908-5482-square-phase/observer-validation.json)
preserves each5471 parent complete decision stream and whole-prefix bytes.
The original strict comparator failed on native boundary-byte identity:
enabling the global event trace adds **476 analysis/end rows** before the
phase-only filter, shifting sequence numbers. Native `eventtrace.c` calls the
standalone boundary writer before that filter; its detail helper otherwise
returns without a trace file. All **986 original native boundary rows** agree
after accounting for precisely those extra rows and sequence shifts. Candidate
boundaries are byte-identical. The original failure and immutable comparator
are retained; successor5486 records the normalization with eight fixtures,
including rejection of altered state, unknown events, out-of-window analysis,
wrong stage/sequence and missing boundaries. No solver failure was found.

At the first decision split (501/5849), both solvers select variable43773 from
saved phase, target0. Tactix saves negative at conflict492, decision5612;
native retains positive, last saved at491/5458. The histories differ earlier:
native also saves positive at13/219 while Tactix's first recorded save is198/2225.
Matching decision streams therefore did not establish identical assignments.
Tactix observes phase-pick before incrementing decision/level; native observes
after incrementing them. This accounts for5482 pick-row offsets, not polarity.

New [5487](../runs/20260908-5487-square-implications/manifest.json), job
**1671745**, uses existing selected implication observers through conflict502
to identify reason/assignment ownership. Check full parent replay and bounded
observer normalization before interpreting it. Do not modify phase priority,
assume equal auxiliary-variable definitions, or declare the negative phase a
defect without tracing its earlier cause.

## Rank32/37: decision-relevant component boundaries

[Rank32 prefix5479](../runs/20260908-5479-rank32-bound-prefix/summary.json)
has **18,583,703 common decision rows**. The first decision split is
19375/18523001, level385:224891 versus224889. Its first coarse boundary split
is Sweep exit at19375, after equal entry active159001, irred586474,
red2576, binary213599. Exit active is158995/158994, binary213757/213761;
large-clause counts agree. This is not enough to establish missing useful work.
[5490](../runs/20260908-5490-rank32-sweep/manifest.json), job**1671777**, captures
Sweep schedule, environments, queries, units/equivalences and effort at19375,
stopping just afterward. Require each observed prefix to replay its5479 parent.

[Rank37 extended5478](../runs/20260908-5478-rank37-bound-extended/summary.json)
has **25,001,084 common suffix rows** from the separately validated100K anchor.
Its first split is188502/60180275, level340:26688 versus9940. Do not confuse
the suffix count with a cumulative count. The first coarse boundary split is
Sweep exit152381, binaries151957/151959 with active/large counts equal.
The [next nonbinary-count boundary](../runs/20260908-5478-rank37-bound-extended/nonbinary-localization.json)
is Vivify exit188501: retained redundant clauses13354/13353, with equal active,
irredundant and binary counts. Earlier binary-only differences remain unresolved,
not accepted as neutral by this second view. Both deserve source/state tracing;
neither is yet a proved cause. Rank32's earlier active-variable difference is
the next focused component probe; Rank31's common100K prefix remains recorded.

## Background correctness and validation

Claude owner5442 returned terminal **PASS** on source5438:80,000 cases,
71,907 checked verdicts (42,119 models/29,788 proofs),8,093 nonterminating cases,
240 corpus replays, zero failures;496 direct reachability witnesses. It records
that generated closure entries did not hit pending propagation/unflushed roots;
fixed root-closure fixtures passed. Its authoritative report is
`benchmarks/runs/20260907-5442-periodic-fuzz/report.md`. This statement uses only
Claude's terminal response; no fuzz jobs, logs or artifacts were independently
inspected. It does not cover post5438 repairs.

New Claude-owned checkpoint**5492** runs on current5473 in process**3041017**,
parent session**9171**. It owns immutable copies/builds/harness snapshots, the
changed-surface risk matrix, direct reachability, oracles, cleanup and terminal
report. Generated legs retain100logicalCPU/400000MiB/50workers. Poll only its
terminal response. Actual correctness failures pause dependent jobs and invoke
the bounded repair workflow; pending checks are not promotion authority.

Production CTest passes**77/77 in6.00s**. Eight new boundary-normalization
fixtures pass. New unit tests were explicitly considered: normalization adds
logic and has positive/negative tests; other helpers reuse existing complete
stream/certificate checks, immutable runner and configuration-only observers.
No solver implementation change occurred this turn. Future causal repairs must
add exact-transition coverage. The wrap-up audit records named normal pin,
result/trace, residue, docs and Andrew TeX/PDF checks. No manual deletion, source
promotion, push retry or separate-checker contract change; push remains unapproved.
The first wrap-up attempt rejected an overfull PDF line. Its test-log copy is
retained; the sentence was reflowed and the PDF recompiled twice before the
fresh final audit. This presentation failure did not change solver evidence.
SLURM accounting then became unavailable (`Unable to connect to database`),
including repeated queries for the same job. The final audit preserves that
error and queries the controller for the named normal jobs; purged jobs retain
unknown current scheduler state, not inferred completion. Live queue mapping,
all pin/result/trace hashes, certificate checks, cleanup and docs remain strict.
