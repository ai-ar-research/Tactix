# AM446: nonincremental reference proof producers mapped to typed Tactix actions

Date: 2026-09-25. Read-only audit of the locally pinned CaDiCaL 3.0.0,
SC24 Kissat, AE-Kissat-MAB, Satsuma SC26 and LymphoSAT sources against
Tactix `3ee44797`. This completes a **source-level proof-rule crosswalk**;
it does not certify N1 reachability, current certificate correctness or N4
scheduling parity. AM398/402/403 own the earlier emitter and PB findings.

## CaDiCaL producers

A scan of non-tracer `cadical/src/*.cpp` for calls to `Proof` that add a
derived clause, delete a clause, strengthen, or record weakening finds the
following producer modules. `clause.cpp` is the common clause/proof lifecycle
service, including the only direct RAT dispatch among these files
(`:548-640`), not an additional schedulable rule.

| Source producer | Relevant source event | Tactix rule/effect |
|---|---|---|
| `analyze.cpp` | root unit/empty and online strengthening (`:22-41,884-906`) | M3 Analyze, C2 Strengthen |
| `block.cpp` | blocked-clause weakening and extension stack (`:272-276,408-412,624-627`) | X1 BCE, reconstruction |
| `condition.cpp` | conditional deletion and witness restoration (`:790-794`) | X5 Condition |
| `congruence.cpp` | gate lemmas, transient deletion, clause strengthening (`:956-977,3479-3480,3709-3766`) | V6 Congruence, C2 effect |
| `cover.cpp` | covered-clause lemmas with weakening metadata (`:403-406,439-443`) | X3 CCE |
| `decompose.cpp` | equivalence binaries, replacement and deletion (`:445-469,613-617,665-674`) | V3 Substitute |
| `definition.cpp` | Kitten definition-core RUP lemmas/units (`:75-83,142-162,178-279`) | E1 BVE definition extraction |
| `elim.cpp` | eliminated-clause weakening (`:465,541,630-652`) | E1 BVE, reconstruction |
| `factor.cpp` | fresh-variable RAT and quotient deletion (`:619,663`) | E2 Factor |
| `subsume.cpp` | strengthening and subsuming deletion (`:140,164`) | C1/C2 Subsume/Strengthen |
| `sweep.cpp` | embedded proof replay, unit/equivalence and deletion (`:589-616,918-942,1151-1153`) | V5 Sweep |
| `vivify.cpp` | implication-guided strengthening (`:109`) | C3 Vivify |

These mappings are semantic families, not a proof that Tactix reproduces
every CaDiCaL candidate or all its proof-hint chains. The separate
incremental tracer events are deferred by plan §5.1/O7. `weaken_minus`,
`weaken_plus` and `strengthen` carry deletion/model-restoration bookkeeping
or shorthand for a clausal addition; AM403 checks the exact tracer boundary.
No additional *nonincremental clausal inference primitive* is exposed by this
producer scan beyond RUP, RAT, deletion and finite substitution witnesses.

## Kissat and competition winners

SC24 Kissat and AE-Kissat-MAB have byte-identical `proof.c` and
`definition.c` on the archived hashes in AM398. `proof.h:23-37` and
`proof.c:287-403` expose add/strengthen/delete clausal events, which map to
Tactix RUP/RAT/deletion. The source-level producer families map as follows:
`analyze.c`→M3; `forward.c`/`shrink.c`→C1/C2;
`substitute.c`→V3; `transitive.c`→V4;
`congruence.c`→V6; `eliminate.c`/`fastel.c` and `definition.c`→E1;
`factor.c`→E2. `fastel.c:391-457` deletes and weakens eliminated-variable
clauses; it does not create a new proof command. This is a proof-rule mapping,
not a claim of exact reference scheduling or candidate ordering.

Satsuma's SC26 SR path has clausal units, blocked clauses, binary cuts and
orbitopal fixing that map to RUP/RAT/X7 finite witnesses (AM398). Its VeriPB
mode and four active listed LymphoSAT family paths use general PB arithmetic,
redundancy/dominance and subproofs (AM402). AM408 independently observed
LymphoSAT `red`/`pol`/subproof output on a selected archived baseline row;
AM410/411 show Tactix can prove two particular winner rows through its
startup PBP or native DSR, without closing the **general live PB interface
gap**. Tactix currently exposes PBP only for terminal Lucky 19/20 at the
original-input boundary. This is the remaining qualified proof-calculus
coverage gap, not evidence that the same formulas lack clausal proofs.

## Acceptance boundary

The rule catalog and 30/30 inventory disposition check establish a typed
place for each mapped clausal family, but cannot prove all legal public
instances are reachable. In particular, H2 still rejects a legal X7
trajectory under the canonical no-option checker; AM444/445 only classify
bounded alternative representations. AM444's Sweep LRAT repair has two
checked handoffs but its affected Opus owner, full Debug and combined gates
remain live. Until independent original-CNF proofs/models, random-policy
reachability and the full Rank/B/C/WV gates pass on one combined pin, N1/N2
and any N4 parity claim remain open. A general PB action would also require a
deliberate format-boundary design under plan §5; this audit does not silently
expand the approved startup-PBP contract.
