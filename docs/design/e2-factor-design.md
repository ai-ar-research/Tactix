# E2 Factor — frozen compact design record

**Status:** implemented. The generalized rule is authoritative in
`docs/calculus.md` E2; implementation invariants and N1 status are in
`docs/implementation-notes.md` §5 and PROGRESS.

## 1. Reference shape

Factor is bounded variable addition. It finds a positive-gain rectangular grid
`{{f_i} ∪ R_j}`, introduces a fresh extension variable, adds dividers
`{x,f_i}` and quotients `{¬x} ∪ R_j`, then deletes the grid. Dividers precede
quotients and grid deletion is last so each quotient's RAT resolvents are the
still-live grid clauses.

## 2. Ownership boundary

The rule owns proof-bearing validation and rewrite. Policy owns admission,
candidate ordering, budgets, the BVE-coupled acceptance bound, and optional
composite scheduling. The typed exact-grid path and its lazy exhaustive
enumerator expose profitable factor/remainder subgrids independently of the
canonical matcher's choices; `CompleteCurrentPass` consumes that space under
the caller's application budget and stop signal.

## 3. Model and proof invariants

Extension identities are fresh and projected from output models. BVE does not
schedule them. Native LRAT uses vacuous RAT for dividers and one resolution
group per divider for each quotient. Every duplicated grid clause is removed
through the shared binary/arena deletion paths.

## 5. Implementation notes

### 5.4 Tick approximation and duplicate watches

Factor charges one tick plus one tick per 128 bytes of watch material touched,
matching Kissat's cache-line-shaped currency approximately. Tactix watch lists
may retain duplicate entries, so the meter is intentionally an approximation;
grid membership and gain calculations must still deduplicate logical clauses.
The current constants, counters, and deviations live in the scheduling ledger.
