# Rank569 FastEliminate self-strengthening correctness checkpoint

This checkpoint freezes the bounded, unaccepted implementation increment that
ports SC2024 FastEliminate's noncollapsed feasibility self-strengthening. It is
not a Rank569 closure or a DefaultPolicy acceptance result. Causal timing,
fuzz, and the required regression families remain open.

## Source-backed discrepancy and scope

The startup census in
`benchmarks/runs/20261007-rank569-fastel-pivot-census-r2/result.json`
found ten native-only eliminated pivots: 35, 52, 69, 86, 103, 120, 137, 154,
171, and 188. Each native success used feasibility-time self-strengthening
before the absolute resolvent cap. The earlier prototype made the patched
Tactix trace match Kissat 4.0.4 with the SC2024 `fastel=1` setting through the
first 50 conflicts; `benchmarks/runs/20261007-rank569-fastel-self-strengthen-r1/comparison.json`
records 223 decisions, 49 analyses, 49 learned clauses, and 7,275 implications
with no selected-event divergence. That comparison predates the final
source-identity repair and is diagnostic evidence, not a timing claim for this
pin.

The final increment enters the mutating feasibility path only when the raw
positive-by-negative cross product exceeds the typed absolute cap. A
non-tautological resolvent of size at least two that strictly subsumes a live
large parent is emitted through the existing two-parent RUP path, replaces the
parent, and does not count toward the cap. The checked replacement remains if a
later pair makes elimination decline. Linear literal marks check containment
inside the already charged resolution attempt. Tentative resolvents retain the
physical `Ref` of each large parent, so replacing a source prunes its stale
images identically with no proof sink, DRAT/DSR, or LRAT; proof clause IDs are
not used as source identity. The conservative feasibility count is not refunded
after a later replacement, matching the native scan's accounting.

At or below the raw cap, ordinary resolution runs unchanged. Native deletion
and `fast_strengthened` counter parity for collapsed unit or empty feasibility
results remain open; existing BVE still publishes those checked consequences.

## Immutable pin and focused validation

The final source is baseline `1e8295106b34616eefe5f434b4fc72a4654a3527`
plus patch SHA-256
`9de4ab1b83e52d6402dc846170764e28325719f7e1911d9b856d9967dfd28163`.
The source archive SHA-256 is
`5e6a99da098829a44e63e78f4c84d0f046acd66d35956fae08d59a35e80ea8ec`.
The production Release binary is
`3a403b2038649a0ec82515a32b9b221aea0727a7f5369fef88f773841acb3cba`;
the same-source unpatched Release control is
`27cf0cab924c52479d6c11885d284ae461d6c4911ef2e557847f58bad87e9469`.
Artifacts are under
`benchmarks/runs/20261007-rank569-fastel-self-strengthen-r7/`.

- SLURM 1754384: focused Debug completed with `test_bve_invocation`,
  `test_bve`, and `test_scheduling` passing. Coverage includes success before
  cap zero, no feasibility strengthening at the raw cap, no-proof stale-source
  pruning, retained progress after decline, and the atomic invocation-budget
  boundary.
- SLURM 1754386: the same three targets passed under ASan/UBSan with empty
  stderr.
- SLURM 1754428: a 2-second Rank569 ASCII DRAT prefix, using
  `--default-graph-labeling=off --default-structured-symmetry=off --drat
  --no-binary`, produced 16,802,938 bytes, SHA-256
  `9ac62abe23bbbf51fae0456b62328564e8e57ca96b00ac7526ccb6acbb02f1a3`.
  Full forward DRAT/RAT checking reported 56,087 of 66,481 lemmas in core,
  zero RAT lemmas, and `VERIFIED derivation: all lemmas preserve
  satisfiability`. Status 1 and `s DERIVATION` are expected because the bounded
  prefix contains no empty clause. This check is distinct from the production
  `--dsr` route used by the full candidate.
- Documentation size, calculus/guide synchronization, reference-option
  coverage, section references, citations, and `git diff --check` pass.

## Pending causal and acceptance evidence

SLURM 1754435 runs the unpatched and patched Release binaries sequentially for
300 seconds each on the same allocated core with production defaults and DSR
output. SLURM 1754440 is the one-core, 32 GiB, 5,000-second Rank569 candidate;
its manifest records the 3,359.975-second target threshold and routes any
terminal DSR certificate to the pinned independent checker pipeline. Neither
pending job is evidence of benefit yet. Required fuzz, B28+C5, Rank1–50 WV,
WV100, and WV2 qualification remain open.
