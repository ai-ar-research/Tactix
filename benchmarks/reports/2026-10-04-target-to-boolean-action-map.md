# Saved-evidence map from recovery targets to Boolean theory actions

## Scope and claim boundary

This is a read-only planning audit. No solver, checker, benchmark, or detector
was run. The report maps the fixed 44 portfolio misses and the LymphoSAT-owned
WV100/WV2--89 memberships to the Boolean theory actions and internal
recognizers present in the current worktree. A mapping means that saved evidence
identifies a plausible premise family; it does **not** establish that the current
action admits the instance, produces a useful consequence, or solves it.

The fixed set comes from
[`portfolio-misses.csv`](../../report-to-andrew/20261003T164617Z-tierd-portfolio-misses/portfolio-misses.csv).
Owner attribution and the old final-handler records come from the frozen
[`attribution.csv`](../runs/20261003-winner-integration-attribution/attribution.csv)
and `20260819-tierd-lymphosat-modlog` results. The WV sets come from the frozen
[`WV100.tsv`](../tiers/WV100.tsv) and [`WV2-89.tsv`](../tiers/WV2-89.tsv).
The old handler run is attribution-only and was not independently checked; the
separate WV baseline establishes ownership, not that the logged handler caused
the checked solve. The fixed campaign's saved Tactix `solver_statistics` are
used only as target-bound recognition signals. Current semantics are taken from
[`semantic-extension-foundations.md`](../../docs/semantic-extension-foundations.md),
[`calculus.md`](../../docs/calculus.md), and the
[`v5 checkpoint`](2026-10-04-certified-theory-actions-v5.md).

There are 79 LymphoSAT memberships: 20 LymphoSAT-checked rows among the fixed
44, 34 sole-owner WV100 rows, and 25 fastest-owner WV2 rows. Fixed rank 630 is
also WV2 rank 155, and fixed rank 744 is also WV100 rank 90, leaving 77 distinct
LymphoSAT-associated instances. Together with the other fixed misses, this
report covers 101 distinct formulas. It makes zero target-recovery claims.

## Action key and evidence scale

| Key | Current capability | Important boundary |
|---|---|---|
| `A2` | `AffineConsequence` with `CertifiedBasisV2`, using the internal affine-basis transaction | Complete clause-backed GF(2) equations; adds small rows or exact XOR-tree definitions. `AffineBasis` is not a second registered rule. |
| `A1` | `AffineConsequence` selected-small-consequence mode | One selected consequence of width at most four. |
| `P` | `ParityConsequence` | Complete GF(2) encodings; selected bounded consequence; public typed action. |
| `E` | `ExactCoverExtension` | Only a complete signed SGEN image; defines block-occupancy variables. It does not prove exact-cover infeasibility or choose a cover. |
| `I` | `IndependentSetCover` | Only the strict layered exact-count image; caller supplies a sorted `N-K+1` vertex cover payload. |
| `G3` | `Gf3AffineConsequence` | Caller supplies a canonical pair of recognized GF(3) equation ordinals; publishes their elimination resultant. |
| `D` | `DirectCspSupport` | Caller supplies two complete ordered signed one-hot domains and one left row; only binary extensional relations are supported. |
| `R` | typed startup Ramsey action | Exact complete signed two-color K4 encoding on 18 or 19 vertices at the original-input boundary. |
| `SC` | internal/public Lucky signed-cardinality proof surface | Complete signed-ternary cardinality groups; terminal proof, not a continuing consequence action. |
| `ST` | internal/public Lucky st-connectivity proof surface | Exact bounded-width st-connectivity image; terminal proof. |
| `CP` | internal startup count/parity or balanced row-cover engines | Useful proof machinery exists, but a generally applicable continuing typed action is absent. |
| `GR` | internal graph interval, row-cover, disjunction, or gate recognizers | Proof-backed internal building blocks; public source-bound producer/action remains incomplete. |
| `TTF` | internal truth-table frontier recognizer | Public action was removed because its retained snapshot could not be authenticated against the checker population. |
| `CG` | ordinary Congruence/gate recognition | Implemented Boolean reasoning, but no family-specific theory payload or complete structural interpretation. |

Evidence is graded independently of likely usefulness:

- **H:** formula-bound saved structural evidence or an exact retained
  recognition/proof audit exists.
- **M:** an attribution-only final handler or checked matcher plus source
  semantics gives a direct family lead, without a current action admission.
- **L:** filename, broad gate counters, or an incomplete matcher suggests a
  family; premises are unverified.
- **N:** saved evidence supplies no meaningful bridge to an implemented theory
  action.

## Fixed 44 portfolio misses

The “saved signal” column is evidence, not current-v5 execution. Certificate-side
rows remain a separate problem: a new theory action is useful there only if it
produces a materially more checkable derivation; it cannot retroactively validate
the saved proof stream.

| Tier-D rank | Verdict | Evidence | Saved signal | Likely current payload family | Missing premise, detection, or action gap |
|---:|:---:|:---:|---|---|---|
| 82 | UNSAT | L | 192 AND gates; only Kissat checked | `CG` | CFI/symmetry interpretation is absent; no typed structural payload. This is also a checker-timeout row. |
| 97 | SAT | L | one affine equation and substantial SR; MAB-only checked | `A1/A2?` | One equation does not establish a useful affine image; identify complete sources and a consequence before offering a payload. |
| 147 | UNSAT | M | Ramsey name, 66 SR applications; Satsuma checked; LymphoSAT matcher did not accept | `R?` | Run-free evidence does not establish exact K4 admission for this spelling. Bind the saved CNF to the current recognizer before treating `R` as eligible. |
| 190 | SAT | L | no specialist or structural counter; ordinary Kissat/LymphoSAT/MAB checked | `D?` | Timetabling domains and supports are plausible, but no exact one-hot/domain-pair census is saved. |
| 226 | UNSAT | N | LymphoSAT historical final handler was fallback | none | No missing LymphoSAT theory mechanism is established. Primary saved gap is proof/checker cost. |
| 263 | UNSAT | H | 1,899 parity equations, 2,475 affine equations, 43,382 XOR gates | `A2`, then `P/A1` | Need current-source owner capture, productive basis/consequence selection, and compact proof-cost accounting on the totalizer residual. |
| 291 | UNSAT | N | fallback; 43 AND gates and 20 SR applications | none | No theory family is established; saved miss is terminal proof checking. |
| 323 | UNSAT | L | bit-vector subtraction name; no saved specialist recognition | `CG` | Recover exact arithmetic/gate semantics and useful certified consequences; gate count alone is insufficient. |
| 343 | SAT | N | no structural signal; generic Kissat and LymphoSAT checked | none | Attribute search/phase behavior before proposing a theory payload. |
| 367 | SAT | N | Satsuma/MAB checked; no structural signal | none | Symmetry/search attribution is missing; no current Boolean-theory action is justified. |
| 401 | SAT | H | heule-nol matcher/final handler; 347 one-hot domains, 1,047 AMO-derived SR binaries | `D` only as a binary-domain substrate | The essential constraints are ternary no-monochromatic hyperedges. Add an exact n-ary extensional-CSP/hypergraph consequence producer; binary `D` cannot express the full relation. |
| 410 | SAT | L | 74 affine equations and 1,949 AND gates; MAB-only checked | `A2?`, `CG` | No useful affine coordinates were recovered; scheduling semantics and exact source-backed consequences remain unknown. |
| 453 | SAT | L | mechanical-master-key matcher accepted but old final handler is unknown; very large SR/AMO activity | `D/I?` | Matcher acceptance is not handler attribution. Recover exact domains/constraints and reject any benchmark-layout shortcut. |
| 479 | SAT | L | parity-looking name, but no saved parity/affine recognition | `A2/P?` | Determine whether simplification destroyed complete XOR groups or whether the name is misleading. |
| 489 | UNSAT | L | millions of AND/XOR/ITE gates; no specialist handler | `CG` | A circuit invariant or compact proof action is missing; this is primarily a checker-timeout row. |
| 497 | SAT | M | sorting-networks matcher and final handler | `CG`, `GR` | Recover comparator semantics from clauses and publish reusable order/cardinality consequences. Fixed phase lists are excluded. |
| 499 | SAT | H | station-repacking matcher; old Tactix recognizer found a 1,393-vertex target-22 independent-set image | `I` | Revalidate with the strict current image, construct legal caller-selected covers, and expose enough consequences/advice for completion. The checked winner was MAB, not LymphoSAT. |
| 511 | UNSAT | M | planning matcher accepted but returned unknown; fallback solved in the old run | none | Do not map planning acceptance to a LymphoSAT win. Saved gap is proof/checker cost; useful planning invariants remain unidentified. |
| 540 | SAT | H | 74 affine equations, two coordinates and two candidates | `A2/A1` | Determine why candidates did not complete and whether certified basis variables plus separate advice expose useful search choices. |
| 541 | UNSAT | N | historical fallback; almost no structural signal | none | No theory bridge is established; checker cost dominates saved evidence. |
| 557 | SAT | H | 1,137 affine equations, two coordinates and two candidates | `A2/A1` | Same as rank 540, at larger scale; the action needs productive consequence selection rather than constructor restoration. |
| 569 | UNSAT | H | 24 parity/affine equations, 7,427 XOR gates; saved Tactix emitted UNSAT | `A2/P` | Investigate compact certified consequences and proof routing; the portfolio miss is a saved dsr-trim failure, not an unproved search gap. |
| 627 | UNSAT | M | planning matcher; 29,505 affine equations but the affine constructor exhausted before coordinates | `A2?` | Establish whether equations are complete/useful under current bounds. The saved miss is a proof-route/allocation failure. |
| 630 | UNSAT | H | Ramsey matcher/final handler; exact 19-vertex retained derivation | `R` | Current action is terminal startup-only. Production scheduling/limits and current-source target validation remain unaccepted. Also WV2 rank 155. |
| 654 | UNSAT | H | old independent-set recognizer found a 535-vertex target-11 image; extensive unique-AMO SR | `I`, possibly `CP` | Strict-image revalidation and selected-cover payloads are needed; graph-coloring contradiction/symmetry still needs a global certified argument. Saved proof route failed allocation. |
| 744 | UNSAT | M | core-based-generator matcher returned unknown; fallback solved | none | No specialized owner mechanism is established. This is a checker-timeout row and also WV100 rank 90. |
| 754 | SAT | L | timetable; Satsuma/MAB checked | `D?` | Recover exact one-hot domains and binary/n-ary scheduling relations; no saved detector evidence. |
| 762 | SAT | L | XOR-shift name; LymphoSAT historical final handler was fallback and no parity equations were logged | `A2/P?` | Need clause-backed XOR admission; family name is not a premise. |
| 781 | SAT | N | Satsuma-only checked; no structural signal | none | Symmetry/search mechanism attribution is still missing. |
| 887 | SAT | L | timetable; Satsuma/MAB checked | `D?` | Same domain/relation recovery gap as ranks 190 and 754. |
| 896 | UNSAT | H | 90 affine equations, two coordinates and two candidates; Satsuma/MAB checked | `A2/A1` | Determine whether affine consequences compose with a scheduling/counting contradiction; no such action composition is currently defined. |
| 899 | SAT | N | LymphoSAT checked through fallback | none | No specialist theory mechanism is supported by saved evidence. |
| 941 | SAT | L | 49,608 AND and 96 ITE gates; historical fallback | `CG` | Recover a source-bound state-machine/circuit interpretation and useful implications. |
| 991 | SAT | H | antibandwidth matcher/final handler plus exact retained interval/row recovery on this CNF | `GR` leading to exact-OR definitions, row cover, `D`, and semantic advice | The graph recognizer, occupancy definitions, and row-cover proof are not one public repeated action; graph constraints beyond permutation also need compact certified export. |
| 1004 | UNSAT | H | 24 parity/affine equations, 7,434 XOR gates; saved Tactix emitted UNSAT | `A2/P` | Same compact-proof/routing issue as rank 569; saved failure was dsr-trim allocation. |
| 1009 | UNSAT | N | MAB-only checked; no structural signal | none | Attribute MAB/search behavior before introducing theory machinery. |
| 1040 | SAT | M | station-repacking matcher; 34,153 AND gates and large SR activity | `I/D?` | No saved strict independent-set/domain image. Recover exact high-level constraints before constructing a payload. |
| 1075 | UNSAT | L | old loose independent-set recognizer reported a huge target-2 image amid a large circuit; saved Tactix emitted UNSAT | `CG`; `I?` only after strict revalidation | Treat the old count-based recognition as suspect until every clause is classified. Saved miss is checker timeout. |
| 1085 | UNSAT | L | very large AND/XOR/ITE population; no specialist handler | `CG` | Missing compact circuit invariant; saved proof timed out in checking. |
| 1118 | SAT | N | MAB-only checked; no structural signal | none | Search-policy attribution is required first. |
| 1129 | UNSAT | N | LymphoSAT historical fallback; no structural signal | none | No theory family is established; checker cost is the saved gap. |
| 1144 | SAT | L | timetable plus SR activity; Kissat/Satsuma/MAB checked | `D?` | Recover and certify domains/supports; symmetry may be the stronger lead. |
| 1161 | SAT | M | mechanical-master-key matcher accepted, but the old final handler is unknown | `D/I?` | The checked newer LymphoSAT solve cannot be attributed to the matcher. Exact domain/constraint recovery is missing. |
| 1163 | SAT | N | generic Kissat/MAB checked; no structural signal | none | Native search/phase attribution precedes any theory action. |

## WV100 sole-LymphoSAT memberships

These rows have separately checked sole ownership. The handler column is from
the attribution-only older run, so even a direct semantic match remains a
mechanism lead. Repeated rows in the same handler family are listed separately
to preserve the fixed denominator.

| WV rank (Tier-D rank) | Verdict | Evidence | Historical final handler | Likely action | Missing gap |
|---|:---:|:---:|---|---|---|
| 1 (224) `sgen1-unsat-121-100` | UNSAT | H | `sgen` | `SC` | Exact signed-ternary counting is supported by the terminal signed-cardinality surface. `E` is inapplicable to this pure-ternary proof family; a reusable nonterminal count consequence remains absent. |
| 7 (122) `two-trees-511v` | SAT | H | `binary-tree-parity` | `A2`, then `P/A1` | Complete and shortened XOR encodings need current owner capture; basis variables/consequences must replace assignment construction. |
| 9 (187) `16` | SAT | H | `gf3-sat` | `G3` | Repeated equation-pair elimination, action selection, and semantic-variable advice must expose enough of the solution space; one resultant is unlikely to suffice. |
| 10 (239) `2` | SAT | H | `gf3-sat` | `G3` | Same GF(3) closure/scheduling gap. |
| 11 (249) `6` | SAT | H | `gf3-sat` | `G3` | Same GF(3) closure/scheduling gap. |
| 12 (382) `4` | SAT | H | `gf3-sat` | `G3` | Same GF(3) closure/scheduling gap. |
| 13 (395) `14` | SAT | H | `gf3-sat` | `G3` | Same GF(3) closure/scheduling gap. |
| 15 (555) `10` | SAT | H | `gf3-sat` | `G3` | Same GF(3) closure/scheduling gap. |
| 16 (595) `two-trees-1023v` | SAT | H | `binary-tree-parity` | `A2`, then `P/A1` | Same parity-basis gap as WV7, at larger width/free dimension. |
| 18 (680) `13` | SAT | H | `gf3-sat` | `G3` | Same GF(3) closure/scheduling gap. |
| 19 (820) `12` | SAT | H | `gf3-sat` | `G3` | Same GF(3) closure/scheduling gap. |
| 22 (1164) `8` | SAT | H | `gf3-sat` | `G3` | Same GF(3) closure/scheduling gap. |
| 23 (812) `1` | SAT | H | `gf3-sat` | `G3` | Same GF(3) closure/scheduling gap. |
| 25 (231) `mchess_17` | UNSAT | M | `mutilated-chessboard` | `CP/GR` candidate | Owner proves a Hall/cardinality contradiction over cell covers and AMOs. No current public action recognizes that bipartite incidence and emits the global count proof. |
| 26 (348) `mchess_20` | UNSAT | M | `mutilated-chessboard` | `CP/GR` candidate | Same Hall/cardinality action gap. |
| 27 (349) `fermat-931960058139995587` | SAT | M | `fermat` | `CG`; no complete theory action | Existing reference recognition recovers arithmetic circuits, but factors are choices. Add certified arithmetic identities/definitions that make Boolean branching effective. |
| 29 (669) `15` | SAT | H | `gf3-sat` | `G3` | Same GF(3) closure/scheduling gap. |
| 33 (1047) `9` | SAT | H | `gf3-sat` | `G3` | Same GF(3) closure/scheduling gap. |
| 40 (851) `crn_40_1521_s` | SAT | M | `automata-synchronization` | `CG`, possibly `D` for certified local transitions | State-image variables are sets rather than one-hot values, so `D` is not directly admissible. A bounded transition/reachability producer is missing. |
| 51 (1007) `mdp-32-11-sat` | SAT | H | `minimum-disagreement-parity` | `A2/P` | Affine sources are established, but the decisive low-weight/cardinality syndrome constraint is not translated into continuing Boolean constraints. |
| 63 (965) `mdp-32-14-sat` | SAT | H | `minimum-disagreement-parity` | `A2/P` | Same affine-plus-cardinality gap. |
| 64 (782) `mdp-36-10-sat` | SAT | H | `minimum-disagreement-parity` | `A2/P` | Same affine-plus-cardinality gap. |
| 65 (813) `mdp-36-14-sat` | SAT | H | `minimum-disagreement-parity` | `A2/P` | Same affine-plus-cardinality gap. |
| 68 (415) `hwmcc17miters-xits-iso-6s163` | UNSAT | N | fallback | `CG/A2/P?` | No specialized owner mechanism was logged. Inspect complete gate/XOR premises before assigning a theory family. |
| 73 (1139) `circuit_32in64out...6in6out...` | SAT | M | `random-circuits` | `CG/GR` | Exact truth-table gate recovery exists only in retained constructor code. A source-bound gate-table consequence/definition action is missing. |
| 74 (641) `hwmcc17miters-xits-iso-6s299b685` | UNSAT | N | fallback | `CG/A2/P?` | Same unattributed hardware-proof gap as WV68. |
| 80 (720) `tseitin_grid_n250_m250` | UNSAT | H | `tseitin-formulas` | `P/A2`; internal startup parity proof | Complete parity-sum contradiction is known. Continuing consequences and compact checker-friendly proof composition remain the useful integration targets. |
| 83 (327) `tseitin_d3_n100000` | UNSAT | H | `tseitin-formulas` | `P/A2`; internal startup parity proof | Same parity-sum/proof-cost gap. |
| 84 (853) `StConn_7_128` | UNSAT | H | `st-connectivity-principle` | `ST` | Exact terminal recognizer/proof exists; reusable reachability lemmas and a continuing public action do not. |
| 85 (972) `16_2` | SAT | M | `cryptography-simon` | `CG`, `A2/P` for XOR subrelations | Round AND/XOR/key relations need certified circuit-level consequences; exhaustive key choice is not an inference. |
| 86 (1113) `tseitin_grid_n400_m400` | UNSAT | H | `tseitin-formulas` | `P/A2`; internal startup parity proof | Same parity-sum/proof-cost gap. |
| 90 (744) `ncc_none_21015_5_3_3_0_0_11` | UNSAT | M | fallback after `core-based-generator` returned unknown | none | Do not attribute the checked win to the matcher. This is also fixed rank 744 and primarily a saved proof-checking miss. |
| 93 (792) `rbsat-v945c61409g3` | SAT | M | `rbsat` | `D` | The owner semantics exactly match signed one-hot domains plus forbidden pairs. Missing: safe domain discovery/offering, repeated row scheduling, and sufficient global propagation/advice. |
| 98 (507) `rbsat-v1150c84314gyes1` | SAT | M | `rbsat` | `D` | Same DirectCSP action-generation and scheduling gap. |

## WV2--89 fastest-LymphoSAT memberships

Fastest ownership is weaker than sole ownership, but every row below is a fixed
technique-transfer target under the plan.

| WV2 rank (Tier-D rank) | Verdict | Evidence | Historical final handler | Likely action | Missing gap |
|---|:---:|:---:|---|---|---|
| 132 (189) `tseitin_d3_n158` | UNSAT | H | `tseitin-formulas` | `P/A2`, startup parity | Compact complete parity refutation/consequence scheduling. |
| 133 (332) `tseitin_n188_d3` | UNSAT | H | `tseitin-formulas` | `P/A2`, startup parity | Same. |
| 138 (208) `cliquecoloring_n32_k5_c4` | UNSAT | M | `coloring-clique` | `D` plus `CP/GR` | Binary domain supports alone do not prove the clique/color pigeonhole contradiction; exact global counting/symmetry action is missing. |
| 139 (271) `tseitingrid6x200_shuffled` | UNSAT | H | `tseitin-formulas` | `P/A2`, startup parity | Same parity gap, with shuffled admission. |
| 141 (419) `tseitin_d3_n174` | UNSAT | H | `tseitin-formulas` | `P/A2`, startup parity | Same. |
| 143 (926) `tseitin_d3_n162` | UNSAT | H | `tseitin-formulas` | `P/A2`, startup parity | Same. |
| 145 (1140) `bphp_p23_h22` | UNSAT | M | `binary-pigeon-hole` | `CP/GR`; `TTF?` only after source authentication | Decode binary truth tables and derive a compact pigeonhole/counting proof. Current public actions do not own this full bridge. |
| 146 (281) `tseitin_grid_n11_m20` | UNSAT | H | `tseitin-formulas` | `P/A2`, startup parity | Same parity gap. |
| 147 (328) `sgen1-sat-180-100` | SAT | H | `sgen` | `E` | Strict SGEN image is the exact current family. Definitions alone do not choose an exact cover; repeated semantic advice/consequences are still needed. |
| 148 (639) `clqcl_40_6_5` | UNSAT | M | `clique-coloring` | `D` plus `CP/GR` | Recover clique slots/color domains and publish a global pigeonhole/counting contradiction. |
| 149 (691) `sted5_0x1e3-20` | SAT | M | `stedman-triples` | `D/GR?` | Transition-table recovery is source-supported, but templates/cycles are choices. Add exact transition supports and reusable reachability/cycle constraints. |
| 151 (1038) `fermat-834855329100173267` | SAT | M | `fermat` | `CG`; no complete theory action | Same arithmetic-definition gap as WV100 rank 27. |
| 153 (175) `clqcl_50_6_5` | UNSAT | M | `clique-coloring` | `D` plus `CP/GR` | Same clique/color counting gap. |
| 155 (630) `ramsey_4_4_19` | UNSAT | H | `ramsey-numbers` | `R` | Exact current startup family; production scheduling/target validation remain. Also fixed rank 630. |
| 158 (27) `clqcl_100_6_5` | UNSAT | M | `clique-coloring` | `D` plus `CP/GR` | Same clique/color counting gap at larger size. |
| 159 (953) `1-ET-512-K-96` | SAT | H | `independent-set` | `I` | Strict current image matches. Generate legal selected covers repeatedly and use advice without restoring greedy/swap construction. |
| 160 (181) `xor_op_n40_d3` | UNSAT | M | `ordering-principle-xor` | `A2/P`, then missing ordering action | XOR-rail definitions can expose high-level literals; structural vertex-elimination consequences are not implemented as a shared action. |
| 161 (485) `FmlaEquivChain_4_8_8` | UNSAT | H | `equivalence-chain-principle` | internal startup `CP`, `CG` | Count preservation is the decisive invariant. A compact, general source-bound continuing action is absent; exact generator-order recognition is unacceptable. |
| 184 (52) `noL-11-0` | SAT | M | `heule-nol` | `D` substrate; missing n-ary CSP action | Three-color one-hot domains are usable, but each no-monochromatic hyperedge is ternary. |
| 187 (678) `mdp-32-10-sat` | SAT | H | `minimum-disagreement-parity` | `A2/P` | Missing low-weight/cardinality syndrome translation. |
| 188 (799) `mdp-32-16-sat` | SAT | H | `minimum-disagreement-parity` | `A2/P` | Same. |
| 221 (484) `noL-11-4` | SAT | M | `heule-nol` | `D` substrate; missing n-ary CSP action | Same hypergraph consequence gap. |
| 222 (363) `noL-11-8` | SAT | M | `heule-nol` | `D` substrate; missing n-ary CSP action | Same. |
| 223 (872) `noL-11-16` | SAT | M | `heule-nol` | `D` substrate; missing n-ary CSP action | Same. |
| 224 (406) `puzzle32_sat` | SAT | M | `sliding-puzzle` | `CG/GR`, local `D` only where exact domains exist | Need source-bound transition/state definitions and useful reachability constraints. IDA* action sequences are choices and must not return as a dedicated solver. |

## Coverage observations

The current action vocabulary aligns well with four sizeable cohorts, but only
at the premise-family level:

1. **GF(3):** twelve WV100 rows share the exact `gf3-sat` owner family. `G3`
   has the right proof boundary and payload identity. The major gap is action
   generation: enumerate eligible pairs, apply repeated elimination without
   duplicates, and expose useful variables/consequences to CDCL.
2. **GF(2):** parity/affine evidence covers both two-tree rows, four MDP rows,
   at least nine Tseitin/parity rows across WV/WV2, and strong fixed-target
   signals on lightsout and both multiplier rows. `A2` is the wide-basis action;
   `P/A1` export small consequences. MDP additionally needs a compact Boolean
   representation of its weight bound, while Tseitin/ordering instances need
   terminal or structural proof composition.
3. **Finite domains:** `D` is an exact match for RB-style binary CSP and a
   useful substrate for coloring, timetabling, noL, and transition systems.
   The latter families require n-ary constraints, global counts, reachability,
   or symmetry; treating all of them as binary `D` instances would lose the
   decisive theory information.
4. **Exact cover/independent set:** `E` and `I` have exact retained source
   families for `sgen1-sat-180-100` and `1-ET-512-K-96`. Saved fixed-target
   recognitions at ranks 499 and 654 are promising but require the stricter
   current classifier before payload construction. `E` definitions and one `I`
   cover are foundations, not complete replacements by themselves.

Several owner families still have no adequate shared Boolean action: Hall
counting for mutilated chessboard, clique/color and binary pigeonhole global
counting, n-ary hypergraph CSP, comparator-network semantics, arithmetic
circuits, automata/transition reachability, and equivalence-chain count
preservation. The internal counting, row-cover, gate, disjunction, and TTF work
reduces proof-engineering risk but does not supply public authenticated actions.

## Prioritized next recovery matrix

| Priority | Cohort and fixed members | Existing foundation | Next complete engineering increment | Evidence needed before any recovery claim |
|---:|---|---|---|---|
| 1 | 12 WV100 `gf3-sat` rows | public `G3` plus separate semantic advice | Add mutation-free enumeration of eligible retained equation pairs, deterministic repeated closure, relevant-change/duplicate handling, and selectable consequence/advice order. Keep exact pair payloads in replay. | Per-row current recognizer admission, useful-result counts, proof/model oracle, constructor-disabled result, fuzz and regression gates. |
| 2 | GF(2) affine/parity: WV two-trees, MDP, Tseitin; WV2 Tseitin/xor-op/equiv-chain; fixed 263/569/762/1004 | `A2`, `A1`, `P`, internal startup parity/count | Build a shared closure schedule: basis first, small consequences second, then family-independent certified count/weight interfaces. Separate MDP weight constraints from plain affine reasoning. | Exact source census per row, proof additions/checking cost, action yield, semantic-variable use, and constructor-disabled checked outcomes. |
| 3 | RB CSP WV93/98, then noL WV2 184/221--223 and fixed 401 | public binary `D`; internal n-ary live/action | Binary pair offer/publication and n-ary exact capture, prefix definitions, certified residual exclusions, provenance, projection, and durable action keys are implemented. Add the mutation-free native offerer and typed public controller surface. | Every offered payload, independent DSR/DRAT checks, effective fuzz reach, models checked on original CNF, noL recovery, and regression gates. |
| 4 | WV2 exact-cover 147 and independent-set 159; fixed 499/654 | public `E` and `I` | Generate controller-visible semantic variables and legal `I` cover payloads from exact retained images; add compact count/cover consequences where definitions alone do not propagate. | Strict current recognition on each candidate, payload coverage, proof/model checks, no use of historical chosen solutions. |
| 5 | Fixed/WV2 Ramsey 630/155 and tentative fixed 147; WV1 signed-cardinality; WV84 st-connectivity | `R`, `SC`, `ST` | Validate exact saved inputs against current recognizers and preserve startup boundaries. Extract continuing consequences only where a live proof contract exists. | Current-source recognition and independent proof checks; no inference from names or old matcher acceptance. |
| 6 | Fixed 991 graph labeling | exact internal interval recovery, exact-OR definitions, row-cover proof | Compose one authenticated public graph producer/action that records interval sources, occupancy definitions, row covers, graph supports, projection, and advice candidates. | Exact target mapping, bounded replay, independent prefix proofs and final model; no old graph constructor. |
| 7 | Mutilated chessboard, clique/color, binary PHP, equivalence chain | internal counting/row-cover engines | Generalize a source-bound cardinality/Hall/count-preservation action with DSR by default and a justified rule-specific PB route only if clausal proof cost is impractical. | Renamed/signed/shuffled premise validation, compact proof scaling, negative near-miss controls, public typed scheduling. |
| 8 | Sorting networks, Fermat, random circuits, Simon, automata, Stedman, sliding puzzle | `CG`, retained detectors, internal definition helper | Extract exact reusable theory relations and Boolean definitions one family at a time. Prioritize consequences that strengthen ordinary CDCL; do not port witness search, templates, fixed phases, or IDA*/factor/key search. | Complete source classification, soundness/projection argument, productive nonterminal clauses, then constructor-disabled checked recovery. |
| 9 | Fixed rows graded `N` | none | Perform source- and saved-state attribution before implementation. Many are generic search/MAB/symmetry or certificate-side misses rather than missing LymphoSAT theory. | A formula-bound mechanism signal; filenames and solver ownership alone are insufficient. |

The fastest path to broad coverage is therefore not one action per historical
handler. It is to finish repeated closure and action generation for the already
implemented GF(3), GF(2), DirectCSP, exact-cover, and independent-set surfaces,
then use the uncovered rows to define the next general n-ary CSP and counting
actions. Fixed proof-checker misses should be tracked alongside this matrix but
must not be counted as theory-action recoveries without a newly checked proof.
