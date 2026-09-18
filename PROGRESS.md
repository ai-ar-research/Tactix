# Progress and resume guide

<!-- track-sched:begin -->
## Active ordered goal

**Phase 1 comes first.** Independently check Rank 1--50 at 50/50 and B28+C5 at
33/33, each case within 1,200 seconds and 32 GiB. Aggregate solver time must be
below the frozen R16 gates: Rank below 8,115.735s and B28+C5 below 952.263s.
Preserve WV61 coverage, public rule selectability, native DSR proofs and original
CNF models. Rank 1 and Rank 6 overhead must stay repaired through general
mechanisms. Individual trajectory noise is acceptable when both full-suite gates
pass and no unexplained drastic regression remains.

**Phase 2 starts only after Phase 1 passes.** Reach independently checked WV100
100/100 within 1,200 seconds and 32 GiB per case by integrating each owning
baseline's techniques into Tactix. No sequential portfolio, external solver,
benchmark dispatch, or arbitrary benchmark-tuned cutoff is allowed. Every
integrated technique must be at least as sophisticated as its owning state-of-
the-art implementation, including Kissat, Satsuma and LymphoSAT mechanisms.

Use cpu-q for one-off profiles, correctness checks and coarse solved/timeout
iteration. Use matched allocations for close runtime claims and the final Rank
and B28+C5 gates. All solver runs use Slurm, immutable pins, native proof/model
oracles and bounded memory.

## Current checkpoint

AM149 remains the accepted Phase-1 authority. Rank 1--50 solved 50/50 in
7,945.569s and B28+C5 solved 33/33 in 893.232s under the matched c12 contract;
WV61 is 61/61 clean and Claude-owned fuzz is PASS. Authority:
`build/20260918-am149-helper-backed-cut-feedback-authority/manifest.json`.

AM160 is the first Phase-2 successor. LymphoSAT's retained owner-attribution
table is `benchmarks/runs/20260819-tierd-lymphosat-modlog/results-table.tsv`.
It identified `oddball-weighing` as owner of four open cases. Tactix now exposes
a general typed Lucky instance 10 for structurally recognized pair-choice and
signed-ternary blocks, with the owner's seeded annealing trajectory, bounded
admission, private auxiliary completion, whole-CNF validation, and H-only
guidance on failed search. There is no filename, rank, benchmark, or size-class
dispatch. Authority:
`build/20260918-am160-owner-exact-balanced-signature-authority/manifest.json`;
binary SHA-256 `f16414d77507593d77043f28e7979231896835271fdd9427c093b85302597ec6`.

AM161 job 1702525 solves WV89/92/94/100 in 2.169/2.420/3.272/3.923s. All four
original-CNF models pass, cleanup is clean, and each case used one restart,
26--225 annealing steps, and three completion rounds. AM166 job 1702708 adds
WV27 in 0.066s and 12.3 MiB with a verified original-CNF model. Its general typed Lucky11 action
recognizes ordered arithmetic-circuit input blocks, recovers the comparison
constant, runs the owner's deterministic 64-bit factoring strategy, privately
completes auxiliaries to a feedback fixpoint, and validates the whole live CNF.
Frozen binary SHA-256 is
`6340aec09e641a242e2afb271cdb2f67806d5bab2ee69ce65c696cca7d417e06`.
AM168 job 1702754 adds WV40 in 0.066s and AM170 job 1702885 adds WV73 in
1.319s; both original-CNF models pass and cleanup is clean. Independently
checked coverage is 89/100. AM171 job 1702894 adds WV85 in 28.115s after
941,695,142 numeric key candidates; its 8,896-clause original-CNF model passes.
Open ranks are 1, 25, 26, 80, 83, 84, 86, 87, 95, 96 and 97.

The cumulative Simon successor complete Release suite passes 167/167. Complete matched AM160
successor gates are live: Rank 1--50 job 1702533 and B28+C5 job 1702534, both
c12, 32 GiB, 1,200s, full-width on cpu-dense with immutable pins.

The user-authorized frozen AM149 full Tier-D campaign is job 1702332. Its
750 GB pending-task guard and the stricter global 700 GiB guard are live;
running tasks are never held or interrupted.

## Exact next action

Finish jobs 1702533/1702534 and retain the successor only if both complete
coverage and aggregate gates pass without a drastic individual regression.
Integrate LymphoSAT's exact bounded-width crossing-sequence RUP refutation for
StConn WV84 as a typed, structural, native-DSR action. Then continue through the
remaining SAT-owner rows while retaining the WV80/83/86 Tseitin design as a
staged native-DSR proof-system gap. Preserve every new checked solve and all
Phase-1 gates.
<!-- track-sched:end -->
