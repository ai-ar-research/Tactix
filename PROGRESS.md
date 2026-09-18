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
1.319s; both original-CNF models pass and cleanup is clean. AM171 job 1702894
adds WV85 in 28.115s after 941,695,142 numeric key candidates; its 8,896-clause
original-CNF model passes. AM174 job 1702975 adds WV84: typed Lucky15 recognizes
the exact width-7, 128-column st-connectivity family and emits 7,861,711 native
RUP clauses in 9.231s and 13.5 MiB solver RSS. The 267.5 MB DSR certificate
passes dsr-trim and patched Trestle in 204.548s; cleanup is clean. Independently
checked coverage was 90/100 at that checkpoint.

The general AE-kissat-MAB successor adds simultaneous EVSIDS and CHB stable
heaps, the owner's age-weighted CHB updates, restart reward/momentum/adaptive
UCB selection, root restart on arm switches, and selected-heap TransitiveReduce
ordering. AM177 independently checks WV87 SAT in 1.620s and WV97 SAT in
179.719s. AM179 job 1703169 independently checks WV96 SAT in 1,197.329s; the
boundary-only predecessor times out, establishing selected-heap inprocessor
coupling as causal. Coverage is now 93/100. Open ranks are 1, 25, 26, 80, 83,
86 and 95.

SC2024's post-Probe fast variable elimination was tested as canonical BVE with
the owner's four-round schedule and admission bounds. Corrected BVE-only AM185
times out on WV95 both alone and with MAB, and MAB+fastel also times out on
WV96. The rejected variant has been removed rather than retained as another
strategy branch.

The native parity successor recognizes complete arity-2--4 XOR systems with
even incidence and odd right-hand-side sum. Its eager recognizer allocation is
now bounded by actual input size. WV80 checks, and AM197 independently verifies
WV83: the 65.364s solve emits a 1.741 GB DSR certificate that passes dsr-trim
and canonical Trestle; WV86 remains in checking. The
typed Lucky7 successor now recognizes complete bipartite exact-one incidence
systems and emits a modulo-three ER refutation when the color classes differ.
Default-policy job AM194 independently checks WV25/26 in
12.486/17.645s at 6 MiB solver RSS; both native DSR certificates pass dsr-trim
and Trestle. AM195's canonical-Trestle replication verifies WV25 in 814.133s
total and WV26 in 1,636.742s total. No filename, rank,
family, size-class dispatch, external solver stage, or sequential portfolio is
present. The remaining open owner gaps are WV1's SGEN counting proof and WV95.

AM189's independent parity fuzz campaign completed 10,000-case default and
random-policy quick legs with zero failures and found two bounded gating bugs.
The candidate-side eager-reservation defect is fixed in `2e429368`. The pinned
dsr-trim RAT-candidate bitmask defect is repaired by the repository checker
patch: capacity now grows past `mask_index`, and setup runs the minimized
24-clause certificate through dsr-trim and canonical Trestle. The owner found
zero canonical/optimized Trestle divergences across 31,949 streams. Its
remaining R1 finding is an N4 scheduling gap: structured-root SR can preempt
the parity refutation on a 3,000-variable residual.

The cumulative StConn successor Release suite passes all 168 tests. AM173's
Claude-owned proof/scheduling fuzz completed both Release and ASan campaigns.
It exposed invalid harness expectations for non-refuting proof prefixes and a
normalization-preserving mutant, plus one unrelated random-policy certificate
failure; these findings still require owner adjudication before StConn is
accepted. Complete matched AM160 successor gates have solver results for every
case: Rank 1--50 is 50/50 in 8,007.968s and B28+C5 is 33/33 in 895.121s. Four
long Trestle tasks remain active before those successor gates are final.

The user-authorized frozen AM149 full Tier-D campaign is job 1702332. Its
750 GB pending-task guard and the stricter global 700 GiB guard are live;
running tasks are never held or interrupted.

## Exact next action

Finish the WV86 parity check and the parity R1 scheduling repair, then fuzz the
exact-one rule. AM217 measures whether WV95's remaining gap is downstream
random-trajectory sensitivity after matching SC2024's Factor/BVE footprint;
its result may motivate a general adaptive diversification policy, but cannot
promote a fixed winning seed or a portfolio. Keep WV1's signed-cardinality
counting proof staged unless it gains a native DSR formulation. Preserve every
checked solve and both Phase-1 aggregate gates.
<!-- track-sched:end -->
