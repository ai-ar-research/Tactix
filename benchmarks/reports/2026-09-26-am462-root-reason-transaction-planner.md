# AM462: one checker-independent transaction plan covers D1 and both H2 controls

Date: 2026-09-26. This is a bounded proof-planning diagnostic, not a solver
repair or N2 acceptance. It changes no public X7 action, checker binary,
checker option, or canonical native DSR→LSR→Trestle route.

The planner reads each archived original CNF and the first explicit root
unit plus its immediately following deletion block. It propagates that unit
over the original clauses, records the clause chosen as each implied
literal's reason, and selects only deletions of those reason clauses for
early scheduling. Before each selected deletion, it checks whether the
unit remains RUP without that clause. If not, it enumerates
non-tautological resolvents of the clause with another still-live clause,
selects the shortest lexicographic resolvent that restores the unit's RUP,
and emits it as a proof-only RUP bridge before deleting the reason. Candidate
selection uses only local propagation and resolution, never a checker trial.
The frozen checker then independently validates the generated complete proof.

| Archived case | Early reason deletions | RUP bridge | No-option forward/backward + Trestle |
|---|---|---|---|
| D1 29v/27c Probe→CCE | physical line 11, `(21 ∨ 10)` | `(9 ∨ 10)`, resolving input clauses 14 and 2 on `21` | 4/4 verified |
| Minimal H2 21v/16c | physical line 7, `(¬18 ∨ 19)` | none | 4/4 verified |
| Original H2 21v/42c | physical lines 19, 21, 23, 28, 29, 30 | `(¬15 ∨ 17)`, resolving input clauses 32 and 33 on `16` | 4/4 verified |

For original H2, the six automatically selected deletion lines exactly match
the independently diagnosed implied-unit reasons in AM445. Moving *all* 29
first-block deletions instead is not valid: even with four greedily selected
bridges that preserve the first unit, the later proof fails forward checking
at addition 19. This negative control shows why reason selection is part of
the mechanism, not just a cost optimization; run
`python3 benchmarks/probes/scratch/am461-proof-bridges/greedy.py --all` to
reproduce it.

Reproduce with `python3 benchmarks/probes/scratch/am461-proof-bridges/plan.py`.
The generated proofs are `d1-planned.dsr` (`f3970d17...`),
`h2-minimal-planned.dsr` (`993ca37a...`), and `h2-original-planned.dsr`
(`58ed199c...`); each is checked in both directions and each LSR is verified
by frozen Trestle against its exact original CNF. The planner is limited to
the first unit and a following block of original-clause deletions. It does
not yet handle later units, proof-added clauses, duplicates, arbitrary
public X7 timing, or the full checker watch/UP order. Production work needs
a streaming proof-state transaction with explicit RUP and population
invariants, followed by latest-source Opus fuzz and N1--N4 gates. No
instance-specific ordering or silent checker-contract change is acceptable.
