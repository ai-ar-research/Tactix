# AM372: X5 root-satisfied candidate legality

Date: 2026-09-24. Parent `e764cf63`. This is a general N1/Condition guard repair and an ungated semantic-rule checkpoint, not combined N1/N2 acceptance or Tier D.

## Finding and change

The X5 calculus admitted a live clause already satisfied by a permanent root value: `¬C` has no root-consistent assignment, so the conditional-autarky implication is vacuous and a reconstructed root-extending model never needs the cube. The implementation nevertheless rejected every such candidate through `k.values[lit] > 0`, an engineering restriction in implementation-notes §16. This removed a legal public action, especially when a clause remained live as a root propagation reason.

`condition_prepare` now validates a root-true candidate's tuple and exact live target but skips the vacuous local implication scan. The existing binary and large deletion primitives retire any root reason, publishing its already-entailed DSR root unit before deleting the clause. All other X5 checks (quiescent root, witness consistency, live exact candidate, stop, binary all-copy deletion, reconstruction) remain. There is no formula, variable, size, benchmark, or phase-name special case. The calculus proof delta, implementation boundary, and rule guide now state the root-reason unit event.

## Fixed evidence

- An internal rule fixture reaches both live binary and large root reasons, applies X5, proves deletion, verifies the original root assignment remains, and checks the reason becomes a permanent unit marker. The old explicit `k.values[lit] > 0` condition would reject both.
- The public C certificate driver derives `¬2` (and for a large candidate `¬7`) through an individually selected Probe action **after input completion**, then propagates. This leaves the exact selected clause live and root-satisfied; X5 applies through `tactix_apply_condition`. The initial attempt with an imported root unit showed why an input-satisfied clause is not a valid reach witness in strict LRAT: normalization can retire it before X5. The later-Probe fixture is the real legal boundary.
- Duplicate binary and large candidates, each both ordinary and root-satisfied, produced 20 UNSAT proofs across DRAT binary/ASCII, native LRAT, and DSR binary/ASCII. All verified against the exact original CNF with pinned independent checkers; all 20 corresponding empty-proof controls rejected. Root-binary proof sizes were 44/90/171/67/127 bytes and root-large 57/118/219/88/185 bytes in that order. The original ten cases also reverified.
- Two SAT paths (`sat` forcing cube reconstruction, and `root-sat` with no repair needed) returned models accepted against their original CNFs by `tools/fuzz/modelcheck.py`; corrupting literal `1` to `¬1` was rejected in both. The full 22-case oracle passed in Release, Debug, and an updated ASan+UBSan build; `condition`/`api_n1` passed under all three. The Release eight-test local set (`proof`, output failure, API, scheduling, journal, Condition) passed 8/8. Documentation inventory, proof-rule sync, option coverage, boundary, and size checks passed with the existing six near-cap warnings.

## Limits

These are fixed microcases. An immutable-source Opus affected-scope fuzz campaign for this guard widening, random mixed-rule schedules, and representative broader proof/model checks remain due. The prior AM369 owner still evaluates its older `0837a631` snapshot and cannot validate this change by transfer. X5 still lacks an automatic cube producer and typed online-program batching; other engineering restrictions and the learned-binary local-check conservatism remain for the N1 audit. N2 full-path, N3/N4, retained Rank/B+C/WV gates, online-learning cleanup, and the later full Tier D run are not accepted here.
