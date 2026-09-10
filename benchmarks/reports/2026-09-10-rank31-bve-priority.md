# Rank31: a lost elimination with ample budget

[7070](../runs/20260910-7068-rank31-selected-bve/solver-summary-7070.json)
passes full observer neutrality: both expected UNSAT solves reproduce their
own complete Release work/certificate and every byte of7064's full macro and
inprocessing boundaries. Candidate/previous diagnostic times are938.643/861.200
seconds, below1200; trace overhead is not an acceptance timing. Independent
proof checks continue asynchronously.

[7072](../runs/20260910-7068-rank31-selected-bve/effect-summary-7072.json)
narrows the first differing BVE phase at conflict5187498:

| Selected-phase measure | Candidate7030 | Previous6730 |
|---|---:|---:|
| Attempt count |37925|37938|
| Eliminated variables |1584|1585|
| Entry ticks |94820657|94820657|
| Final ticks |98891322|98892566|
| Effort limit |192068775|192068775|
| Attempt index for355342 (zero-based) |14433|6347|
| Outcome for355342 |declined|eliminated|

Variable355342 is the only difference between the two eliminated-variable
sets. Both arms attempt it once with retry enabled, but at different formula
states. This is a concrete lost-elimination witness, not proof that it alone
causes the full runtime regression or that one resulting formula is stronger.
The phase ends far below its ceiling, so increasing the budget does not address
this observation.

The first11 complete attempts match every recorded field. Attempt11 swaps
497345/497326; each arm processes the other next with the same individual
outcome and66 combined ticks. Those facts do not establish commutativity or
clause-pose identity. The predecessor also makes13 extra attempts over nine
variables, including three extra355344 attempts. Preserve those differences
rather than inferring a simple omitted retry from shifted row numbers.

## Next discriminator

[7073](../runs/20260910-7073-rank31-bve-priority/manifest.json),job1679819
since2026-09-10T10:48:39EDT, tracks four selected heap subjects:
355342,355344,497326,497345. The unchanged trace binaries record initial score,
position, occurrence counts, heap root/size and later producer-triggered
movement at5187498. This distinguishes an incoming priority/topology difference
from a later update; it does not reconstruct the whole physical schedule.

No solver, strategy constant, maintained harness file or existing campaign pin
changes. A private copy of the tested solver-environment wrapper additionally
allows only the read-only heap conflict selector. Its focused fixtures check
solver/checker separation, restoration on success/failure, seven malformed
values and three forbidden controls. Reused6196 heap fixtures pass; added
four-subject/two-round controls reject incomplete and reordered initial rows.
No new solver unit tests or duplicate fuzz campaign are needed for this
observer-only increment.277 benchmark and22 SLURM tests remain required.

Validator7075 must reproduce complete own Release statistics/certificate and
every byte of7068's macro, boundary and candidate traces before interpreting
the heap history. Both tasks use12 logical CPUs/48000MiB,1200 solver-only
seconds,16000 checker seconds and5h allocation, full width/no node constraint.
Normal proof checks and cleanup remain asynchronous. No native rerun or global
event trace is enabled; no heap tie or retry selector becomes solver policy.

Rank31 remains the largest current deficit. Rank35 remains the residual-cost
control; Rank43 is the next shorter UNSAT case (five inprocessing phases,
34848 versus10588 conflicts). Keep the supported general collection repair,
Rank21 shelved, established4/32/33/49/46 closures and N1--N4. Full50 remains
provisionally1.123176512x; runtime and memory goals are unmet. At this checkpoint
all12 repeated timing proofs and66 B/C outcomes are checked/clean; full50 has
48 checked/2 pending.7076 owns later scheduler/result/cleanup observations.
No manual material deletion, fuzz access, denied push/transfer retry or source
promotion. Existing doc-size debt is reported, not automatically compacted.
