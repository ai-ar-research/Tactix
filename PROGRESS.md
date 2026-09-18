# Progress and resume guide

<!-- track-sched:begin -->
## Active ordered goal

**Phase 1 is complete.** Preserve its independently checked Rank 1--50 and
B28+C5 performance, WV61 coverage, public rule selectability, native DSR proofs,
original-CNF SAT models, and Claude-owned fuzz validation.

**Phase 2 is active.** Reach independently checked WV100 100/100 within 1,200
seconds and 32 GiB per case by integrating each owning baseline's techniques
into Tactix. No sequential portfolio, external solver stage, benchmark dispatch,
or arbitrary benchmark-tuned cutoff is allowed. Every integrated technique must
be at least as sophisticated as its owning state-of-the-art implementation,
including Kissat, Satsuma, and LymphoSAT mechanisms.

Use cpu-q for one-off profiles, correctness checks, and coarse solved/timeout
iteration. Use matched allocations for close runtime claims and the final Rank
and B28+C5 gates. All solver runs use Slurm, immutable pins, native proof/model
oracles, and bounded memory.

## Phase 1 checkpoint

AM149 is the accepted Phase 1 authority. Rank 1--50 solves 50/50 in 7,945.569
seconds, below the frozen 8,115.735-second gate. B28+C5 solves 33/33 in 893.232
seconds, below the frozen 952.263-second gate. Both use the matched c12,
32-GiB, 1,200-second contract. Rank 1 solves in 1.018 seconds; Rank 6 solves in
52.802 seconds after removing repeated complete-BIG scans and matching the
owner's exact marks and collection behavior.

The checkpoint preserves independently checked WV61 coverage. The complete
Release suite passes 162/162, native UNSAT proofs pass DSR-to-LSR-to-Trestle,
SAT answers pass original-CNF model checking, and the Claude-owned fuzz campaign
passes. Frozen authority:
`build/20260918-am149-helper-backed-cut-feedback-authority/manifest.json`.
Frozen binary SHA-256:
`2ef87798c56b42eb4aad4c9c5285f177f8634fb47dd0d8a690ca7a9846404485`.

The general performance repair retains one typed structured-root program,
progress-sensitive Satsuma-scale refinement, exact binary images and owner BIG
marks, sparse root collection, helper-backed graph-cut feedback, and reusable
local-search guidance. It adds no filename, rank, benchmark, or size-class
dispatch.

## Exact next action

Use the retained LymphoSAT subsolver-attribution table to prioritize unsolved
WV100 owner classes. Integrate each owner mechanism as a public typed Tactix
strategy, validate proofs or whole-CNF models independently, and rerun complete
Rank 1--50 and B28+C5 gates for every retained successor.
<!-- track-sched:end -->
