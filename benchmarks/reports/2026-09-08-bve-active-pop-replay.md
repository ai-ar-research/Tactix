# Active-pop real-instance closure

Candidate5962 Rank32 diagnostic5969/job1675800 completed normally and cleaned its
owned scratch. [Complete attempt validation](../runs/20260908-5969-bve-active-pop-rank32-prefix/active-pop-validation.json)
removes exactly nine inactive applications:168851 new attempts versus168860 old,
with no inactive new attempt. Every remaining attempt agrees with the labelled
parent active projection on identity, begin/end outcome, assignment and absolute
ticks/limit. This closes the motivating policy defect, not the heap-order gap.

All four ordinary300K traces and the partial proof are byte-identical to5900.
The first native attempt difference remains index120872:225614 versus225641,
both declining at43990691 ticks under65731490. [Complete decision comparison](../runs/20260908-5969-bve-active-pop-rank32-prefix/summary.json)
still matches68091135 native decisions before conflict271085,decision67954350,
level75,where Tactix selects−29407 and native−29066. Early/suffix overlap is
verified, not double counted. Full Rank1–40 job1675801 continues; no new runtime
gain follows from this bounded identity.

The [normal campaign audit](2026-09-08-bve-active-pop-validation.json) closes
parent5900 full40 and BC66, and rejected5920 BC66. New5962 BC remains64/66
checked at the audit (all66 expected solves), with B0.743989× and C0.931615×
production. Full5970 had27 provisional and24 checked results at that checkpoint;
this partial view is not scored. No new correctness rejection was observed.

The untouched cpu-q5784 campaign had1033 checked-valid results,565 solver
timeouts, the four known status mismatches and one new
[checker timeout](2026-09-08-checker-timeout-384.json). Task384 returned expected
UNSAT in850.606s; its checker exhausted16000.004s. It remains invalid/unsolved,
not a proved rejected derivation and not an empty-input status case. The new
audit classifier has eight negative controls; no runner or acceptance change.

Next diagnostic remains5978/job1675845: full heaps before earlier common pop
462198, following5972's complete replay and60705 placement8930/8954. No source
promotion or fuzz claim. Pending checks continue asynchronously.
