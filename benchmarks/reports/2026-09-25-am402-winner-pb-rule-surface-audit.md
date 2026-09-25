# AM402: winner pseudo-Boolean proof-rule surface audit

Date: 2026-09-25. Read-only source comparison on current Tactix `092deac4`
and the locally pinned SC26 winner source. This is a proof-vocabulary audit,
not an N1/N2 acceptance run or a claim that a winner technique has been
ported. AM398 covers the separate Kissat-fork and Satsuma clausal comparison.

## Finding

There is a concrete **proof-calculus gap**, but not an established missing
*clausal* inference primitive. Tactix's ordinary action proof vocabulary is
RUP, RAT, finite substitution redundancy (SR), and deletion
(`docs/calculus.md:33-43`; `src/proof/proof.hpp:72-106`). Its PBP output is
confined to complete original-input Lucky 19/20 startup refutations before any
ordinary proof event (`DEVELOPMENT_PLAN.md:162-185`). Those two producers do
emit PB `rup`, `red`, `pol`, and local subproofs
(`src/rules/lucky/parity_refutation.cpp:643-728`,
`src/rules/lucky/count_preservation.cpp:317-441`), so saying that Tactix has
*no* PB arithmetic would be false. There is no registered, general live PB
arithmetic/redundance action or proof-event interface that can accept an
arbitrary `pol`/`red`/subproof derivation after the startup boundary.

The pinned verified LymphoSAT bundle selects specialized solvers through
`src/solver/order.json` and a generated matcher registry
(`src/solver/CMakeLists.txt:15-63`). On UNSAT, its composite verifies the
selected solver's PBP with VeriPB before publishing the proof
(`src/solver/core/main.cpp:496-557,678-710`). The following are direct
examples of **active listed solver paths**, not merely a filename inventory:

| LymphoSAT producer | Witnessed proof operations | Source |
|---|---|---|
| `equivalence-chain-principle` | PB sums, `pbc : subproof`, clausal `rup`, `qed pbc`, deletion | `solvers/equivalence-chain-principle/solver/solver.cpp:382-483` |
| `xor-chain` | PB addition, scaling, integer division, literal arithmetic | `solvers/xor-chain/solver/solver.cpp:345-433,620-755` |
| `tseitin-formulas` | PB `red` and `pol` throughout a family-specific refutation | `solvers/tseitin-formulas/solver/solver.cpp:479-524,976-1120,1909-1922` |
| `or_randxor` | fresh PB redundancy constraints and arithmetic/rounding | `solvers/or_randxor/solver/solver.cpp:230-307` |

A bounded literal-source inventory over the verified bundle's `solver.cpp`
files finds `pol` in at least 15 family producers, `red` in at least seven,
and `pbc` in at least three. These are lower bounds because output is sometimes
assembled indirectly. They do not establish that every producer is selected
for a competition instance or that Tactix lacks an alternative clausal proof.

Satsuma's pinned SC26 `proof.h:200-312,460-547,1165-1201` also emits PB
dominance/subproof and arithmetic in its VeriPB mode. Its SR-mode clause
effects map to the existing Tactix RUP/RAT/SR/deletion vocabulary, as AM398
records. A PB dominance *proof command* and a semantic symmetry action should
not be conflated: X7 can express finite clausal substitution witnesses, but
that does not supply a general PB arithmetic backend or prove equal efficiency.

## Contract implication and disposition

This is a **qualified yes** to the user's missing-proof-rules question:
competition winners use general PB arithmetic and PB redundancy/dominance
derivations that Tactix does not expose as general actions. It is **not** yet
evidence of an uncovered clausal RUP/RAT/SR primitive, nor an automatic N1
violation of the *currently declared* action catalog. The distinction matters
for N2 coverage of new specialized refutations and for proof-size/runtime
comparisons. Avoid a family-specific emitter: any implementation should use a
typed, generally applicable PB derivation interface, explicit legality and
resource bounds, original-CNF verification, and an intentional proof-format
boundary. The current one-way startup-PBP/ordinary-DSR choice makes a general
live PB path a substantial architecture decision; it cannot safely be spliced
into the DSR stream ad hoc. Review that design against the canonical DSR
contract and N1 before accepting new PB-powered rules. Continue the remaining
CaDiCaL/Kissat producer and N1/N2 reachability audits separately.

AM400/AM401 full Debug jobs `1720786`/`1720898` were running at this audit;
their completion, both affected Opus fuzz owners, H2's canonical no-option
checker issue, and post-finalization API model behavior remain unresolved.
No Tier-D run was submitted.
