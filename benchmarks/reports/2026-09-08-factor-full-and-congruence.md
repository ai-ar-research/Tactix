# Factor runtime follow-up and current-source Congruence probes

The objective remains all Rank1–40 aggregate runtime at most1.01x Kissat through
general implementation repairs, with checked B28+C5 gates. Production is unchanged.
The preceding Factor support repair and focused correctness/causal evidence are
frozen in [its report](2026-09-08-factor-root-support-repair.md) and local commit
`b15b114e`. No source is promoted; no push is authorized or attempted.

## Runtime evidence and next decision

The full unchanged80-task campaign5604/job1672600 compares candidate5592
(Release SHA12b3731294d2fd6e8a273fb4b808b2444e8ccd6dc4cff9f771622a476e1ad340)
with clean Kissat (SHA3aa8e41f976e213d3c4627150e9cd4e5fce0ce9508d67ab469a7696e26a8d49c).
The complete solver snapshot at10:03:09UTC has all40 pairs:7671.990/6744.081
seconds = **1.1375886499583854x Kissat**,40 solves each (22SAT/18UNSAT),zero
timeouts,58/80 checked/clean. This is not yet a final checked score or promotion.
Maximum RSS is15967668/15348304KiB; memory remains a separate acceptance dimension.
Helper5608 froze that snapshot once;5615 validates each bound record and ranks
current losses;5609 seals only after all80 independent checks and cleanup pass.
The preceding5522 view is now80/80 checked at1.1621795918x; different measurement
windows prevent attributing the entire ratio difference solely to this repair.

The full scoring view establishes the leading diagnostic priorities:

| Rank | Current Tactix seconds | Kissat seconds | Excess seconds |
|---|---:|---:|---:|
|31|412.682|262.625|150.057|
|32|320.163|272.044|48.119|
|33|474.433|309.468|164.965|
|37|465.333|344.817|120.516|
|35|409.498|304.922|104.576|

These are same-campaign pairs with expected verdicts; pending UNSAT certificates
are provisional passes only. The selected Rank33 remains a decision-relevant
loss on the actual current implementation. It produces the same proof bytes as
parent5522, but not every non-timing statistic matches, so no exact-work claim
is inferred from proof identity alone. Rank39 retains exact parent statistics
and proof bytes. Parent timing came from another window and cannot establish
the isolated effect of the repair.

Three timing pairs were predeclared in5599. Wave2/job1672681 was submitted after
wave1's expected solver records and exact within-arm statistics/proof identity,
without waiting for checkers. Wave0 is323.718/389.784=0.830506x parent5522;
The complete three-pair checkpoint gives ratios0.830506/0.824875/0.821560,
median0.8248752848, with exact within-arm statistics/proof identity and spreads
1.6046%/0.5220%, passing the predeclared2% precision criterion. This is replicated
net runtime improvement, not isolated unit cost; all six proof checks were
pending at the snapshot. B28+C5 5598 is64/66 checked at this checkpoint, all B28
complete, with two C5 proof checks pending. Separate Vivify5550 is now66/66
checked/clean, combined0.927432x production; its noisy timing5551 still provides
no reliable standalone speedup and it is not incorporated into5592.

## Next general implementation hypothesis

The previous5522 Congruence witness5582/5583 identified five retained ternaries
after the first Congruence9457 output. New current-source witness5611/job1672685
rebinds that exact input and tested observer to5592. Validator5612 requires two
clean100K-conflict stops, all trace/pin hashes, full native parent replay and
whole current candidate/native decision comparison. It records, rather than
assumes, old candidate artifact identity. No later decision causality is claimed.

Static inspection finds a discriminating hypothesis: Kissat's
`init_xor_gate_extraction` and CaDiCaL's corresponding initializer garbage
root-satisfied clauses they encounter before exceeding the effective-size cap.
Tactix's `build_clause_index` merely skips those clauses in its read-only index.
Forward cleanup later runs only after a merge; it need not erase this earlier
lifecycle distinction in a no-merge call. This is a potential general mechanism
gap, not yet a demonstrated cause of the five clauses or the full runtime loss.
No Congruence implementation or guard has been changed.

Witness5613/job1672687 uses the same immutable pins and existing observer options
to capture physical pose, root values and garbage state for clauses containing
external variables236195/236197. Validator5614 requires whole5611 replay per
arm, including binary snapshots and partial-proof bytes; only the selected
formula format differs. A missing native clause alone does not identify its
deletion stage: garbage may already have been collected.

## Tests and background correctness

Every change received a unit-test consideration. No solver code changed after
the frozen5592 repair. Existing19 Factor and8 Congruence parser fixtures cover
the current-source rebinding. New5614 adds six positive and ten negative
clause-state fixtures for pose permutation, ordered value mapping, legal values,
garbage flags, duplicate preservation and malformed input rejection; all pass.
The reused full-score counter parser and explicit80-record/hash assertions cover
5615's reporting-only binding; no new solver unit test is needed for that helper.
Main Release CTest passes77/77 in12.10s. Frozen patch/log/matrix artifacts retain
their exact original whitespace; the hand-edited scope passes diff whitespace
checks. Living docs remain within their existing budgets.

Claude's terminal5539 report is authoritative **PASS on5522**:70,000 generated
cases,64,364 checked verdicts,37,692 original-CNF models,26,672 independent
proofs,5,636 recorded timeouts and zero failures. It also reports420 corpus
replays and267 direct reachability cases. Session70659 is closed. Parent read
only the terminal response, not fuzz jobs/artifacts, and did not recompute it.
Claude reports campaign-owned cleanup; parent manually deleted nothing.

New changed-source checkpoint5610 is owned by live Claude CLI session78639.
It covers5592, unlike5539, and must demonstrate root-fixed support exclusion
and actual successful Factor rewrites under independent model/DRAT/LRAT,
sanitizer, random-policy and stress checks. Generated legs retain100 logical
CPUs/400000MiB/50-worker contracts and immutable harness snapshots. Parent only
polls this CLI handle; no passing result is inferred. A genuine correctness
failure stops dependent work and invokes the bounded repair workflow.

## Handoff

All40 scoring and timing replication are frozen; inspect5611 then5613 with their
strict validators before choosing a Congruence change. Keep proof checking and
Claude-owned fuzzing asynchronous. Audit5616 records exact normal job/result
states, documentation checks, main test evidence and the Andrew TeX/PDF hashes.
No manual removal, production promotion or push occurred.
