# WV100 orbitopal SR G0 obligation classification

Date: 2026-09-13. This is the bounded G0 result for the first structured
producer in [the WV100 mechanism protocol](../../docs/design/2026-09-13-wv100-mechanism-integration.md).
It invokes no reference solver and changes no Tactix rule or schedule.

## Question and method

Can a formula-driven orbitopal producer discharge every SR image obligation,
including dependencies between repeated unit additions, and does ordinary unit
propagation reach the observed empty residual?

An ignored Python prototype independently implemented the published orbitopal
unit schema. It found positive unique-literal clauses from literal incidence,
aligned them using unique cross-block negative-binary matchings, and admitted a
matrix only after every adjacent row swap was an exact automorphism of the full
input clause multiset. It then emitted upper-triangular negative units in the
required column-major order. For each unit it constructed the adjacent-row SR
witness with the two current matrix literals overridden to false and true.

Every clause image in `(F and prior units and candidate)|witness` was classified
as one of:

- a clause already in the original formula;
- a unit introduced by an earlier symmetry application;
- satisfied or tautological under the witness; or
- a concrete RUP discharge from the closure of `F`, prior units and the negated
  candidate.

The prototype rejected an application if any image was unresolved. Its SHA-256
was `901a943c8a721fe896b3fa748f5e70e487235d5d85ebd61e1c5e6b9d9d251d4a`.
Job `1688232` ran on `cpu-dense-hwulab-q` with eight CPUs, 32 GiB and a
300-second process limit. Inputs were decompressed through a pipe into memory;
no formula or proof file was created.

## Result

| Representative | Matrix | Unit SR applications | Direct original | Prior-unit dependency | RUP | Witness-satisfied / tautological | Unresolved |
|---|---:|---:|---:|---:|---:|---:|---:|
| WV100 rank 2, `php_sudoku_p15_h14` | 14 x 15 | 91 | 254,436 | 4,823 | 1,638 | 2,639 | 0 |
| WV100 rank 17, `chnl-020x061` | 20 x 61 | 190 | 13,908,190 | 19,095 | 10,260 | 11,780 | 0 |

The PHP sequence propagated all 210 variables and reached conflict. The channel
sequence propagated 1,220 of 2,440 variables and reached conflict. A second
cross-check streamed each augmented CNF directly from prototype version
`6ecea0ddf75eb91e7ab77a15a7f653e59dd2d31f285ff43768f9c11516e0040c`
into accepted Tactix Release
`b63f30031229a36b9fea46e37590e892ab4a178a1239357ff3f80557548e1413`.
Both tasks in job `1688243` returned `s UNSATISFIABLE` with solver status 20 in
one and two seconds. This independently confirms the prototype's final unit
propagation result; it does not certify the SR additions themselves.

Two negative controls fired. Removing the row swap from the first witness left
14 unsupported PHP images and 60 unsupported channel images. The same strict
producer found no ULC matrix on Johnson-style `count_p2_M21` and exited with its
documented non-applicable status; that third task explains array `1688232`'s
aggregate failed state.

## Design consequence

G0 closes for the first structured orbitopal producer. Its direct-image,
prior-unit and RUP obligations are all necessary; treating every automorphism
image as a direct formula clause would be incomplete. The ordered producer is
also stateful: later unit witnesses depend on earlier symmetry units, so online
interruption must preserve the producer cursor and committed-unit prefix.

Together with the native-LRAT G1 prototype and the structured G2 boundary, this
permits implementation of the first structured SR increment. G0 remains open
for Johnson and general-automorphism producers when those distinct mechanisms
are introduced.
