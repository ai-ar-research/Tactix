# AM444: one H2 deletion can precede its implied root

Date: 2026-09-25. Checker-only control on the frozen AM436 16-clause public
H2 reproducer; no solver, public X7 action, checker route or checker option
changed. It does not close H2 or N2.

The exact ASCII spelling of AM436's binary DSR is archived in the AM437
artifacts. Moving only its `d -18 19 0` line from position 7 to the first
proof line makes the pinned **no-option** `dsr-trim` verify the complete proof
in both forward and canonical backward modes. Both independently elaborated
LSRs return `s VERIFIED UNSAT` under pinned Trestle against the exact original
16-clause CNF. The moved proof is
`benchmarks/probes/am444-h2-early-delete/early-delete.dsr`; checker and input
hashes are in its `inputs.sha256`.

This isolates a temporal window for this one checker-retained reason: when
deleted before the first derived unit, it has not yet become a global-UP
reason. Tactix emits that deletion later, when no-option `dsr-trim` ignores
it. A general emitter would have to establish such a valid ordering for every
retired reason without invalidating earlier derivations. AM443's separate
two-variable input has a reason already implied by an *original* unit, so
moving a deletion before proof additions cannot create that window. Its
no-option negative control and AM404/407's non-monotone option controls still
apply. The canonical checker route remains unchanged; no legal X7 tuple has
been suppressed.
