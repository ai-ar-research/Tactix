# Rank31: validated late BVE localization

The largest current runtime deficit remains Rank31:534.277 seconds versus
fixed Kissat264.532, with10993050 versus4540757 conflicts. The source-backed
collection-retry repair7030 remains isolated and unpromoted. Its replicated
Rank31 regression is real; reversing it alone would not close the whole native
gap. Keep the general repair and investigate its coupled mechanism.

## Completed full replay

[7066](../runs/20260910-7064-rank31-full-boundaries/solver-summary-7066.json)
validates both full expected UNSAT outcomes, exact complete own Release
non-timing statistics and certificate bytes/hash, and every byte of the cached
two-million-conflict macro/inprocessing prefix. Both diagnostic solvers finished
within1200 seconds. Independent proof checks remain asynchronous; trace-build
times are not acceptance timings.

The first differing raw inprocessing row is BVE end at conflict5187498,
decision211943881. Every preceding recorded boundary matches, including the
immediately preceding Strengthen end/BVE begin. Candidate and predecessor then
have51371/51370 active variables and313594/313612 irredundant clauses, with
equal recorded propagations, binaries, redundant clauses and search ticks.
The first macro difference is the same elimination phase's end, with51160/51159
unassigned variables after subsequent passes. These coarse observations do not
prove initial formula identity, identify the first decision split, or establish
which resulting formula is stronger. Fewer variables or clauses alone is not a
strength verdict.

## Selected next experiment

[7068](../runs/20260910-7068-rank31-selected-bve/manifest.json),job1679815
submitted2026-09-10T10:28:47EDT, reuses unchanged7030/6730 trace binaries.
It adds only candidate begin/end/postflush observations at conflict5187498 to
the already validated full macro/boundary run. Candidate identity, retry and
retirement flags, roots, effort ticks/limits and application outcomes can
distinguish an extra attempt, changed ordering, different allowance or changed
effect. No global event trace, heap observer, decision stream or forced exit.
Both arms run independently at full width with12 logical CPUs,48000MiB,
1200 solver-only seconds and the existing16000-second proof-check budget.
No Kissat rerun or change to an existing campaign is involved.

Validator7070 requires exact whole own Release work/certificate and
byte-identical complete7064 macro/boundary files before interpreting attempts.
It reuses6192's candidate framing/grouping fixtures and7042's work fingerprint.
Only the new campaign receives the updated solver-environment wrapper: the
sole additional allowed SAT control selects the BVE conflict, validates its
unsigned64-bit value, and is restored before checkers or on exceptions.
Four new tests cover this behavior and reject unrelated/stop controls.
All277 benchmark-harness and22 SLURM tests pass. Solver source and observers
are unchanged, so no duplicate solver correctness/fuzz campaign is warranted
for this diagnostic-only increment.

At this checkpoint all12 repeated Rank31/35 timing certificates are now
checked/clean; B28+C5 remains66/66 checked/clean. Full50 has42 checked/clean
and8 proofs pending. Repaired Rank35's profile and both7064 proofs are pending.
The successor7071 audit records exact scheduler/pin/result authority and
cleanup states. No manual material deletion, fuzz access, promotion, FlatLists
redesign or denied push/transfer retry. Production remainsc2a95a6.

## Priority and limits

Validate7070 and inspect the first changed attempt before selecting a repair.
Retain Rank35 as the residual implementation-cost control; its27 exact major
native counters support that role, not a claim of identical full state or
timer scope. Rank43 is the next short UNSAT gap (124.565 versus46.089 seconds,
34848 versus10588 conflicts), followed by other large unshelved gaps. Do not
repeat rejected copy/reconnection tweaks merely because their functions are
hot. Rank21 stays shelved and established4/32/33/49/46 closures remain protected.
The provisional full50 ratio remains1.123176512; performance and memory goals
are unmet. Existing documentation-size debt is reported, not automatically
compacted.
