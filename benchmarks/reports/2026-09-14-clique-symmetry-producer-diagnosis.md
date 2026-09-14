# Clique symmetry producer diagnosis

## Observed gap

The current exact-row producer makes no SR transition on WV100 Satsuma rank 4,
`cliquecolouring_n13_k8_c7`. The formula has 21 unsigned long clauses: 13
clauses of size 7 and eight clauses of size 13. Exact adjacent swaps of the
eight size-13 blocks exist, but a row-only partial permutation is not an
automorphism because the corresponding graph-selection and coloring variable
families must move together.

A pinned Satsuma 1.3 run recognizes two row structures and one row-column
structure, emits 91 symmetry/orbitopal units, performs 188 propagations, and
finishes UNSAT in about 0.013 solver seconds. Its DSR witnesses use complete
formula automorphisms. For example, the first observed unit maps candidate
literal `-170` to witness literal `171` while also permuting the coupled
variable families. This rules out scheduling as the cause of the zero-transition
result: Tactix lacks the producer that completes and fixes the full symmetry.

## General prototype result

A formula-derived colored-CNF incidence graph finds 11 generators without a
benchmark name, rank, or family predicate. Passing those generators through
Tactix's accepted direct-SR validator produced 22 independently valid SR units,
including support sizes of 24 and 72. The resulting formula still timed out at
60 seconds. Raw generator involutions therefore expose real transitions but do
not reproduce Satsuma's closure.

The remaining mechanism is stabilizer-chain clause fixing: sift all detected
generators into a group representation, select a literal with a nontrivial
stabilizer orbit in a unit-like or at-most-one structure, obtain a transversal
element mapping it to another clause literal, emit the corresponding full
permutation as a direct DSR witness, extend the stabilizer base, and repeat.
This is a general formula mechanism and can generate useful group elements that
are absent from the detector's initial generator list.

## Implementation boundary

Prototype the stabilizer loop with deterministic seed, explicit generator,
time, memory, unit, and iteration budgets. Expose those values through the typed
strategy settings; retain no producer-side admission or search constants. Apply
only at level zero or after the existing explicit Restart-to-root transition.

The inspected DejaVu implementation uses exceptions and RTTI, while Tactix's
core is compiled without both. A production integration must isolate a pinned,
licensed backend in its own translation unit or library, catch backend failures,
and expose a narrow failure-safe result interface to the solver. Direct inclusion
in `tactix_lib` is not an acceptable build boundary. First acceptance requires
the rank-4 effective transition, complete direct-DSR elaboration and Trestle
verification, deterministic replay and interruption controls; the broader
57-row Satsuma group and the fixed performance gates follow.
