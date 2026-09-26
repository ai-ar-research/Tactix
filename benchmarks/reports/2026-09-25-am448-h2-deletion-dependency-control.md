# AM448: original H2 passes canonical checkers under a selective deletion order

Date: 2026-09-25. This is an exhaustive **checker-only** ordering control on
AM423's frozen 21-variable, 42-clause public X7 replay. It changes neither
the solver nor the legal action surface, proof emitter, checker pin, or
canonical no-option DSR-to-LSR-to-Trestle route. H2/N2 remain open.

The exact 680-byte binary proof was decoded line-for-line to the archived
ASCII representation (SHA-256 `af2f33c7...`). AM445 identified six input
deletions whose late execution the no-option checker ignores after its first
global-UP closure. AM448 tried **all 64 subsets** of moving those six existing
deletion lines before the first proof addition, preserving every other line and
their relative order. The original CNF (`4b0fa708...`) and frozen AM369
`dsr-trim` (`3b5c7737...`) were used throughout.

Only four subsets pass both forward every-lemma checking and backward
elaboration. All contain physical deletion lines **21, 23 and 29** (clauses
`-8 -9`, `-9 15`, `-19 20`); lines 19 and 30 are optional, and line 28
(`-15 16`, input clause 32) must remain after the first unit. The accepted
forward LSR's first unit `17` cites clause 32, making this dependency
explicit. Moving all six, as in AM445, invalidates that unit's UP derivation.
The 64 checks yielded four
forward successes and 16 backward successes, so a backward-only success is
not sufficient. The smallest passing subset produces ASCII DSR SHA-256
`b782b87a...`; its forward and backward LSR files independently pass pinned
Trestle `srcheck` (`19bf79f4...`) against the **original** CNF, each reporting
`s VERIFIED UNSAT`.

This is a real canonical-contract proof for the bounded original H2 instance,
but no general production repair: choosing these three physical lines for this
CNF would be instance-specific tuning. The result establishes a more precise
design constraint. Deletions that will become protected checker reasons must
be scheduled before the root closure **only when the earlier addition remains
independently provable**. A general emitter needs a dependency-aware proof
transaction or another derivation that handles this conflict across arbitrary
legal X7 actions. H2 stays open until such a mechanism and cumulative fuzz,
proof/model, and performance gates pass.

Reproducible inputs, all 64 verdicts, selected proof, both LSRs, and checker
logs are in `benchmarks/probes/am448-h2-subset-order/`; run its `verify.py`.
