# Uniform DRAT binary input identity

## Result

DRAT and proofless production no longer retain a permanent exact-key archive
for binary clauses. During DIMACS import they use a compact no-false-negative
filter followed by exact mirrored-watch confirmation. Native LRAT continues to
retain the exact binary-pair-to-proof-id map.

This deletes the former `2^24` predicted-binary cutoff. There is no replacement
formula-size, density, benchmark, rank, identity, or classification gate. The
filter grows by its ordinary representation load, and every positive answer is
confirmed against solver state before a duplicate is rejected.

## Consumer audit

After input normalization/restatement is complete:

- native LRAT consumes exact binary proof ids in Substitute and proof chains;
- DRAT names clauses by their literals and consumes no binary id value;
- proofless solving consumes no proof identity;
- binary liveness is answered exactly by mirrored watches;
- `BinaryIdMap::size_` remains a monotone material watermark for Factor and
  scheduling diagnostics, even after key storage is released;
- the compile-time rule journal derives distinct pairs from watches when keys
  are absent and does not become a production consumer.

Thus the change is representation and proof-format wiring. It changes no rule,
admission, effort, clause creation order, classification consumer, or default
strategy.

## Mechanism

The DIMACS header seeds a three-hash bit filter at four bits per predicted
binary entry. An absent filter signature proves the pair is new. A possible
hit scans the smaller exact binary-watch population. If actual binary input
exceeds the advisory prediction, the filter expands at its representation load
boundary and is rebuilt from existing mirrored watches. This load-factor
growth affects cost only; it cannot change clause acceptance.

Native LRAT bypasses the filter and reserves the exact key/value archive. The
input filter is scoped to parsing and released before solving. Headerless/API
input can release an already-populated non-LRAT archive at `finish_input`
without losing its exact count.

## Focused correctness

The parser regressions cover:

- an intentionally underdeclared header that forces filter growth;
- 20 distinct binary pairs followed by 20 exact duplicates;
- one mirrored copy per retained pair;
- preservation of the monotone count after storage release;
- native-LRAT key and id retention;
- proofless/DRAT storage release.

DRAT and native-LRAT micro proofs, proof-rule synchronization, the rule journal,
and the zero-overhead event-trace check pass. The Release suite passes 76/76.
Focused parser and binary-lifecycle tests pass under ASAN/UBSAN. LeakSanitizer
cannot run under the environment's ptrace wrapper, so the sanitizer run uses
`detect_leaks=0` rather than claiming leak coverage.

## Rank 5 memory witness

Rank 5
`1c21a43aa1ad437e4a7e4eb5b9cd45a1-9.normalised.cnf.xz` exercises the former
large-input cutoff under the real binary-DRAT path.

| measurement | candidate | accepted Tactix | Kissat |
|---|---:|---:|---:|
| peak RSS (KiB) | 10,569,972 | 10,530,588 | 10,402,844 |
| internal solve (s) | 3.384 | 3.434 | — |
| decisions | 0 | 0 | — |
| conflicts | 0 | 0 | — |
| propagations | 58,640,574 | 58,640,574 | — |

The candidate model independently satisfies all 157,745,317 clauses with all
58,633,530 variables assigned. Candidate RSS is 0.4% above accepted Tactix and
1.6% above Kissat, so the uniform mechanism preserves the already-close large
formula memory behavior without a special cutoff. Local wall time is not used:
writing the 520-MiB model dominated that measurement.

## Rank 6 binary-heavy witness

Rank 6
`7a5e9bdbf9b8c025b1f986d6098b6206-cfi-rigid-t2-0048-04-or_3_shuffle_all.cnf.xz`
preserves the exact accepted trajectory:

- 551,149 conflicts;
- 5,224,515 decisions;
- 200,773,083 propagations;
- 49 factorings and 758,605,085 Factor ticks;
- two BVE eliminations and 98,839 resolutions.

Its model independently satisfies all 11,099,040 clauses.

| measurement | candidate | accepted Tactix | prior pushed | Kissat |
|---|---:|---:|---:|---:|
| peak RSS (KiB) | 705,312 | 1,287,052 | 829,152 | 350,588 |
| wall time (s) | 94.99 local | 99.778 cluster | about 109 local | 54.299 cluster |
| internal solve (s) | 91.952 | 97.169 | about 106 | — |

Peak RSS falls 45.2% from the accepted baseline and 14.9% from the preceding
key-only DRAT representation. Tactix still uses about twice Kissat's Rank 6
memory; the remaining dominant transient is Factor's global watch-arena growth,
not a formula-policy discrepancy.

## Pricing status

The local witnesses authorize committing the uniform mechanism. They do not
replace homogeneous pricing. The next run is one 40-task Tactix candidate arm,
joined per rank with the immutable accepted-Tactix and current-Kissat results in
`20260828-1450-tierd-ranks1-40-dense-binary-cost`. UNSAT solver timings are
published provisionally on solver completion and never wait for proof checking.
