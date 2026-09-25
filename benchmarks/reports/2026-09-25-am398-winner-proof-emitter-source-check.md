# AM398: bounded winner proof-emitter source check

Date: 2026-09-25. Current Tactix source: `bf61e138`. Read-only comparison of
the locally pinned SC24 Kissat, AE-Kissat-MAB, and Satsuma sources. This narrows
the source-inventory question; it is not an exhaustive survey of every winner
path, a new N1/N2 proof check, or evidence that N3/N4 are satisfied.

## Kissat fork

The complete `src/proof.c` files in `winners/kissat-sc2024` and
`winners/AE_kissat2025_MAB/AE_kissat2025_MAB` are byte-identical (SHA-256
`dd393d3767b53cb3c0fb26a6dba3e4ad4dd21460b8dba018f942e128003dedda`).
Their complete `src/definition.c` files are also byte-identical (SHA-256
`f8fa6b4fd4e6cc2bfd03213754d3ce49f43bd000d71db88824a1c7c8800ece76`).
That file's `kissat_find_definition` extracts an existing gate from the
literal's two watch environments via Kitten and can emit one-sided core RUP
lemmas/units; it is *not* an operation that introduces a generic fresh
definitional variable (`definition.c:91-220`, `gates.c:31-59`). A source name
alone would therefore be misleading evidence for the deferred generic
extension proposal in plan §9.1.

The differing `factor.c` portion changes tick accounting, completion return,
and retry watermark, while retaining Factor's proof mechanism. The reviewed
`congruence.c` diff changes a unit simplification case, proof-chain polarity,
and clause lookup condition; `transitive.c` changes the score source for
ordering. These are meaningful implementation/strategy differences, but the
comparison did not expose a new proof-emitter primitive unique to the MAB
fork. It does not prove that every changed source path is semantically
equivalent or correctly covered by Tactix.

## Satsuma proof surface

The SC26 Satsuma source under
`winners/satsuma-iter-kissat-sc2026/anders/satsuma-iter-kissat/src/solver/satsuma-dev`
has the same `proof.h` implementation as local Satsuma v1.4 except its version
comment (nine diff lines). Its public emission methods are `sbp`,
`blocked_clause`, `rup_clause`, `drat_clause`, `orbitopal_fixing`,
`binary_clause`, and deletion (`proof.h:1308-1438`). In SR mode, the first
three write ordinary clause additions; orbitopal fixing and binary cuts write
explicit finite substitution witnesses. In VeriPB mode, these paths use
`rup`, `red`, polynomial combinations, and dominance subproofs. Call sites in
`predicate.h:543-563`, `cnf.h:100,449`, and `satsuma.h:746,830` cover the
reviewed orbitopal, cut, blocked, and unit paths.

Tactix's clausal event vocabulary in `docs/calculus.md:33-43` includes RUP,
RAT, finite SR and deletion; X7 supplies the general finite SR action, while
the symmetry producer is a strategy, not a new rule guard. This is a
*source-level mapping*, not proof that all legal Satsuma-produced witnesses
are N1-reachable or that every emitted Tactix proof validates. The PB
dominance proof format is not a general live Tactix action; Tactix instead
uses native DSR for these clausal symmetry effects and reserves PBP for
specified terminal startup refutations. Format choice still belongs to N2.

## Open boundary

CaDiCaL's `Proof`/`Tracer` interfaces distinguish derived clauses (including
RAT), deletion, `weaken_minus` restoration metadata, and incremental
assumption/constraint events (`cadical/src/proof.hpp:48-108`,
`tracer.hpp:20-135`). `weaken_minus` records a clause that may need model
restoration, not a new clausal inference (`proof.cpp:396-419,589-598`).
Tactix's deferred incremental API contract is therefore a separate protocol
gap, not evidence by itself of a missing nonincremental proof rule. This is
interface classification, not a complete CaDiCaL producer audit.

The older AM368 report's X5 gap is no longer current: explicit variable-length
Condition is implemented and default-off, as `docs/implementation-notes.md`
§16-17 records. The potential generic fresh-variable extension in the frozen
Phase-4.20 survey is still a deferred Phase-4.5 design item, not established
here as a missing *used* winner action. LymphoSAT's many family-specific PBP
provers and remaining CaDiCaL/Kissat source paths still require systematic
source-to-action audit; an inventory-table pass alone does not prove coverage.
H2's canonical no-option DSR rejection and the separate post-finalization API
bug remain N2/N1 blockers. No Tier-D run was submitted.
