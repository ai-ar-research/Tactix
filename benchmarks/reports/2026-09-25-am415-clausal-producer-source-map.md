# AM415: bounded CaDiCaL/Kissat producer-to-rule source map

Date: 2026-09-25. Read-only audit on Tactix `c7b1e795`, pinned CaDiCaL
`7b99c07f0bcab5824a5a3ce62c7066554017f641`, and local Kissat
`8af8e56f174b778aef3aa45af9f739b2a5f492c2`. No solver, proof route,
checker, rule guard or schedule was changed. AM403 covers their emitter
interfaces; this report checks the transformations at the producer call sites.

## Source map, not an N1/N2 verdict

| Reference producer/source | Observable transformation | Tactix catalog candidate |
|---|---|---|
| CaDiCaL `analyze.cpp`, `clause.cpp`; Kissat `analyze.c`, `clause.c`, `assign.c` | learned clause/root unit/empty and on-the-fly strengthening | M3 ConflictAnalysis, M1 Propagate, C2 Strengthen |
| CaDiCaL `block.cpp`, `cover.cpp`, `condition.cpp` | blocked and covered/asymmetric-covered deletions; conditional-autarky deletion with model-extension data | X1 BCE, X3 CCE, X5 Condition |
| CaDiCaL `congruence.cpp`, `decompose.cpp`; Kissat `congruence.c`, `substitute.c` | equivalence/gate consequences, SCC representative substitution and clause rewriting | V6 Congruence, V3 Substitute |
| CaDiCaL `elim.cpp`, `definition.cpp`, `factor.cpp`; Kissat `eliminate.c`, `definition.c`, `resolve.c` | resolution elimination, Kitten gate/core extraction with temporary RUP lemmas, fresh-variable factoring with RAT divider | E1 BVE, E2 Factor |
| CaDiCaL `subsume.cpp`, `backward.cpp`, `instantiate.cpp`, `vivify.cpp`; Kissat `forward.c`, `vivify.c` | subsumption, RUP strengthening/instantiation and vivification | C1/C2, X4 Instantiate, C3 Vivify |
| CaDiCaL `sweep.cpp`; Kissat `sweep.c`, `fastel.c`, `probe.c` | environment/core consequences, connected substitutions, failed-literal units and transitive cleanup | V5 Sweep, V1 Probe, V3 Substitute, V4 TransitiveReduce |
| CaDiCaL `collect.cpp`; Kissat `collect.c`, `propdense.c`, `internal.c` | deletion and add-before-delete maintenance of already justified clauses | S2 Reduce / shared formula maintenance, not a new inference |

The inspected CaDiCaL transformation files call the proof layer through
`add_derived_clause`, `add_derived_rat_clause`, derived unit/empty,
`strengthen_clause`, `delete_clause`, or weaken/restore bookkeeping. In
particular, `cadical/src/definition.cpp:52-163` prefixes Kitten core lemmas
with a proved unit and deletes the temporaries; `kissat/src/definition.c:45-87`
does the analogous operation. These are unit consequences inside elimination,
not unrestricted fresh-variable definitional extension. CaDiCaL's
`factor.cpp:619-663` is the fresh-variable RAT case mapped to E2. Its
`cover.cpp:405-442` records successive model-extension segments, mapped to
X3's chain rather than a bare deletion. `condition.cpp:779-797` records an
autarky witness; X5 now exposes an explicit finite `(clause,cube)` action even
though CaDiCaL's default `condition=0` (`options.hpp:58`).

Kissat's direct proof macros in `analyze.c`, `assign.c`, `clause.c`,
`collect.c`, `congruence.c`, `definition.c`, `eliminate.c`, `fastel.c`,
`forward.c`, `internal.c`, `propdense.c`, `resolve.c`, `substitute.c`,
`sweep.c`, and `vivify.c` add clauses/units/empty, delete, or shrink by
add-before-delete. The map covers those call-site families and their
indirect producer roles; it does not establish identical candidate selection,
effort accounting, or proof byte streams. Tactix's frozen registry declares
the candidate rule families (`DEVELOPMENT_PLAN.md` §5.2;
`src/rules/registry.cpp`), and `check-inventory.py` reports 30/30 *dispositions*.
As its own contract warns, that check does not prove actual registration,
legal-instance reachability, or correctness of the dispositions.

## Conclusion and remaining evidence

This bounded source comparison found no further **clausal proof-event type**
beyond the AM403 RUP/RAT/delete boundary, and no obvious missing clausal
transformation family among these inspected producers. It does **not** close
producer parity: the semantic breadth of each mapped Tactix action, its
public N1 instance surface, independent N2 proof/model checks on all legal
paths, and N4 default scheduling still require current-source evidence.
CaDiCaL's incremental assumption/constraint protocol remains deferred by
plan O7 and was not counted as a nonincremental proof rule.

The separate AM402/408 winner finding remains genuine: LymphoSAT and
Satsuma/VeriPB have general live pseudo-Boolean arithmetic, redundancy and
dominance derivations; Tactix has only terminal startup PBP for Lucky 19/20.
The exact AM410/411 paired probes show that this can be a proof-size gap
without proving missing semantic UNSAT coverage on those rows. No N1--N4 or
Tier-D acceptance follows from this source map.
