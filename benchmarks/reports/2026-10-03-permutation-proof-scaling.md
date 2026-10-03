# Bounded permutation-proof scaling

## Question and method

The [checked row-cover microproofs](2026-10-03-permutation-cover-proof-feasibility.md) establish correctness at small sizes, not practical cost at the 117-by-117 target. Test the same BDD construction in count-only mode on synthetic n-by-n row-AMO/column-cover matrices with the first row negated. This is a feasibility probe, not a target solver run, certificate verification or baseline refresh.

SLURM array 1747634 ran all four tasks at full width on cpu-q, one CPU/32 GiB each, ten-minute allocations and 300-second process limits. The immutable pin is `build/pin-permutation-cover-count-aeb1e04e-01`; the [run manifest, sources and results](../runs/20261003-permutation-cover-count-scaling/) bind its binary and runner. The explicit diagnostic limits were one million BDD nodes and ten million proof additions, not proposed production defaults. No proof stream was written. A fixed n=4 control exactly reproduced the previously checked 264 nodes/1,610 additions.

## Results

| n | Completed row/column blocks (of 2n) | BDD nodes | Counted additions | Outcome |
|---|---:|---:|---:|---|
| 8 | 16/16 | 3,663 | 26,704 | construction completed |
| 16 | 32/32 | 52,172 | 491,655 | construction completed |
| 32 | 63/64 | 768,253 | 10,000,000 | addition allowance exhausted |
| 117 | 22/234 | 1,000,000 | 5,738,404 | node allowance exhausted |

All task wrappers completed with exit 0 and retained explicit `complete`/`stopped` results; scheduler completion is not certificate success. Watcher 18412 was collected after authoritative completion. Short sampled SLURM MaxRSS values are not reliable peak-memory estimates for these brief subprocesses and are not used as such.

The n=117 cap occurs during the row phase, before all row bounds or any column aggregation is complete. Do not integrate this naive whole-matrix BDD expansion as the current full-target proof strategy. The result rejects this bounded implementation path, not the row-cover theorem, all DSR proofs, or theory-to-Boolean integration. It also does not establish that pseudo-Boolean proof is necessary. The synthetic pairwise-AMO input does not exploit the target's existing interval definitions; that distinction matters.

## Next design work

Pursue compact arithmetic/extension derivations that reuse certified interval structure, rather than materializing every pairwise exclusion and global BDD intermediate. Assess each candidate with a bounded feasibility control and independent checks before integration. Keep the proof-only helper lifetime separate from live search variables and record every published constraint's semantics and derivation.

In parallel, the 19,071 already-certified interval auxiliaries provide a route to semantic branching without constructing another encoding. Review public Reorder/Decide reuse, identity and provenance, EVSIDS/VMTF behavior, and comparison of unchanged constraints with/without structural guidance. This is an investigation, not permission for a guessed boost constant or a default-only controller. Preserve Factor's reference demotion behavior; theory-variable advice requires a separate explicit policy choice.

Retain the compact Ramsey certificate path and the full fixed 36/44 target scope. No production change, new checked target, N1–N4 pass or regression acceptance is claimed. Both-policy fuzz and current-pin Debug remain separate pending work. Completed synthetic input files were removed after hashing; `cleanup.json` records exact paths/sizes and the retained generator recreates them. Native source, count records, pins and all prior proof evidence remain intact.
