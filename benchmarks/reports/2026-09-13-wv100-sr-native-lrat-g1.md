# WV100 substitution-redundancy G1: native LRAT prototype

Date: 2026-09-13. This is the bounded G1 proof-feasibility result for
[the WV100 mechanism protocol](../../docs/design/2026-09-13-wv100-mechanism-integration.md).
It is not solver coverage and changes no Tactix rule or schedule.

## Question and method

Can Tactix translate a general SR clause plus finite literal substitution into
its existing native `add_rup` / `add_rat` / `del` vocabulary, capturing LRAT
hints against the exact live proof population rather than relying on an
external converter?

An ignored Python prototype independently implemented the published SR
extension-variable construction. Before writing each addition it attempted
RUP by unit propagation. If that failed, it checked the first literal as the
RAT pivot against every live clause containing the opposite pivot, including
tautological resolvents, and recorded the complete signed LRAT hint stream.
Deletions addressed one exact live clause identity. The input was the official
`sr2drat` repository's small `R_18_4_4` example; neither Satsuma nor `sr2drat`
was executed.

| Artifact | SHA-256 |
|---|---|
| CNF, 153 variables / 6,120 clauses / 151,575 bytes | `dcdc225ccf35b3366e5b3bae60f1b3995b46cbd670e514ee7e8ccf80a05e0b4b` |
| SR, 39 records / 17,155 bytes | `f72dfe042b80c500dbec05d5881eca4169668b6240c714076015d992c74ed4b1` |
| Prototype | `6f1369de51bb6703f9ab079f46d9062b3b169e8f27398714ad79ee828ab6e439` |
| Native LRAT | `ea2dbfc29aef098407adb3747300ccf0fc448d526d682844ec5fb5c4c5fbff9e` |

## Result

The prototype expanded the 39 SR records into a 47,445,835-byte native LRAT
proof with 888,337 operations:

| Operation | Count |
|---|---:|
| additions | 444,188 |
| RUP additions | 425,381 |
| RAT additions | 18,807 |
| RAT resolution-partner groups | 23,233 |
| exact deletions | 444,149 |

The proof population peaked at 13,086 live clauses and ended at 6,159: the
6,120 inputs plus the 39 retained SR lemmas. Proof-only variables reached 2,739
while the formula had 153 variables. They never entered a solver model.

The final proof passed pinned `cake_lpr` with `s VERIFIED UNSAT` in 3.21 seconds
at 2,539,124 KiB peak RSS. Removing the final empty-clause line produced
`empty clause not derived at end of proof` in 2.41 seconds, so the acceptance
oracle's negative control fired. Prototype construction took 813.62 seconds
for the first 38 records; rebuilding the exact state and appending the terminal
empty after fixing its parser edge case took 4.17 seconds. Peak prototype RSS
over those stages was 493,904 KiB. These measurements describe this simple
prototype and are not production estimates.

Legacy `lrat-check` rejected line 16,644, a valid vacuous-RAT addition whose
opposite-pivot introduction clause had already been deleted. Its implementation
retains the stale introduction index when a RAT line has no negative groups.
The formally verified CakeML checker accepted the same proof, and the prototype
had checked the exact live population. `lrat-check` therefore remains a
diagnostic oracle; production acceptance continues to use `cake_lpr`.

## Design consequence

G1 succeeds: the existing proof vocabulary can express independently checked
general SR expansion. The solver implementation still needs a streaming,
interruptible proof transaction, recycling of deleted proof-only records,
native C++ RUP/RAT hint construction, and proof-shadow equality before O16 can
close. Expansion depends strongly on the live formula and witness support, so
admission must use configurable work and scratch budgets rather than a fixed
per-SR-line multiplier. G0 classification and G2 schedule-boundary testing
remain open.
