# Largest-gap causal screen: search effects outrank small loop changes

Current6730 remains unpromoted at8300.164/7239.010=1.146588277x fixed Kissat,
with50/50 checked outcomes. The ≤1.01 target is unmet. Production and the fixed
reference are unchanged. Ranks31/35 account for422.062s,39.77% of the current
net gap; their selection is owned by the [priority report](2026-09-10-largest-gap-priority.md).

## Current release profiles

[7004](../runs/20260910-7002-largest-gap-release-profiles/solver-summary-7004.json)
validates all four expected UNSAT solves, exact own unprofiled counters and
certificate bytes/hash, every candidate non-timing statistic, and zero lost
samples. These are ordinary release/native binaries, not stopped trace builds.
At sealing all four checks remain pending; this permits investigation only.

Candidate search propagation contributes38.61%/37.00% exclusive user-cycle
samples on31/35, private probing propagation5.26%/4.95%, shrink3.68%/3.37%,
and heap descent2.88%/3.50%. Native search propagation is43.49%/43.60%.
Different symbol boundaries and different work prevent treating those percentages
as a unit-cost ratio. Inclusive callpath ancestors overlap and must not be added.
The wrapper replaces solver perf-stat, so it supplies no solver IPC; its own
memory can dominate measured profile RSS. Use unprofiled records for memory.

The [cost binding7010](../runs/20260910-7002-largest-gap-release-profiles/cost-priority-7010.json)
also rejects a premature Rank31 cleanup retry: current heap maintenance/stable
insertion text is identical to retained5870 text. Later queue-stamp changes are
not a direct heap repair. Historical5870 has only a changed-file manifest; its
retained text is bound now, not falsely asserted historically executable-bound.
Failed writer7009 assumed a complete old source manifest and raised KeyError
before any output write; it is preserved, and fresh7010 records the limitation.
The earlier5920 full-run/TierC regression remains controlling evidence.

## TransitiveReduce on/off in both solvers

[7006 manifest](../runs/20260910-7006-transitive-largest-gap-ablation/manifest.json)
and [7008 validation](../runs/20260910-7006-transitive-largest-gap-ablation/solver-summary-7008.json)
own the eight-arm screen, job1679645. Each task uses12 logical CPUs,48000MiB,
1200 solver-only seconds,16000 checker seconds and5hours, full width without a
node constraint. Immutable unchanged binaries use solver-only
`TACTIX_SLOT_DISABLE=TransitiveReduce` or native `--transitive=0` in the off arms.
The fixed native denominator is not refreshed.

|Rank|Tactix on / off seconds|Kissat on / off seconds|Tactix off/on conflicts|Kissat off/on conflicts|
|---|---:|---:|---:|---:|
|31|484.144 /453.047|264.086 /528.416|0.813912|2.495087|
|35|509.292 /366.555|305.254 /262.884|0.526141|0.863509|

All eight solvers return expected UNSAT with no solve loss/timeout. All on arms
replay own prior work/certificates exactly; candidate on arms also reproduce all
non-timing statistics. Off-arm zero activity is observed, not inferred from
options or missing statistics. All eight proofs are pending at the7008 snapshot.

Rank35's142.737s reduction is much larger than direct TransitiveReduce cost
(about5s): downstream work is the promising lever. Its conflicts nearly halve,
but decisions rise18.9%; fewer conflicts alone do not establish stronger work.
Rank31 gains only31.097s while native becomes about2x slower when disabled.
Thus blanket disabling is not the answer. The ablation changes source setup,
binary prioritization, units, deletions and later scheduling/search. It neither
isolates one deletion nor proves faulty implementation or global CDCL parity.
One screen is not three-wave timing acceptance, a default adoption, or a full50
gain. No solver source or policy default changed.

## Next discriminating experiment and tests

Previous Rank35 late-stage witnesses compare6424 with Tactix6305, not native.
Their legitimate deletion must not be undone merely to recover an older branch.
New [7011](../runs/20260910-7011-rank35-current-native-prefix/manifest.json),
job1679655 since08:15:01EDT, compares current6730 and the cached native diagnostic
pin through1M conflicts, streaming from100K. Both must independently reproduce
the historical40,636,523-row100K hash before any suffix claim. Validator7013
checks clean stopped execution, bound traces, complete suffix accounting and the
first actual native split. Changed inherited hashes require earlier localization.
This is a two-task25-minute stopped diagnostic, not proof/timing acceptance.

The new environment wrapper scopes declared knobs to the exact solver invocation,
restores pre-existing/absent values even on failure, and leaves checker calls
untouched. The activity reader rejects missing/duplicate/malformed rows and
mode mismatches. Eleven new unit tests pass; the full benchmark-tool suite is
273/273. Existing prefix tests cover the reused observer/reader; no solver
instrumentation changed. Future solver edits still require explicit test review.

Defer small isolated ITE6969 full50. Preserve4/32, stronger33/49,46, N1–N4
and the rule/strategy boundary; Rank21 remains shelved. No promotion, fuzz access,
FlatLists redesign or denied push/transfer retry. Proofs continue asynchronously;
the successor audit owns later final statuses. No benchmark material was manually
deleted. Andrew's report is local and unsent. Living-document caps are unchanged.
