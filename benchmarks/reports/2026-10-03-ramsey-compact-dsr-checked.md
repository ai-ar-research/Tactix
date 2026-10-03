# Compact full Ramsey DSR certificate independently checked

2026-10-03. Status: full original-CNF certificate feasibility VERIFIED, including
every-lemma audit and negative controls. Public solver integration and cumulative
acceptance remain OPEN. No production source changed in this increment.

## Result

The standalone native prototype proves the exact saved rank-630 formula,
`ramsey_4_4_19.normalised.cnf.xz`, using witnessed symmetry, short RUP resolution
and Tactix's existing BDD counting engine. The compact 1,650,212-byte DSR proof
passes no-option dsr-trim/Trestle and the separate every-lemma witnessed audit.
The same route rejects a deliberately false refutation of a satisfiable unit CNF.

No PB commands, external solver stage or stored model/proof answer are used.
This establishes a complete DSR path for the original input; it does not claim
an integrated second target, N1–N4 acceptance or readiness for learning. Rank 991
remains the graph-enabled candidate's checked target. The startup Ramsey action
still needs implementation through the shared public execution interfaces.

## Measured evidence

All rows use cpu-q, one CPU and 32 GiB, without node constraints or throttles.
These are separate diagnostic allocations, not matched statistical repetitions.
Certificate sizes and node/addition counts are exact; timing differences remain
single observations and are not whole-solver or historical speedup claims.

| Certificate | BDD nodes | Additions counted by sink | Native DSR bytes | Derivation seconds | Canonical + every-lemma check seconds |
|---|---:|---:|---:|---:|---:|
| Original full R(4,4) prototype, job 1747619 | 39,062 | 216,891 | 4,016,637 | 0.070899 | 654.4371 |
| Compact full R(4,4), job 1747624 | 9,192 | 74,782 | 1,650,212 | 0.026883 | 82.7134 |

Additions counted by the sink exclude the explicit witnessed-symmetry prelude;
the compact proof additionally deletes 7,626 original clauses. Derivation timers
exclude input copying/parsing/recognition and checker execution. Job 1747624's
full driver wall time was 83.011 seconds; scheduler elapsed was 85 seconds.
Its reported batch MaxRSS is 139,952 KiB, a sampled job metric, not a guaranteed
per-stage maximum or future production footprint.

Compact full-input checker events retain exact commands, outputs and statuses:

| Stage | Seconds | Exit |
|---|---:|---:|
| Canonical no-option dsr-trim | 1.4657 | 0 |
| Canonical Trestle | 6.2602 | 0 |
| Every-lemma dsr-trim -f | 0.8978 | 0 |
| Every-lemma Trestle | 74.0312 | 0 |
| False-proof negative control, dsr-trim | 0.0060 | 1, rejected |

The canonical route totals approximately 7.73 stage seconds; the 82.71 figure
also includes the every-lemma audit and wrapper work. Neither checker timeout
nor failed certificate is relabeled as verified. Limits were 300 seconds for
generation, 1,800 seconds for each complete canonical/audit pipeline and a
40-minute allocation. These are feasibility runs, not the regression protocol.

The isolated compact R(3,4) gate, job 1747623, also passes canonical, every-lemma
and negative-control checks on its 36-variable/210-clause input. It uses 9,192
BDD nodes, 74,689 additions and 1,372,963 DSR bytes; derivation was 0.025047 seconds,
canonical checking 11.2994 seconds, total driver time 26.1565 seconds. The earlier
BDD-only subproof remains frozen in the [feasibility checkpoint](2026-10-03-ramsey-dsr-feasibility.md).

## Why the proof is smaller

The first full proof encoded every conditional R(3,3) exclusion through BDDs.
That is unnecessary for the five incident edges involved in each local argument.
After emitting the same twenty RUP context clauses, the compact generator recursively
splits those five edges. A branch stops once three selected edges have the same
color, contradicting an existing clause. Only the nineteen internal resolvents
are emitted. The last resolvent is the six-neighbor context exclusion.

This is a general finite derivation instantiated from recovered vertex/literal
identities, not a stored answer or rank-dependent rule. BDDs remain responsible
for the larger degree constraints and their integer counting contradiction.
The rule's eventual preflight must meter both parts and respect caller limits.

A second change removes unnecessary original clauses only after the complete
witnessed symmetry prefix and all induced negative triangles have been certified.
The proof retains the 126 positive four-clique clauses on the selected nine
vertices; together with 84 derived negative triangles they form R(3,4).
Deleting the other 7,626 original clauses weakens the proof database and introduces
no premise. The full original database was present for every symmetry step.
This is proof-database reduction in a terminal derivation, not a claim of safe
live symmetry after arbitrary inprocessing.

The full compact runner verifies that the entire post-deletion counting tail
matches the independently checked compact R(3,4) proof byte-for-byte after
renaming the selected original variables and fresh proof-only auxiliaries.
The temporary witnessed-unit auxiliary identity is explicitly reserved.
Independent full original-CNF checking remains mandatory despite this composition
check; it has passed.

## Control still in flight

Deletion-only job 1747622 retains the original BDD derivation. Its runner proves
that removing just the 7,626 newly added deletion lines reproduces the original
full proof byte-for-byte. Canonical dsr-trim and Trestle completed in 36.9227 and
43.3214 seconds; forward dsr-trim completed in 21.0390 seconds. The forward
Trestle audit remains live at this checkpoint. Its final independent disposition
and total cost must be collected; no final acceptance or timing conclusion is
claimed for this control. Keep its immutable pin and active artifacts intact.

## Provenance and integration decision

Original uncompressed input SHA-256:
`9c8ddd5ad86590ef636f46a61ecf4732abf42dadee70fd2be3c0783c08e7160f`.
Original full DSR SHA-256:
`6708c64aa8046c0cf8c503019d1bb233619c1321319a15f73a44ec803073e69c`.
Compact full DSR SHA-256:
`109277dd17dce86e90b659bc87784f8eab6aa08ab80fdef66cf2cebaaea200cd`.

Both original-CNF runs independently verify the complete 171-variable,
7,752-clause signed K4 encoding. Their copied input is byte-identical to the
saved compressed input after decompression. Build, source, binary, runner,
checker and original-input bindings were verified at collection. The retained
clean dsr-trim/compressor authority and Trestle source/patch/binary review apply.
No historical solver/comparison campaign was rerun.

Use the compact derivation for the proposed public startup-certificate path in
the prior mechanism review. Extract reusable BDD code rather than duplicating it;
keep original-input admission, format guards, full bounded preflight, fresh identity
management, atomic proof publication and raw execution feedback explicit. Both
DefaultPolicy and random selection need effective typed action coverage and replay.
Unsupported formats decline before mutation; no fallback or mixed PB stream is
introduced. General restart-time SR retains its separate live-database validation
and review requirement. Graph scheduling and all current-pin N1–N4, Debug, fuzz
and four-suite preservation obligations remain open. All 44 targets stay in scope.

## Artifacts and hygiene

- `benchmarks/runs/20261003-ramsey-r44-dsr-feasibility/`: original full checked proof.
- `benchmarks/runs/20261003-ramsey-r34-compact-dsr-feasibility/`: checked compact subproof.
- `benchmarks/runs/20261003-ramsey-r44-compact-dsr-feasibility/`: checked compact full proof,
  stage events/stdout/stderr, negative control, manifests and accounting.
- `benchmarks/runs/20261003-ramsey-r44-reduced-dsr-feasibility/`: active deletion-only control.
- `benchmarks/runs/20261003-ramsey-compact-dsr-checkpoint/`: all new driver/build sources,
  immutable-build manifests and the active control's submission record.
- Immutable `build/pin-ramsey-*` binaries and local checker copies are retained.
- After terminal verification, 23,836,225 bytes of regenerated intermediates were
  removed from the three completed runs. Exact names, hashes and reasons are in
  their cleanup manifests. Original CNFs, native proofs and unique evidence remain;
  parent proofs referenced by the active control were preserved. No active files
  or private fuzz artifacts were removed or inspected.
- Andrew handoff: `report-to-andrew/20261003T202500Z-ramsey-compact-dsr-checked/`.
