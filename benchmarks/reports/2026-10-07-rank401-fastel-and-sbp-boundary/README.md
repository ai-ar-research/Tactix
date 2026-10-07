# Rank401: matched search and one differing symmetry clause

Current-source37226diagnostic1754585 feeds the exact archived Satsuma residual to Tactix and Kissat4.0.4 under matched initial fast-elimination settings. All captured decision tuples agree, including conflict index, decision index, decision level, mode and signed external literal:249/249 with fastel0 and364/364 with fastel1, within the50-conflict window. The actual bundled Satsuma+Kissat defaults to fastel0; Tactix defaults to its initial fast-elimination pass. That configuration difference explains the earlier first-decision1419-versus1416 mismatch on this identical CNF. This is bounded prefix evidence, not whole-run equivalence or a reason to change a global default.

The actual integrated transformation boundary still differs. Authoritative snapshot1754599 and offline comparator1754600 find8333canonical clauses, identical38root assignments and1051queue variables in both handoffs. Exactly one clause differs:

| Integrated Tactix | Actual Satsuma |
| --- | --- |
| -724 OR1417 | -1419 OR726 |

A fresh Tactix parse of the Satsuma residual matches its canonical clause multiset exactly. The integrated activation queue first differs from that fresh parse at index2: variable4 versus3. Therefore formula selection and presentation remain separate issues even though search matches on identical input under matched settings.

Origin trace1754604 identifies the differing clause as the depth1Schreier orbit cut. Its Satsuma trace variant reproduces BOTH the exact baseline residual hash and exact prefix-proof hash, so the trace is attributable to the reference. Satsuma chooses representative1419, orbit member726, transversal support12. Tactix chooses local point1446 (external724), with the remaining orbit member1417. Both encode an orbit cut as negated representative OR other; there is no observed proof-sign error.

The active owner in `src/policies/structured_root_strategy.cpp` explicitly selects `FirstAmoThenForwardRemainder` but leaves typed `cut_point` at `FirstOpen`. Its source comment records a deliberate historical choice: largest-orbit/highest-variable cuts had re-rolled satisfiable trajectories in earlier gates. Satsuma's `find_non_trivial_orbit` scans unordered variables backwards, preferring the highest variable of the largest nontrivial orbit. The separate legacy preset in `default_policy.cpp` already assigns `LargestOrbitLast`, but it is not the active owner observed here. Updating that legacy preset would not address this result.

The next approved diagnostic,1754605, changes only the existing typed cut-point assignment in an immutable archive of the active owner. It must first reproduce the reference clause multiset, then compare queue/presentation before search experiments. Production remains unchanged. Prior negative Rank16/23 controls prohibit promoting a blanket ordering or fastel change without generalization gates; TierD899/WV166 is a later Satsuma-policy sentinel, with no new run launched here.

Snapshot wrapper failures1754593/1754596 are retained: the first omitted the macro-trace gate, and the second exited before formula capture.1754599 uses the existing post-snapshot exit hook and finishes each arm in under0.5s. All builds and evaluations ran on Slurm. These are diagnostic artifacts, not new independently verified instance closures.
