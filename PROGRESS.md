# Progress and resume guide

## Active ordered goal

**Phase 1 comes first.** Independently check Rank 1--50 at 50/50 and B28+C5 at
33/33, each case within 1,200 seconds and 32 GiB. Aggregate solver time must be
below the frozen R16 gates: Rank below 8,115.735s and B28+C5 below 952.263s.
Preserve WV61 coverage, public rule selectability, native DSR proofs and original
CNF models. Rank 6 must remain no slower than frozen Satsuma+Kissat's 203.321s
under comparable hwu/c4/32 GiB conditions. Repair Rank 1 and Rank 6 overhead with
general mechanisms; individual trajectory regressions are acceptable only when
both complete suite aggregates improve.

**Phase 2 starts only after Phase 1 passes.** Reach independently checked WV100
100/100 within 1,200 seconds and 32 GiB per case by integrating each owning
baseline's techniques into Tactix. No sequential portfolio, external solver,
benchmark dispatch, or arbitrary benchmark-tuned cutoff is allowed.

Use cpu-q for one-off profiles and coarse solved/timeout iteration. Use matched
allocations for close runtime claims and the final aggregate gates. All solver
runs use SLURM, full-width arrays, immutable pins, anonymous scratch, and the
native proof/model oracle.

## Current checkpoint

The root source is the coherent **AM56 native AMO-provenance BCE** checkpoint:

- Authority: `build/20260917-120206-am56-native-amo-bce-authority/manifest.json`.
- Snapshot: `build/source-20260917-120206-am56-native-amo-bce`.
- Source tree SHA256: `02093cb73198632e00c4fcbae5c14d34e8f894a46f0507172bb92d6a9bfe11bf`.
- Bit-identical Release A/B binary SHA256:
  `2a2fbb4c355a5c741bcdd92c09c09b7d45c3579b0b9b3448f1c086b28cda09d0`.
- All 343 manifest source files match the root byte for byte.
- Release 161/161 passed in 199.69s. Focused ASan/UBSan/leak tests and fixed
  proof/model oracles passed.

AM56 runs canonical BCE once after structured processing only when that scope
actually published unique-literal AMO helper binaries. The monotonic provenance
counter excludes caller-supplied SR. The policy therefore recovers Rank 26
without imposing the harmful cleanup observed after unrelated SR progress.
Implementation details and prior rule contracts remain in
[implementation notes](docs/implementation-notes.md).

## Decision-relevant evidence

The current complete Phase 1 vectors are still AM36 and fail acceptance:
B28+C5 is 33/33 in 1,414.317s; Rank is 49/50 in 10,520.336s with Rank 29 timed
out. Later focused work recovers Rank 29 and Rank 26, but focused unions do not
replace a coherent full gate. The required aggregates and independent checks
remain open.

AM56's independently checked focused screen is:

| Case | Solver seconds | Result |
|---|---:|---|
| Rank 26 | 399.586 | SAT model verified; 1,089 AMO binaries trigger BCE, 1,084 helpers deleted |
| Rank 1 | 6.025 | UNSAT proof verified |
| Rank 6 | 122.575 | SAT model verified; below the 203.321s Satsuma gate |
| B28 rank 7 | 3.672 | SAT model verified; no unrelated BCE |
| WV88 | 92.482 | UNSAT proof verified |

AM57's fixed 128-generator prefix is rejected. It improves WV88 to 41.321s and
Rank 26 to 306.742s, but regresses Rank 1 to 9.580s and Rank 6 to 324.775s; all
four outcomes independently check. Keep its frozen run as negative evidence,
not source policy.

Rank 1's gap is SR preprocessing. AM56 spends 5.775s in the structured root and
5.614s in unattributed structured work, applies 615 SR clauses, then proves UNSAT
with one conflict. Its expensive inclusive phases include 2.936s of orbitopal
generator-orbit work and 0.956s of generic detection; clause application itself
costs 36ms.

Frozen Satsuma+Kissat solves the same Rank 1 in 1.218s total with a verified proof
(`sc24-26-32g-5000s/results/00563.json`). Diagnostic job 1699170, using the exact
frozen binaries and input on cpu-q, measures 1.09s Satsuma plus 0.18s Kissat.
Satsuma detects 614 generators, then follows its baseline-owned large-domain
path because 85,230 variables exceed `schreier_domain_limit=50,000`: negation
fixing publishes one unit, binary fixing publishes nothing, and a second pass
stops after no further consequence. It does not build the full clausal Schreier
chain that produces Tactix's 615 clauses. Detailed evidence and qualifications:
[AM55--AM57 and Rank 1 attribution](benchmarks/reports/2026-09-17-shared-discovery-amo-cleanup-and-rank1-attribution.md).

## Active work and exact next action

Storage guard PID 2967585 owns allocation 1697679 and must remain live, fresh,
and clear before submissions. Claude owner PID 3454766 is still working under
`build/am55-claude-shared-sr-discovery/`; read only `handoff.md` or
`terminal-report.md` when present, never raw owner streams. **NOTHING IN FLIGHT**
in SLURM beyond the storage-guard allocation; no solver/checker arrays remain.

Next, integrate Satsuma's large-domain negation/binary fixing behavior into the
native typed structured strategy. Use the owning baseline's 50,000-variable
boundary and exact publication-driven repetition, preserve the unrestricted
public graph-SR action, and keep the earlier useful-inference admission path that
can finish Rank 1 before SR. First require Rank 1 to match or beat 1.218s without
Rank 6 regression; then screen Rank 26, B7, WV75/77/88 and the known early-pass
controls. Run complete B28+C5 and Rank 1--50 gates only after the focused candidate
is coherent. After Phase 1 passes, resume WV100 expansion.

## Recent changes

- AM56 made post-AMO BCE admission depend on actual native helper publication and recovered Rank 26.
- AM55 shared one non-refilling SR discovery allowance across the structured root and brought Rank 6 below its case gate.
- AM54 added exact within-pass row feasibility feedback and unique coloring ownership.
- AM48 reused prepared gate extraction for useful early-Congruence admission, yielding a checked 0.317s Rank 1 diagnostic.
- AM34 repaired forward Congruence's root propagation closure; independent fuzz found no candidate defect.
