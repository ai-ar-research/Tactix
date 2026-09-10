# Collection retry advice: a concrete Rank35 scheduling repair experiment

Isolated7030 suppresses BVE/BCE/score removal advice only when sparse root
collection retires a satisfied large clause. This follows Kissat's collector;
it is not a blanket deletion change, a default feature disable, or an accepted
runtime improvement. Production and current6730 remain unchanged. Current's
full50 checked score is still8300.164/7239.010=1.146588277x fixed Kissat.

## Evidence and reference distinction

[7026](../runs/20260910-7024-rank35-retry-producer/replay-summary-7026.json)
validates exact complete own7018 schedules, decision/boundary/macro traces and
partial-proof bytes/hash before interpreting the selected journals.
[7032](../runs/20260910-7024-rank35-retry-producer/producer-localization-7032.json)
then validates all15 prior attempts of external174970. Portable effects,
absolute effort and limits agree; each final retry flag is clear. After the
last shared attempt at271520, current alone marks174970 at276460/D73339360,
deleting clause174993,174970,-174997,116807,115633. The mark lies between
Reduce begin/end at that exact conflict/decision count. Native has no later mark.
This is one representative of the21 extra initial BVE members, not proof every
extra has the same origin or that all excess runtime has this cause.

Kissat collect.c:sparse_sweep_garbage_clauses retires a satisfied large record
by updating counts/proof without mark_removed_literals. Its binary watch flush
uses kissat_delete_binary, which does mark removal. Vivify's satisfied-clause
path uses kissat_mark_clause_as_garbage, which also marks removal. CaDiCaL's
collect.cpp instead uses mark_garbage and mark_removed for satisfied clauses;
this is a real lineage difference, not falsely claimed reference unanimity.
Kissat is the required tie-break for this scheduling experiment.

Current6730's shared kernel_flush_root_clause always used semantic_removal=true.
The existing kernel_delete_clause semantic_removal switch already separates
proof/formula retirement from retry/BCE/score notification.7030 adds a trailing
argument to the root-flush helper, defaulting to existing semantic behavior;
only sparse collection passes false. Root-false rewrites, binary retirement,
Vivify, clause/proof deletion, model reconstruction and the6730 historical
Reduce boundary remain unchanged. No new kernel primitive, layout field,
guard, public instance restriction, scheduling allowance or tuned constant.

## Validation and gate

[7030 build manifest](../runs/20260910-7030-collect-retry-build/manifest.json)
binds the entire isolated source, incremental/cumulative patches and independent
Release/BC/trace pins. Release/BC SHA is
`4029d621784c7ca12ce8ee575fe7fc6f0bd68d990b9290fd36866297c16fc00b`;
trace SHA is`d587cc7bd6126007b3d05507fce6d850db0163281e1a9ba867af19012b354775`.
All113/113/112 unit suites pass. New40-state tests cover redundancy, every root
position, preexisting marks, default/Vivify versus collection deletion, BVE
event counts, BCE stamps, score-hook notifications and repeated collection.
The actual parent reproduces ten expected bookkeeping differences without
crashing; a binary retirement control remains marked. Existing root-false,
watermark, public BVE and root/reason tests retain their distinct coverage.
The new producer parser has positive/ten-negative framing/schema/order fixtures;
the reused frozen candidate reader also passes its positive/ten-negative fixtures.

[7033](../runs/20260910-7033-collect-retry-proofs/summary.json) verifies16 fixed
parent/candidate collector UNSAT proofs, DRAT and strict cake_lpr, with exact
cross-arm certificates. Two known-invalid SAT proofs are rejected. Existing
fixed model/root/nonroot/pending-reason controls also pass. Legacy lrat-check
is deliberately not an oracle. Only this new micro-campaign's3998 bytes of
generated inputs/proofs were removed after verification; evidence is retained.

[7034](../runs/20260910-7034-collect-retry-BC-gate/manifest.json),job1679667
since2026-09-10T08:58:11EDT, is the unchanged full B28+C5 paired guard against
current6730.66 tasks,12 logical CPUs,48000MiB,1200 solver-only seconds,
16000 checker seconds,5hours, full width with no node constraint/throttle.
Old pins, runners, cpu-q campaign and fixed denominator are hash-protected.
7036 classifies all outcomes; changed search work is intended, not a failed
neutrality test. Expected outcomes permit investigation while checks run;
pending checks never close a gate. This remains unpromoted/unfuzzed material.

## Next decision

After the B/C solver gate, compare the candidate's bounded Rank35 prefix and
BVE frontier against cached native evidence. Preserve an earlier divergence
if the general change creates one; do not demand only the selected phase move.
Then three independent paired31/35 timing waves and cumulative guards decide
whether the mechanism actually reduces worse work. Matching a longer native
prefix is not acceptance: rejected5920 remains a counterexample. A supported
repair still needs full50 impact and the required consolidation/fuzz gate.
Keep small ITE full50 deferred, Rank21 shelved, and established4/32/33/49/46
closures intact. No promotion, fuzz access, FlatLists redesign or denied
push/transfer retry. The successor audit owns later asynchronous check states.
