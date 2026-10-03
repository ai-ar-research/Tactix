# Winner integration: saved-evidence attribution and first mechanism review

## Outcome and scope

The fixed 36 and all 44 targets are intact. This audit reclassified their 220
saved arm records, verified all 229 inputs bound by the portfolio manifest,
and verified 10,071 campaign pin hashes across the three retained campaigns.
No solver or checker ran. Production solver source remains `375b621b`.
This is attribution evidence, not acceptance of an integration candidate.

The reproducible [CSV, summary and hash manifest](../runs/20261003-winner-integration-attribution/)
retain all original target columns and add historical handler logs, actual
certificate bytes/content routes, source bindings and explicitly labeled
investigation leads. Reproduce into a **new** directory with
`python3 -B tools/bench/winner_attribution.py --output benchmarks/runs/NEW-NAME`.
The tool refuses to overwrite an existing directory. It does not rerun the
original collector or alter frozen evidence.

The approved historical approximation remains no-MAB `d170b4a5`. The source
review is against current Tactix and LymphoSAT source files byte-identical to
those in the saved checked campaign's immutable pin. The manifest records
those individual source bindings; it is not a claim that source inspection
identifies every runtime branch.

| Checkpoint | SAT | UNSAT | Solver timeout | Checker timeout | Checker failure |
|---|---:|---:|---:|---:|---:|
| First 36 | 18 | 18 | 23 | 9 | 4 |
| Additional MAB-only eight | 6 | 2 | 7 | 1 | 0 |
| All 44 | 24 | 20 | 30 | 10 | 4 |

## What the LymphoSAT evidence actually establishes

Among the 20 currently checked LymphoSAT target rows, the **older attribution
run** logs 15 final fallback handlers, one each for heule-nol, sorting-networks,
ramsey-numbers and antibandwidth, and one unknown final handler. That older
campaign is `benchmarks/runs/20260819-tierd-lymphosat-modlog/`: its own manifest
explicitly says attribution-only, with no independent proof/model recheck.
Its result-to-manifest and archived-dataset bindings were verified here. The
manifest reports 127/127 nested binaries identical to the old frozen pin;
that historical report is not a new cross-campaign binary-equivalence proof.

The newer checked run logs matcher acceptance but not the final handler. Thus
these are strong mechanism leads, **not proof that the same handler finished
the later run**. In particular, rank 1161 has a checked newer SAT result, while
the older mechanical-master-key attempt returned unknown and fallback timed
out. Calling the newer result a mechanical-master-key success is unsupported.
Planning at rank 511 and core-based-generator at rank 744 also returned unknown
before the older fallback solved. Their accepted matchers do not establish
specialized UNSAT reasoning as the source of the win.

The fallback's pinned `search.c`/`proof.c` show ordinary Kissat-derived CDCL with
PB-format logging. A PB certificate format alone does not establish that the
solver used specialized counting or pseudo-Boolean search.

## Four concrete source findings

All source paths below are relative to
`winners/lymphosat-sc2026/green/lymphosat/src/solver/solvers/` and have explicit
checked-campaign source bindings in the evidence manifest.

| Target | Saved checked LymphoSAT solve | Source mechanism | Integration disposition |
|---|---:|---|---|
| 401, `noL-11-10` | 0.266 s | `heule-nol/solver/solver.cpp`: three-color hypergraph recognition, embedded 473-group witness, then noisy greedy recoloring | Recover a general hypergraph constructor; exclude the embedded witness and size-selected witness seeding. The saved logs do not distinguish immediate witness success from local search. |
| 497, `snw_16_9_Encpre` | 247.229 s | `sorting-networks/solver/solver.cpp`: small CDCL solver plus fixed `snw16_f9`/`snw16_public` phase lists, selected by encoding shape | General structure-derived candidate generation is admissible; importing the fixed phase lists is not. The fast result alone does not demonstrate a general sorting-network search advantage. |
| 630, `ramsey_4_4_19` | 0.567 s | `ramsey-numbers/solver/solver`: full clause-set recognition, vertex/color symmetry, conditional cutting-planes derivation | A genuine missing reasoning lead. Review native clausal/DSR feasibility with the existing certified BDD machinery; retain a justified PB route if efficient clausal certification is impractical. |
| 991, `abw-K-dwt__234.mtx-w50` | 28.852 s | `antibandwidth/solver/solver.cpp`: graph-labeling recovery, degree/BFS order seeds, bounded window DFS, pair-swap repair and annealing, Boolean completion | Strong first general SAT-constructor candidate. Adapt into shared continuing-state actions and retain unsuccessful assignments as selectable guidance. |

**Ramsey details.** Recognition verifies both signed six-literal clauses for
every four-vertex set, with a lexicographic edge-variable mapping. Sixteen
vertex-permutation reductions order a star, and color complement plus reversal
selects a nine-neighbor color class. Conditional R(3,3) exclusions and R(3,4)
degree bounds are summed and divided to obtain the parity contradiction.
The saved result records an 876,582-byte certificate and a successful full
checker pipeline. This is an algorithmic derivation, not a stored UNSAT answer.
Recognition for rank 147 remains unexplained by this audit: its LymphoSAT
matcher did not accept, so the rank/name resemblance does not justify assuming
that the same recognized representation is present.

The current `signed_cardinality_proof.cpp` already emits native RAT/RUP BDD
certificates, but its recognizer admits complete signed-ternary groups and its
existing elimination machinery is not a general implementation of the above
conditional sum/divide derivation. Reuse is a feasibility lead, not completed
support. Restart-time symmetry still needs the documented live proof-database
validation and Claude review; the terminal startup proof is no shortcut around
that obligation.

**Graph-labeling details.** The reference uses 24 initial ordering/labeling
combinations, then (for its selected graph sizes) bounded window DFS and repair.
Its recognizer assumes an indexed primary-variable rectangle; a native
implementation needs exact recovered mappings or a conservative structural
rejection, not rank/name dispatch. All Boolean completions must still pass
whole-live-formula/root validation, reconstruction and the independent original
CNF oracle. The bounded search should publish a useful candidate even when it
cannot finish SAT. Reference wall clocks and size thresholds need explicit,
source-backed typed effort profiles; they must not become hidden default-only
searches or an external portfolio stage.

## Certificate-side misses remain a distinct workstream

The first set has 13 certificate-side misses; the expanded set has 14. The
four saved failures are one dsr-trim segmentation fault (569) and three
allocation failures (627, 654, 1004). No failure is relabeled as an invalid proof.
The table below preserves the failed stage and bytes of the completed UNSAT
stream. A timeout row does not establish that its certificate is valid.

| Rank | Saved Tactix disposition | Failed/incomplete stage | Certificate bytes | Saved failure cause |
|---|---|---|---:|---|
| 82 | checker_timeout | drat-trim | 2,058,524,846 | deadline censor |
| 226 | checker_timeout | drat-trim | 20,324,467,728 | deadline censor |
| 291 | checker_timeout | srcheck | 424,530,578 | deadline censor |
| 489 | checker_timeout | drat-trim | 2,517,823,072 | deadline censor |
| 511 | checker_timeout | drat-trim | 5,654,167,762 | deadline censor |
| 541 | checker_timeout | drat-trim | 247,511,403 | deadline censor |
| 569 | proof_route_or_stage_failure | dsr-trim | 3,165,736,883 | checker_segmentation_fault |
| 627 | proof_route_or_stage_failure | dsr-trim | 5,567,655,309 | dsr_allocation_failure |
| 654 | proof_route_or_stage_failure | dsr-trim | 12,812,827,060 | dsr_allocation_failure |
| 744 | checker_timeout | drat-trim | 401,269,091 | deadline censor |
| 1004 | proof_route_or_stage_failure | dsr-trim | 4,750,744,327 | dsr_allocation_failure |
| 1075 | checker_timeout | dsr-trim | 1,088,746,007 | deadline censor |
| 1085 | checker_timeout | drat-trim | 2,689,394,113 | deadline censor |
| 1129 | checker_timeout | drat-trim | 230,735,672 | deadline censor |

Seven checked LymphoSAT UNSAT certificates are **larger** than the corresponding
completed Tactix streams: ranks 226, 291, 511, 541, 744, 1075 and 1129. For
example, rank 226 records 61,171,537,730 PB bytes versus 20,324,467,728 Tactix
bytes; rank 541 records 1,065,468,767 versus 247,511,403. Different proof formats,
checking algorithms, dependencies and asynchronous placement prevent causal
conclusions from these sizes or times. Investigate derivation structure,
checker memory, trimming and certified export alongside compactness. A global
format switch to PB is not supported by this observation.

The CSV distinguishes SAT model bytes, completed UNSAT proof bytes and partial
or unknown streams left by solver timeouts. A declared format is left blank
when absent in the original record; observed content routing is separate.

## Remaining search gaps and preservation evidence

The CSV labels the other rows as search/phase divergence, symmetry/search
interaction, or unresolved MAB-policy attribution. Those labels are hypotheses.
Satsuma's checked success does not prove that symmetry caused it without an
ablation. The eight MAB-only targets remain in scope; the scheduling ledger
records that enabling the owner's EVSIDS/CHB policy globally previously failed
all four regression conditions. Do not re-enable it merely because these eight
rows are missing. Investigate its useful mechanisms through explicit shared
choices while keeping reward/training deferred.

No new source has passed fuzz or any performance gate in this audit. The old
accepted preparation gates continue to describe only their old source pin.
All candidate acceptances still owe N1–N4, effective default/random coverage,
full Debug, four regression suites and independent target certificates under
the limits in the integration subplan. Routine 1,800-second checker censors
never close one of the 44 targets.

## First complete integration increment

Proceed with the **general graph-labeling constructor plus shared candidate
consumption**, motivated by rank 991. The reference has a reusable algorithm
without the embedded assignments found in the other two SAT specialists.
This prioritization does not assert that it will close the target until tested.
Keep the hypergraph constructor and Ramsey reasoning as the next concrete
mechanisms, while continuing the independent certificate-side investigation.

The source review exposes two prerequisites in today's guidance path:

1. `local_search_guidance_apply_kind` writes `saved` and then copies it to
   `target`. DefaultPolicy calls it before Walk. This contradicts the newly
   agreed private-Walk-seed and target-only consumer semantics.
2. `kernel_offer_local_search_hint` rejects a worse same-kind exact objective
   without formula/objective provenance. Such scores cannot be compared safely
   after the producer's objective changes with the formula.

Implement the consumers and the constructor as one learning-ready increment:
retain separately owned candidates with stable identities, generation,
provenance and effort; explicitly select either private Walk seeding or target
initialization; preserve saved/best at import and focused-mode behavior; let
normal target capture supersede advice. Serialize choices and candidate
references for public/default/random execution and replay. Requests naming a
missing/stale generation need an explicit disposition, not silent replacement.
Common current-CNF evaluation must be charged before comparing producer hints
or exporting a seeded Walk result; zero-work and interrupted requests need
specified, tested effects. The existing unseeded Walk/cadence/effort reference
provides the preservation control.

Only then add the native graph recognizer, bounded constructor/repair,
independently checked SAT completion and useful partial-candidate publication.
Expose the legal action, parameters and raw feedback to both policies. Do not
accept an API-only or terminal-SAT-only subset as this complete increment.
Use generated permuted/signed small encodings, fixed/retired/new variables,
wrong recognitions, stale candidates, interruption and exact replay to establish
semantics before candidate-only Tier-D probes and the cumulative gates.

This report closes the initial saved-evidence attribution task. It closes
**zero** portfolio targets and does not declare the integration goal complete.

## Audit validation and handoff

A second extraction into a separate scratch directory reproduced the CSV,
summary and manifest byte-for-byte. The overwrite negative control returned
an error before mutation, and all authoritative output hashes remained equal.
Documentation size, citations, section references, catalog inventory,
proof-rule synchronization and progress checks passed. Size checks retain
seven existing low-reserve warnings; no unrelated compaction was performed.
Andrew's three-page TeX/PDF was compiled twice and its complete extracted text
reviewed; the final compile has no overfull boxes. No solver source changed,
so no new solver correctness/performance acceptance is claimed or needed for
this evidence-only commit. The queue contains only the unrelated interactive
allocation; no baseline, certificate or earlier experiment was removed.
The three byte-identical replay copies (110,316 bytes) were removed after
comparison; the authoritative extraction and its manifest remain intact.
