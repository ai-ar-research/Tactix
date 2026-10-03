# Live parity adapter boundary validation

The [fixed native run](../runs/20261003-parity-live-boundaries/) extends the internal adapter's evidence beyond binary equivalence. No solver implementation changed in this checkpoint; the new test exercises the current adapter directly. Public action/controller wiring and production acceptance remain pending.

| Case | Expected live outcome | Published consequences | Reserved helpers |
|---|---|---:|---:|
| Sum of ternary and binary XOR encodings | New three-variable XOR equation | 4 ternary clauses | 5 |
| Sum of two ternary XOR encodings | New four-variable XOR equation | 8 quaternary clauses | 7 |
| Cancellation to one variable | Root unit | 1 unit | 3 |
| Same unit against an opposite original root unit | UNSAT | 0 new assignments; empty clause emitted | 3 |
| Unit derivation after variable compaction | Correct external-to-internal remapping | 1 unit | 4 |

Each case checks that a too-small tree allowance leaves external identities and proof-event numbering unchanged. Every nonterminal case repeats the application and checks both NoOp and no additional namespace reservation. Compaction explicitly retires an unconstrained original variable; the derived external unit must map to the correct remaining internal variable. This does not yet cover every remapping or interruption scenario.

Every case passes canonical witness-free DSR conversion/DRAT/CakeLPR, forward every-addition DRAT and all-addition LRAT/CakeLPR. The root-conflict case is checked as a complete proof on its original CNF. Each nonterminal case uses a disjoint initially unit-free closing gadget beyond all helper identities; its LRAT elaborator asserts that prefix hints do not cite the added gadget premises. The checker commands, source/certificate hashes, outputs and individual results are retained. No target-scale runtime or compactness claim follows from these fixed controls.

The final focused pin is `build/pin-parity-live-boundaries-7542c199-02`. Like the previous adapter checkpoint, it links newly built kernel/adapter objects against the documented immutable parent, with test assertions active. It is not full Debug or an all-current-source acceptance pin. The first pin differs only in default temporary-file naming; supplied run-directory fixtures remain byte-identical under the final test.

## Existing fuzz campaign status

The public owner interim report now states zero failures for each of the earlier graph/guidance snapshot's DefaultPolicy and random-controller legs, with 100,010 generated cases plus 51 corpus entries each. Default: 57,225 SAT, 40,148 UNSAT, 2,637 non-terminating at five seconds. Random: 56,408 SAT, 37,894 UNSAT, 5,708 non-terminating. The owner identifies corpus entry 022 as crash-only for the default leg. These are owner-reported interim outcomes, not an independent Codex recount or terminal acceptance.

Scheduler inspection confirms both main legs completed 0:0; targeted coverage job 1747627 and mining/replay job 1747628 are running. The public report snapshot is retained with this run. The campaign predates the parity changes and cannot accept them; its complete targeted/replay disposition still belongs to Claude. No private campaign files were inspected.

Next expose the parity adapter through the shared typed action, replay and raw feedback contract, with a durable provenance-stream binding and appropriate scheduling. Interruption, further negative-interface coverage, full Debug, current-source both-policy fuzz and all four enabled regression gates remain necessary. No real-instance solver campaign or saved baseline comparison was rerun. All 44 target obligations and other structural techniques remain open.
