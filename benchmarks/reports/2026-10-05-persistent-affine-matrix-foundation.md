# Persistent affine matrix foundation

## Decision

Repeated static X9 export is not the GF(2) search mechanism.  The current
Rank263 screen applied 1,328 authenticated rows but created 37,748 extension
variables, spent 138.1 of 300 seconds in propagation, and remained UNKNOWN.
CryptoMiniSat's controlled result instead attributes its 7.12-second UNSAT to
a persistent Gaussian matrix.  Tactix therefore needs bounded matrix
implications and conflicts, with a checked clausal reason published before
either can affect CDCL.

## Implemented foundation

`affine_matrix` copies exact recovered equations under a nonzero immutable
owner identity and retains every reduced row's sorted source-equation
combination.  `synchronize` accepts a complete trail snapshot, so extension,
backtrack and reassignment use one interface.  It conditions the source rows,
computes a deterministic reduced basis, reports conflicts before implications,
and returns the globally entailed clausal reason: false assigned support
literals followed by the implied literal when present.  Unrelated trail
assignments are excluded from the reason.

Rows, variables, width, source count, trail length and work are explicitly
bounded.  Invalid input, interruption and exhaustion leave the matrix and
caller event unchanged.  The focused oracle checks a two-source implication
that neither original row can propagate, the corresponding conflict, an
opposite implication after backtrack/reassignment, exact sources, reason
entailment over every assignment, unrelated-trail filtering and transactional
failure behavior.  Debug and ASan/UBSan (leak detection disabled because the
runner is ptraced) pass.

## Deliberate boundary

This component does not mutate the Boolean formula, enqueue a literal, report
a solver conflict, allocate extension variables or emit proof.  The next
increment must bind it to X9's live authenticated owner and turn the returned
source combination and reason into an atomic DSR/DRAT preflight and
publication.  Only a successfully published ordinary clause may then drive
CDCL.  Exact reason keys should suppress duplicate proof work.  Incremental
row maintenance can replace snapshot reduction after the semantic and proof
boundary is checked; it must preserve the same interface and results.
