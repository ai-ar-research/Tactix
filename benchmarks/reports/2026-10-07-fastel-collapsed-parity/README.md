# FastEliminate collapsed-result parity checkpoint

This checkpoint completes the remaining SC2024 FastEliminate feasibility
self-strengthening shape.  It is a general BVE change with no benchmark,
formula-size, family, or elapsed-time selector, and it changes no production
default.  Cumulative fuzz and regression acceptance remain open.

When an over-cap feasibility pair is physically binary by large, shares the
binary's non-pivot literal, and collapses to a unit, Tactix now publishes the
existing checked two-parent consequence before deleting the large source and
counts one strengthening.  A later capped decline retains that edit.  The
large-by-large collapsed path continues to publish its consequence while
keeping both parents, matching the reference.  Source `Ref`s, rather than
proof-only hint chains, prune tentative resolvents whose parent was replaced
or deleted, so proofless, DRAT/DSR, and LRAT operation follow the same mutation
sequence.  The shared-literal decision is made during the already charged
resolution scan; the change adds no uncharged large-clause search.

The immutable validation is under
`benchmarks/runs/20261007-fastel-collapsed-parity-r4`.  Its baseline is
`efdced45a`, source archive SHA-256
`e41a9542ad300c3a4e5d7dce5fb53dcf24d24a548f8d7d8dd4b5b09fe4190875`,
patch SHA-256
`c0f16338349738bf7c8f2a5930dcefeda59cb4b267607384da2a82fd412a0fc0`,
and wrapper SHA-256
`df2ae21f7b5c737edceec56c8bd15b95dd5199ef3557f13d80865a78d084913f`.
SLURM array `1754542` ran on node017:

- the Debug and ASan/UBSan builds passed `test_bve_invocation`, `test_bve`,
  and `test_scheduling`, with empty test stderr;
- the focused proof fixture emitted a 47-byte LRAT certificate, SHA-256
  `7114c8bfe744a17027e2362ab1f2ad2cb76ad877a2bbb07c953bbe5359997352`;
  `lrat-check` reported `VERIFIED` with ten additions and one deletion, and
  CakeLPR independently reported `s VERIFIED UNSAT`;
- documentation size and calculus/guide synchronization passed.  The Debug
  task's final status is 1 solely because the immutable archive omits the
  repository's untracked reference clones required by option coverage.  The
  same live-checkout option coverage (418 options), section references
  (74/74), and citations (45 resolved) completed with zero errors.

The earlier 300-second Rank569 pair established changed search work but no
runtime improvement, so this checkpoint makes no Rank569 closure claim.  The
same-source full proof-producing candidate remains active as SLURM `1754535`
on admitted shared scratch; only a terminal independently verified proof can
close that target.
