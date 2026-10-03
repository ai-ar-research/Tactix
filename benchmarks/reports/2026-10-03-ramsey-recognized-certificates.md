# Checked Ramsey certificates from recovered signed structure

2026-10-03. Status: three isolated certificates VERIFIED through both canonical
and every-addition DSR checking. Public action, DefaultPolicy integration and
portfolio acceptance remain OPEN. No newly integrated target is claimed.

## Result and derivation

The compact certificate generator now consumes the exact structural recovery
from the [recognition checkpoint](2026-10-03-public-parity-and-ramsey-recognition.md),
rather than assuming edge-variable identities. It recognizes a complete signed
K4 encoding, recovers vertices and oriented edge literals, and independently
checks every recovered source occurrence against the input sequence. All 7,752
clauses are accounted for in each 19-vertex case, and all 6,120 in the 18-vertex
case. Filenames and ranks do not select the derivation.

Witnessed vertex rotations sort a 17-edge star; color exchange with reversal
permits nine positive neighbors. Their induced nine-vertex graph has no positive
triangle and no negative K4. Four positive neighbors are impossible: either two
are positively adjacent, making a positive triangle, or they form a negative K4.
Six negative neighbors are impossible by the conditional two-color triangle
argument on six vertices. Thus every induced vertex has positive degree exactly
three. Summing degrees gives `2 * positive_edges = 27`, a contradiction.
The proof uses direct RUP case splits for the six-neighbor argument and the
existing definitional BDD/RUP counting engine for the integer contradiction.
After the symmetry prefix and induced negative triangles, the proof may delete
all original clauses except the 126 positive K4 clauses on the selected vertices.
This is a terminal derivation, not live inprocessing after arbitrary rewriting.

Two translation details matter:

1. Recovered edge literals may be negative, but the proof API requires positive
   witness domains. A structural map `l -> l'` becomes
   `abs(l) -> (l < 0 ? -l' : l')` before SR serialization.
2. A negative edge is the Boolean indicator `1-x`. In a signed inequality,
   `s * (1-x)` contributes coefficient `-s` and constant `s`, so the normalized
   bound decreases by `s`. Local degree inequalities and their accumulated bounds
   both use this normalization. Merely negating coefficients would be unsound.

The prototype's exact-input certificate scope remains 18 or 19 vertices;
recognition of a smaller complete graph does not imply UNSAT. The calculus now
records the planned startup composition and its proof obligations, while the
engineering notes distinguish these prototype restrictions from an installed rule.

## Measurements and oracle outcomes

Array `1747733` completed all three tasks 0:0 on cpu-q, one CPU and 32 GiB per
case, full width with no node selection. The generator bound remained one million
BDD nodes and ten million sink additions. Recognition used the previously fixed
allowances; no production schedule or target-specific tuned default was added.
Each generator had 300 seconds; canonical and every-addition checking each had
its own 1,800-second pipeline deadline, within a 70-minute allocation. This is
certificate feasibility, not a regression measurement or extended recheck.

| Input | Distinct edges / external extent | BDD nodes | Sink additions | DSR bytes | Generation seconds | Canonical stage seconds | Canonical + every-addition wrapper seconds |
|---|---:|---:|---:|---:|---:|---:|---:|
| Original 19 vertices | 171 / 171 | 9,192 | 74,782 | 1,650,212 | 0.026866 | 7.731647 | 82.189898 |
| Signed sparse 19 vertices, seed 941630 | 171 / 517 | 9,871 | 87,394 | 2,062,035 | 0.0318884 | 10.596492 | 105.705458 |
| Signed sparse 18 vertices, seed 941631 | 153 / 463 | 9,696 | 85,696 | 1,941,251 | 0.0311595 | 10.058066 | 102.748218 |

Sink additions exclude the physical events in the SR prelude. Generation times
exclude parsing and recognition. Canonical stage time sums no-option dsr-trim
and Trestle; the last column includes the separate forward dsr-trim/Trestle
validation of every addition. These are single diagnostic observations, not
whole-solver speedups or performance-gate evidence. Renaming changes BDD variable
order and proof cost, as the table shows.

All three cases have stage exits `[0,0,0,0]` for canonical dsr-trim, canonical
Trestle, forward dsr-trim and forward Trestle. Corrected false-proof controls
then return `[1,1]` with the expected semantic rejection messages. Native proofs,
checker commands/stdout/stderr, event timing and input bindings are retained.

Native proof SHA-256 values:

- Original: `962261ca8cb6018c6ff0eef11d58fd6e5334bdcbd29a7ffcd98d65f9e86fe77a`.
- Signed 19: `cbac706a69613a450e0d05fb75eab526a92daebad663344b47f968e7860bcb83`.
- Signed 18: `2e56bc5db42bc65aac44fc1faf0290f9455387731b9633e10995ca59dede951b`.

The original CNF remains byte-identical to saved rank 630, SHA-256
`9c8ddd5ad86590ef636f46a61ecf4732abf42dadee70fd2be3c0783c08e7160f`.
The two signed cases retain explicit bijections and source provenance.

## Harness failures and corrected controls

The first submission, `1747729`, was cancelled while all three tasks were still
pending. Its inherited one-clause negative fixture produces a checker hash-table
initialization error, not semantic rejection. It supplies no acceptance evidence.
The replacement uses the previously validated satisfiable six-clause fixture,
requires the exact backward and forward false-empty-clause rejection messages,
and passes both fixed self-tests before submission. The known-small-fixture
limitation and the cancelled runner remain visible; no oracle was weakened.

A second defect affected only the recognition-negative controls after successful
certificate checking: the prototype runner gave the corrupted input and output
copy the same path. The output open truncated the test input, so those original
three reported declines per case are invalid evidence of intended corruption
handling. Positive CNFs and checked proofs used distinct paths and are unaffected.
The collection explicitly marks those counts invalid, rather than accepting the
array's exit status as sufficient evidence.

Job `1747738` reruns only the nine affected admission controls with distinct input
and output paths. Missing-clause, duplicate-clause and sign-corruption cases all
return decline before proof creation; each original hash is unchanged and the
copied input is byte-identical. The job completed 0:0. This does not rerun any
positive certificate or extend a checker timeout.

The editable prototype now also rejects input/output-CNF aliases before opening
an output. A separate pin, `pin-ramsey-recognized-dsr-8ff48dd1-02`, passes three
fixed no-mutation controls for direct, symbolic-link and hard-link aliases. The
checked positive certificates belong to pin `-01`; no positive certificate result
is silently transferred to the new pin. The guard does not alter the proof
algorithm, but any later public migration still needs its own complete acceptance.

The initial build script also used the wrong PIN metadata key, failing before
compilation. The corrected build succeeds; both logs remain. A watcher invocation
with a date-only lower bound was rejected before monitoring, then corrected to
the required timestamp syntax. Neither issue altered submitted generator bytes.

## Provenance, remaining work and regression status

Authoritative collection:
`benchmarks/runs/20261003-ramsey-recognized-certificate-v2/collection.json`.
It binds every native proof/input/source map, stage events and corrected controls.
The isolated immutable generator pin is
`build/pin-ramsey-recognized-dsr-8ff48dd1-01`; its build manifest binds the 821-file
candidate source snapshot, link inputs and exact compile/link commands. The
prototype includes the existing signed-cardinality implementation to reuse its
BDD engine in an isolated translation unit. Public migration must extract shared
facilities instead of retaining this research build arrangement.

The prototype records recovered edge meanings and K4 source occurrence IDs;
it does not yet provide full action-level, per-proof-event semantic provenance.
It also lacks whole-certificate mutation-free preflight, public typed selection,
proof-independent namespace reservation, after-action feedback/replay and an
accepted DefaultPolicy schedule. These are required before promotion. SR's
original-database boundary remains in force. All 44 target and N1–N4 obligations,
constructor replacement, broader SAT reasoning and learning preparation remain.

The current parity/graph candidate's B28+C5 `1747647` and Rank1–50 `1747662`
regressions remain active. WV100 `1747663`, WV2–89 `1747665` and full Debug
`1747703` are queued. The fresh parity fuzz owner reports submitted DefaultPolicy/random legs
`1747736`/`1747737`, 100,010 fresh cases each, verified snapshot/checker bindings,
and disjoint seed ranges. Both ordinary legs have X8 off; the targeted harness
is still being prepared, so these legs cannot establish effective X8 coverage.
There is no terminal owner disposition; no private campaign data was inspected. Old snapshots
cannot accept later integration. The 30-minute regression proof-checking cap and
`max(30%,20 seconds)` slowdown allowance are unchanged. No historical baseline or
full-comparison campaign was rerun, and no push was performed.

Keep the active campaign artifacts, original rejected controls, all three native
certificates and checker provenance. No evidence cleanup deletion is made here.
Andrew handoff: `report-to-andrew/20261003T222632Z-recognized-ramsey-certificates/`.
