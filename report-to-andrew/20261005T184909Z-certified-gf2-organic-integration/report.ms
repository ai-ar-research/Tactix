.po 0.8i
.ll 6.9i
.ps 16
.ft B
.ce
Tactix certified GF(2) organic integration
.ce
Current evidence and corrected probe
.ps 11
.ft R
.ce
Development report for Andrew
.ce
October 5, 2026
.sp 2
.ft B
Disposition
.ft R
.sp
.ti 0.25i
The preservation baseline is healthy. Immutable source d7e48970d passes the
complete Debug campaign and all four standing regressions, with no wrong
verdict, checker rejection, or scorer failure. Subsequent opt-in GF(2) work
supplies a learner-selectable certified Boolean action, but its first target
experiment recovered no target: all three completed rows timed out before an
affine action was offered. This report makes no fixed-target, performance, or
portfolio-recovery claim.
.sp
.TS
tab(|) allbox;
l r r l.
Campaign|Disposition|Solver time|Independent disposition
Debug|292/292|820.39 s|244 executables; 340 immutable bindings
B28+C5|33/33|959.350 s|32 checked; one clean proof censor
Rank1--50|50/50|7,620.029 s|42 checked; eight clean proof censors
WV100|100/100|2,806.871 s|98 checked; two clean proof censors
WV2--89|89/89|4,029.436 s|78 checked; two clean censors; nine allowed timeouts
.TE
.sp
.ti 0.25i
B28+C5 is below its 999.87615-second ceiling and Rank is below 8,521.52175
seconds. Every proof censor is a clean expiration of the 1,800-second checker
allowance. The accepted jobs are Debug 1750129, B28+C5 1750135, Rank 1750168,
WV100 1750241, and WV2 1750302. They are the current preservation authority.
The controller and telemetry code changed afterward, so the corrected
checkpoint still owes fresh Debug and four-gate qualification.
.sp 2
.ft B
What changed after the accepted checkpoint
.ft R
.sp
.ti 0.15i
\(bu  a7cd75ba5 adds a supplemental fail-closed GF(2) target auditor. Missing or
capped telemetry cannot be scored as success.
.sp
.ti 0.15i
\(bu  1ea10b1b5 makes a completed empty/no-structure census terminal for its immutable
owner across search and restart epochs. Explicit resource/interruption retry is
preserved. Semantic advice is deferred until the configured propagation
follow-up finishes; fixed/retired variables are filtered and there is no hidden
fallback.
.sp
.ti 0.15i
\(bu  13eea1458 makes the named search-configuration subset learner-owned and
replay-fingerprinted. A probe can hold one configuration while exposing
AffineConsequence, explicit Propagate, and nonzero SemanticVariableAdvice.
.sp
.ti 0.15i
\(bu  fa0f0d5b6 records the initial target diagnosis and corrected contract in the
progress, calculus, interface, and implementation documents.
.sp
.ti 0.25i
Focused tests cover durable no-structure suppression, bounded paging, explicit
resource retry, post-propagation VMTF/EVSIDS advice, stale rejection,
deterministic replay, and fail-closed configuration. This establishes mechanism
behavior, not target recovery.
.sp 2
.ft B
Initial target evidence
.ft R
.sp
.ti 0.25i
Jobs 1750437 and 1750438 used d7e48970d, DSR, the gf2-affine producer, and the
original one-action profile. Their anonymous runtime validations passed.
.sp
.TS
tab(|) allbox;
l r r r r.
Row|Limit|Calls|Work|Candidates/offers
WV2-132 tseitin_d3_n158 UNSAT|600 s|397|4,789,262|0/0
WV100-7 two-trees-511v SAT|1,200 s|281|1,940,937|0/0
WV100-51 mdp-32-11-sat SAT|1,200 s|8|170,560|0/0
.TE
.sp
.ti 0.25i
All three returned UNKNOWN at the solver limit. No checker ran and no X9
request was made. The retained statistics establish calls and zero offers, but
not a complete action chain. The old format wrote a full cumulative observation
around millions of search-service actions, exceeded the 64 MiB cap, and deleted
the feedback file. The supplemental auditor correctly rejects every row.
Compact, lossless, versioned feedback is under redesign; observation must stay
pure and preserve the raw meanings needed by a future learner.
.bp
.ps 14
.ft B
.ce
Diagnosis and corrected probe
.ps 11
.ft R
.sp
.ti 0.25i
The causes are concrete. WV2-132 reaches image width 68 over 157 sources and has
130 selectable rows, beyond old 32/32 bounds. WV100-7 reaches width 62 over 160
sources and has 326 selectable rows. WV100-51 has 2,452 groups against the old
1,000-group cap and exposes 500 selectable rows after that cap is raised. The
old controller also sampled all 20 named search configurations. Durable census
and the search allowlist repair the two control defects.
.sp
.ti 0.25i
Fixed-rank job 1750439 is still running on ranks 263 and 569 under the old
profile. It contributes no completed evidence.
.sp 2
.ps 12
.ft B
Corrected four-row probe
.ps 11
.ft R
.sp
.ti 0.25i
The next immutable probe contains exactly WV2-132, WV100-7, WV100-51, and fixed
rank 263. It allows one named search configuration and AffineConsequence,
Propagate, and SemanticVariableAdvice. The bounded GF(2) profile is:
.sp
.nf
.ps 8
affine-consequence2(12000,30000,1000000,2500,10000,10000000,8,
68,160,4,1000000,1000000,10000000,10000000,68,160,10000,10000,
3,100000,10000,1000000)
.ps 11
.fi
.sp
.ti 0.25i
This raises maximum_groups to 2,500 and image/basis width/source limits to
68/160 while retaining finite proof, journal, extent, pattern, and work bounds.
Rank 263 has 1,899 groups and 1,328 selectable rows.
.sp
.ti 0.25i
Exclude rank 569. Its census has 10,158 groups and 10,230 patterns, but only 24
independent one-source width-three equations and no selectable consequence.
Raising the profile would add census cost without exercising X9. The running
old job does not change this unless it produces contradictory evidence.
.sp
.ti 0.25i
Every corrected row must retain complete telemetry: producer status/effort,
candidate, offer, request and application counts, proof work, extent, yield,
and terminal state. Credit requires an independently checked model or proof.
Timeout, checker failure, missing sidecar, cap loss, or inconsistent terminal
records remain fail-closed diagnostic outcomes.
.sp 2
.ft B
Soundness, scheduling, and learning readiness
.ft R
.sp
.ti 0.25i
The integration boundary remains organic: GF(2) reasoning authenticates theory
structure and offers a typed Boolean consequence or conservative extension to
continuing CDCL. It is neither a private local solver nor a sequential
portfolio. DSR remains the default proof format.
.sp
.ti 0.25i
For N1, the exact affine row is an ordinary public action; search configuration,
propagation, and advice are explicit shared-controller choices. For N2, target
credit requires an independently checked reconstructed model or proof, with
extension projection and provenance. For N3, the accepted default is
competitive, but post-fix source must repeat all four gates. For N4, order,
effort, retry status, search configuration, and advice are typed and observable;
the scheduling ledger and final policy acceptance remain pending.
.sp
.ti 0.25i
Default, random, and future learned policies share action identities and the
execution path. Pure observations and after-action raw feedback expose effort
and result without rollback or hidden fallback. Next: finish lossless telemetry;
freeze and validate the corrected four rows; require checked outcomes; then run
effective both-policy fuzz, fresh Debug, and all four regressions. Expand to
ranks 540/896/1004 and MAB-only 557 only after coherent checked evidence. The
saved d170b4a5 comparison remains authoritative and is not rerun.
