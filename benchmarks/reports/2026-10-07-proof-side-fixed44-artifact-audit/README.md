# Fixed-44 proof-side artifact and regeneration audit

## Result

None of the 13 open proof-side rows has a retained complete certificate that
can be rechecked by the repaired 64-bit checker.  Every historical Tier-D
result records `artifact_deleted=true` and `derived_artifacts_deleted=true`.
The October 5 recovery regenerated ranks 291, 541 and 1129 (plus already
closed rank 744), but its four `cleanup_pending` proofs were subsequently
removed; the declared `retained-proofs` directory is now empty.  Rank1004 has
a retained 4,247,188,278-byte file, but it is a partial certificate from an
UNKNOWN 5,000-second run and cannot prove UNSAT.

All 13 therefore require solver regeneration before any fixed64 recheck.  This
audit launches neither solver nor checker.

## Historical evidence

The common historical generator was binary SHA-256
`2fc7421f4d66ed013756e176bd2d6cd0f0bda4876270830704ea2029ed0eb970`
in `benchmarks/runs/20260928-am546-no-mab-tactix-hybrid-tierd-cpuq`.
Every row returned UNSAT inside 5,000 seconds, but no certificate was accepted.

| original rank | solve s | proof GB | old checker outcome | fixed64 closure likelihood |
|---:|---:|---:|---|---|
| 82 | 1301.661 | 2.059 | DRAT route timeout 14400 s | low: not a width-localized failure |
| 226 | 1325.506 | 20.324 | DRAT route timeout 14401 s | low: conversion/checking cost dominates |
| 291 | 245.978 | 0.425 | `dsr-trim` accepted; Trestle timed out | low: width repair does not address independent-check cost |
| 489 | 835.521 | 2.518 | DRAT route timeout 14401 s | low: not a width-localized failure |
| 511 | 736.585 | 5.654 | DRAT route timeout 14401 s | low: not a width-localized failure |
| 541 | 89.874 | 0.248 | witness-free route timeout 14401 s | medium diagnostic: cheap regeneration; direct DSR route unmeasured |
| 569 | 3034.193 | 3.166 | old checker exit 139 | uncertain; current proof has exact fixed64 watch-defect signature |
| 627 | 2062.818 | 5.568 | `dsr-trim` xrealloc failure in 91 s | high: exact 32-bit capacity family |
| 654 | 4366.435 | 12.813 | `dsr-trim` xrealloc failure in 71 s | medium: capacity cause likely, 32 GiB fixed64 memory fit unproved |
| 1004 | 4106.474 | 4.751 | `dsr-trim` xrealloc failure in 100 s | high for old bytes; current source has not reproduced complete UNSAT |
| 1075 | 176.838 | 1.089 | `dsr-trim` timeout 14393 s | low: width repair is unlikely to improve speed |
| 1085 | 1447.393 | 2.689 | DRAT route timeout 14401 s | low: not a width-localized failure |
| 1129 | 80.613 | 0.231 | witness-free route timeout 14401 s | medium diagnostic: cheap regeneration; direct DSR route unmeasured |

Proof sizes use decimal GB.  `audit.json` preserves exact byte counts, hashes,
instance names, pipelines and checker fields.

Ranks 627, 654 and 1004 are the only old runs with the fast `xrealloc` failure
that motivated the width audit.  Their 5.57, 12.81 and 4.75 GB proofs cross the
capacity regime where 32-bit `RESIZE` arithmetic is known to overflow.  Rank627
is the cleanest first closure attempt: its solver time was 2,063 seconds, its
failure is capacity-shaped, and its proof is materially smaller than rank654's.
Rank1004 had the same checker signature and an old 4,106-second solve, but a
newer immutable solver produced only a partial proof at 5,000 seconds.  Rank654
solved under the limit, but fixed64 doubles ID arrays and its 12.8 GB proof may
exceed the 32 GiB checking envelope.

Ranks 541 and 1129 are the cheapest experiments if the direct repaired DSR
route can avoid the earlier witness-free conversion.  The October 5 generator,
binary SHA-256
`d8c1590fa7d873fc94144f64542be4a9cfb9a40ee6cc1dca5c5cba432e873e66`,
reproduced ranks 291, 541 and 1129 byte-for-byte at 248.902, 93.744 and 80.572
seconds.  Its checker-only retry never reached checking because of a frozen
input-hash preflight mismatch, and the proofs were later cleaned.  This is cost
evidence for regeneration, not retained acceptance evidence.

## Current pins and execution order

The intended coherent generator is production checkpoint `0a15ecaf1`, source
archive SHA-256
`ab70cbb55e8d9ea06e2e16a4254ccefd729ad66d9edf3a4cb7a63b428ec7476e`,
and Release binary SHA-256
`b26efc0fb133787a797820d40c759ae552e2cf4eaabde4bee990f27947890bcf`.
Its binding is
`benchmarks/runs/20261007-firstopen-original-seed-release-r2/release-binding.json`.
The focused gate passed; full Debug, four regressions and independent fuzz
qualification remain pending.  Regeneration should use this exact binary only
after those gates accept, so new closures belong to one coherent candidate.

Fixed64 job `1755133` built repaired `dsr-trim` SHA-256
`30207e6f0a6e3b5374976e6ef06528cea21c429383223ba46ae1020b8d7ff564`
and `dsr-compress` SHA-256
`8ddab8302509ff6c3ebe1b6534698d64ff072b202b5b5f903dc4898bf73dd531`.
On the retained Rank147 certificate it reproduced the canonical binary and
text LSR hashes, passed independent Trestle, rejected a malformed proof, and
removed the known long64 failure.  This focused result supports the checker
pin; it does not pre-accept any large proof.

After the coherent solver and checker profile qualify, regenerate in ascending
cost with one full-width array and per-task independent checking.  Start with
541, 1129, 1075 and 291 to expose route cost cheaply, and include the likely
width recoveries 627, 1004 and 654 at full width rather than throttling the
array.  Then cover 511, 489, 82, 226, 1085 and 569.  Each task must retain a
failed complete proof or its exact failing prefix/deletion neighborhood within
the storage contract; an independently accepted checker result is the only
closure.  Rank569 still has the additional user target of a substantial
runtime and proof-size improvement, so a merely verified repeat would close
the fixed-44 evidence gap without completing its performance objective.

The obsolete `10942` retry still has seven old-target diagnostics running,
including original ranks 82, 226 and 291.  They use the defective unpatched
long64 checker and a source already rejected by B28.  Preserve their terminal
diagnostic evidence, but do not merge them into the coherent candidate or
count an output as accepted without a separately bound repaired-checker pass.
