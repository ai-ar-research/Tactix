# Telemetry wave 0 and Factor call-site follow-up

Frozen diagnostic evidence, 2026-09-10, 23:11 EDT. Not a final acceptance decision.

## Telemetry first paired wave

All six wave0 outcomes are expected UNSAT. Strict7175 validation confirms exact
retained7144 work and certificate fingerprints after excluding only timing and
the explicitly checked binary-telemetry row. Independent checking continues.
No proof wait preceded wave1 submission1680423 at23:09:31; its watcher is28345.

| Rank | Ablated seconds | Control seconds | Ablated slot ms | Control slot ms | Ablated outside-slot ms | Control outside-slot ms |
|---:|---:|---:|---:|---:|---:|---:|
|31|531.024|527.416|168050|168453|362766|358736|
|35|361.350|357.878|124735|125501|236404|232159|
|38|338.355|338.705|22985|22669|315321|316009|

The31/35 inprocessing slots are slightly faster, but whole runtime is worse:
the larger adverse movement lies outside the slot. Rank38 is nearly neutral.
This does not prove an instrumentation benefit or a code-placement cause. The
ablated image changes downstream code addresses; wave0 placed candidate on
node018 and control on node017. Preserve the predeclared three waves, with
reversed arm order in wave1, and report paired medians/ranges plus RSS and
within-arm spread. No repeat-to-select or premature promotion/rejection.

## Factor: the suspected copying hotspot is mostly elsewhere

New7178 partitions every saved Factor sample into disjoint immediate children,
their caller PCs and leaf costs. All three waves reproduce7171's inclusive
envelopes and child totals exactly. Two tiny unresolved unwinder weights
(18,419,127 and18,961,773 periods) are explicitly quarantined, not assigned to
invented call sites. The initial strict reader failed on a genuine non-call
instruction preceded by an unknown frame; the corrected reader retains that
weight and still rejects ordinary invalid caller PCs. New controls exercise
self, descendants, foreign DSOs, unresolved frames and five invalid cases.

The main vector-push child in wave0 costs4,244,933,384 periods. Of that,
3,202,330,446 is at adjusted caller PC0x481536: the out-of-line
`touched.push_back(candidate)` on the cofactor count's zero-to-one transition.
Its matching values in waves1/2 are2,219,092,642 and3,428,453,045 periods.
This is **not quotient-grid row copying**. Reviewed assembly at0x480241 tests
the cofactor count; the zero branch goes to0x48151d, calls vector push at
0x481532, reloads the cofactor pointer, then rejoins the increment at0x48024b.
The source owner is `factor_apply_seeded`'s `count_candidate` lambda. Its
first-discovery order is meaningful for ties and must be preserved.

Index construction's wave0 inclusive10,336,497,198 periods contain
6,353,755,407 self periods; substantial descendants are allocator leaves,
including malloc1,612,155,051, internal malloc948,279,608 and internal
free455,167,401 periods. These are disjoint leaf samples, not extra costs to
add to the inclusive total. Direct main-function release at caller0x47ee73
costs1,926,960,965 periods, with1,777,725,650 and1,943,771,645 in waves1/2.
Assembly identifies the24-byte-stride destructor loop for `FactorIndex.order`:
the same stack object at0x4c0 is passed into `build_factor_index` at entry.

## Next structural choice

The evidence supports examining Factor's independently allocated per-literal
lists and its deeply nested count-candidate hot loop, rather than starting a
grid-copy rewrite from a misleading generic vector symbol. A contiguous
occurrence layout could remove many allocations/releases, but must preserve
the borrowed binary prefix, large/dynamic encounter order, lazy remainder IDs,
stable deletion, live degrees, fresh-variable growth, exact logical ticks and
proof transactions. Existing kernel FlatLists accepts only four-byte words;
Factor records are eight bytes. Do not silently broaden its contract or add a
new capacity restriction. No such representation rewrite is implemented here.

Prioritize complete Factor cost and memory over merely shortening a function.
The saved full envelope remains53.88–54.04B candidate versus32.62B native;
inlining differences prohibit treating entry-self alone as a fair denominator.
The telemetry experiment remains bounded and separate from this larger lead.

## Authorities

- `benchmarks/reports/2026-09-10-factor-child-sites-7178.json` owns sample hashes,
  exact caller maps, conservation checks and unresolved weights.
- `benchmarks/runs/20260910-7170-factor-pc-localization/summary.json` owns exact
  instruction inventories and their frozen disassemblies.
- `benchmarks/runs/20260910-7175-runtime-telemetry-runtime` owns paired solver
  records, immutable manifests/pins, launches and later independent checks.
- `benchmarks/reports/2026-09-10-runtime-evaluation-audit-7173.md` owns the full
  fifty comparison:1.115077752x fixed Kissat; target still unmet.

Test consideration:7178 adds source-reader tests and full sample conservation;
no solver code changed this follow-up. Existing7175 strict fingerprint controls
protect the continuing timing experiment. No fuzz artifact access or cleanup,
new production promotion, benchmark runner mutation or denied push retry.
