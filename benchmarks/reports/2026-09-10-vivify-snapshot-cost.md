# Vivify snapshot-cost experiment and scheduling-default correction

## Outcome and current authority

Isolated6822 omits a redundant temporary literal copy in one existing Vivify
mode. It is a same-work implementation experiment, not an adopted optimization.
Current6730 remains the runtime authority; production is unchanged. The user
prioritizes inprocessing unit cost while retaining scheduling and CDCL parity.
The full50 ratio remains8300.164/7239.010=1.146588277; all50 current certificates
are now checked/clean, but the runtime and memory targets remain unmet.

[Build6822](../runs/20260910-6822-vivify-snapshot-build/manifest.json) pins
Release/BC SHA256
`86a2f2556d3eb595371a2bdb9e7ef77cc6883d1601590f3b5d36a8b4b2e5f4fa`;
trace SHA256
`249d0f277686f138dbd1ee1d55ce7d69bcffd9b264ad5832706274633421925e`.
The source clone, cumulative/incremental patches, complete source hashes,
test logs and immutable parent6730 binding live in that campaign.

## Evidence and change

[Profiles6817](../runs/20260910-6815-inprocessing-release-profiles/solver-summary-6817.json)
validate four unchanged Release/native sampled runs on32/39: expected UNSAT,
exact own-arm work and proof bytes/hash, exact candidate non-timing statistics,
and zero lost samples. Pending independent checks were not called accepted.
Rank32 has10.70% inclusive Vivify samples, including2.99% scheduling;
Rank39 has16.08% inclusive Sweep, with7.44% self in embedded propagation.
These percentages select inspections, not common operation prices.

Current Vivify copies every candidate's literals into a snapshot array. Pure
irredundant schedules without prefix flushing consume only the maximum
occurrence count; they neither commit literal pose nor compare literal prefixes.
Kissat's `init_countref` reads that maximum directly from the clause with
`COUNTREF_COUNTS == 1`. CaDiCaL likewise derives compact occurrence ranks from
clauses, but its distinct tie policy is not copied over the Kissat tie-break.

The experiment suppresses literal snapshot appends only when scope2 and
flushing is off. After both unchanged root-flush passes, it computes the same
maximum from each surviving clause reference. First-pass replacements retain
false priority; the second pass processes true priority. Thus the second pass
does not rewrite previously appended first-pass candidates. Unit/refutation
returns precede sorting. Mixed/public scopes and flushing retain full snapshots;
the existing no-sort regime still uses logical arena order. Candidate ABI stays
40bytes/alignment8. No new constant, API filter, kernel primitive or layout.

[Scope6834](../runs/20260910-6822-vivify-snapshot-build/scope-6834.json)
binds source/reference and unchanged guard/effect/public sections. Only
`vivify.cpp`, the new fixed test, and an explicit return in its reused test helper
differ from6730. Policies, admission, cadence, effort ceilings, meters, reason
selection, CDCL, proof/reconstruction and full legal action surface are unchanged.
C3 calculus and implemented guards were reviewed and need no semantic revision.

Avoided copying is not a measured speedup. Arena rereads may offset it, and
earlier redundant schedules can retain snapshot capacity. Do not claim peak
memory savings from this source change alone. Older5546 unused-census timing
was noisy, and6383 leading-count cache timing was neutral; neither is evidence
for this experiment or grounds to rerun those campaigns for favorable noise.

## Validation and failed fixture

[Control6824](../runs/20260910-6824-vivify-snapshot-control/control-6824.json)
links actual old/new Vivify objects against the same current parent objects.
Both produce exact432 baseline plus216 new population/root-rewrite/scratch
state/model/proof-digest cases, and144 candidate-order cases. The new matrix
covers both sides of the10:1 population regime, three scopes, reuse/flush,
root-false/root-satisfied clauses, cut/repeated rounds, DRAT/LRAT and transitions
back to mixed/flushing queries. Every dose preserves the exhaustive8-variable
model set; queries are pure and agree with public enumeration.

Full independent Release/BC/trace suites pass113/113/112. All
[48 fixed DRAT/LRAT proofs](../runs/20260910-6827-vivify-snapshot-proofs/summary.json)
independently verify; their generated inputs/proofs were removed by their
owning checker helper,65799bytes, regenerable from the retained fixed fixture.
These tests are not random-policy fuzz or broad current-source fuzz acceptance.

The initial6823 parent fixture crashed before candidate compilation because I
renamed an existing C++ `main` as a helper without adding its required explicit
return. Only `main` receives the implicit return0. The compiler warning,
failed unchanged-parent executable/logs and pre-fix source copies are preserved
in [failure6825](../runs/20260910-6822-vivify-snapshot-build/fixture-failure-6825.json).
The private test helper now returns0; fresh6824 passes. No solver crash,
rejected proof or soundness claim is inferred from this test-harness failure.

## Broad gate and predeclared timing

[B28+C5 campaign6828](../runs/20260910-6828-vivify-snapshot-BC-gate/manifest.json),
job1678866 since00:50:58EDT, uses independent Release/BC pins and unchanged
tier composition. [Strict6830](../runs/20260910-6828-vivify-snapshot-BC-gate/summary-6830.json)
has all66 expected results, all33 exact work/certificate pairs,64 checked/clean.
The remaining checks continue asynchronously; audit6836 owns later states.

[Runtime6831](../runs/20260910-6831-vivify-snapshot-runtime/manifest.json)
predeclares three matched waves on ranks32,39,38.32 is the broad inprocessing
control,39 the Sweep-heavy transfer control,38 a known code-cost-sensitive
low-slot adverse control. Each wave has six independent tasks; middle wave
reverses arms. Both arms must match the same current6730 per-rank work and
certificate oracle. Wave0/job1678932 was submitted00:57:11EDT. Advance with6832
only after exact preceding solver records, not after proof completion;6833
summarizes all18 outcomes without selecting favorable runs.

Resources remain12logicalCPU/48000MiB,1200solver-only seconds,16000checker
seconds in5hours, full width/no node constraint. Require all18 certificates
checked/clean and report all paired ratios, medians/ranges, RSS and arm spreads;
absolute claims require both arm spreads≤2%. This is no fixed50 denominator
refresh, source promotion, full50 acceptance or global parity proof.

## Scheduling ledger correction and next action

`SP-VIVIFY-3` incorrectly retained DEFAULT ON. Production/current code defaults
`vivify_flush=false`; frontend changes it only for explicit environment input.
Commit3fdaf955 changed true→false on2026-08-12. The row now says NON-DEFAULT ARM,
retaining the original adverse enabled-arm evidence and reopening condition.
This corrects documentation, not policy. Option coverage passes418 reference
options with0 unaccounted; coverage alone does not prove behavioral parity.

Next advance all predeclared timing waves, inspect paired total/Vivify costs and
the adverse control, and retain or reject6822 from evidence. No further Rank21
search probes. Rank31/35 roughly2×native conflicts and21 changed-work gap remain
open; stronger33 and established4/32/49/46 closures remain protected.
Source fuzz and memory acceptance remain open. No fuzz artifact access,
FlatLists redesign, denied transfer/push retry or promotion. Andrew's local
0058 report is not sent. Winddown05:45/cutoff06:00EDT September10 remains in force;
existing jobs are not automatically cancelled.
