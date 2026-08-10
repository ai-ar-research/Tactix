# Composite Factor grids — frozen compact design record

**Status:** implemented as a policy-controlled Factor scope; current defaults and
measurement belong to the scheduling ledger.

## 1. Mechanism

A Factor pass may allow freshly introduced extension literals to participate in
later grids. Every accepted grid has positive clause reduction, so the live
clause population supplies the termination measure even though the candidate
universe grows during the pass. The proof-bearing E2 rewrite is unchanged.

## 2. Boundaries

Composite admission is strategy, not an abstract guard. It cannot narrow the
public E2 instance surface. The rule validates every selected grid and emits the
same RAT additions and deletions as a non-composite grid. Output projection and
BVE's extension-variable exclusion remain unchanged.

## 3. Current disposition

The old density gate and pre-N1 argument are retired. Composite scheduling is
kept only as a scoped arm because a preprocessing-wide blast over-applied; its
default, counter, and reopening condition are owned by
`docs/implementation-notes.md`. The calculus owns the pure remainder-grid
semantics and contains no separate “composite witness.”
