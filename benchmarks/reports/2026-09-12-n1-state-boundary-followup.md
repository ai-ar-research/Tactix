# N1 state-boundary follow-up — 2026-09-12

The final source audit after the refactor gate checkpoint found one remaining
implementation restriction: Restart and Reduce had stronger guards than their
abstract calculus preconditions. This follow-up closes D-NOOP without changing
the shipped DefaultPolicy schedule.

Restart is now applicable in every consistent state. Level zero is an explicit
no-op. When a caller restarts from a recorded conflict, the conflict remains
available only if the retained trail prefix still falsifies its antecedent;
otherwise the stale record is cleared. If a full Restart leaves the antecedent
falsified entirely by chronological root implications, Restart discharges the
root refutation through the native analysis proof path before any root rewrite
can retire that antecedent. Reduce is also applicable with a recorded conflict:
it locks a large conflict antecedent against selection and
forwards its `Ref` through arena collection, alongside existing protection for
pending learned clauses and non-root trail reasons.

DefaultPolicy still schedules Restart only above root, including its rephase and
inprocessing preparation helpers. Its measured B28+C5 and Rank 1–50 paths are
therefore unchanged by this follow-up. Focused C/C++ tests cover root Restart,
binary-conflict prefix retention/abandonment, public Reduce/Restart composition,
and a selected Reduce deletion that packs the arena while retaining a large
conflict for ConflictAnalysis. The current Release binary and committed
`7a668e76` pin also produced identical exits, stdout, stderr and proof bytes on
all 30 retained fuzz-regression formulas under DefaultPolicy.

Claude Code's [first bounded checkpoint](2026-09-12-claude-d-noop-checkpoint.md)
passed 10/10 focused Release/sanitizer tests but blocked on a code-review edge
case: chronological root implications
could leave a conflict wholly falsified after Restart reached level zero, where
a later root rewrite might retire its antecedent. The repair above makes that
state terminal through the existing native analysis path. A focused regression
now drives the exact state. `micro_proofs.sh` verifies its native LRAT empty
clause against the matching original CNF with formally verified `cake_lpr`, and
requires rejection of an invalid-hint negative control. Claude's
[second bounded checkpoint](2026-09-12-claude-d-noop-recheck.md) independently
traced the state and proof paths, passed 12/12 focused Release/sanitizer checks,
verified the proof with `cake_lpr`, and returned **PASS** with no correctness,
N1, N2 or DefaultPolicy scheduling blocker. Its low-severity proof-oracle
observation was closed by the stronger checker and negative control above.

At this checkpoint, Release passes 146/146 tests and ASan+UBSan passes 145/145.
B28+C5 remains 33/33 valid at
962.588 seconds versus 952.088 seconds retained (1.011028x), with summed peak
RSS 0.991110x. Rank 1–50 has 48/50 valid certificates (25 corrected SAT models
and 23 UNSAT proofs) at 8,327.103 seconds versus 8,126.049 seconds retained
(1.024742x), with summed peak RSS 0.998745x. Proof checks for Rank tasks 17 and
21 continue independently in jobs 1683798 and 1683802.

Tier-D array 1683876 remains unchanged on committed refactor `7a668e76`: 1,166
Tactix-only tasks on `cpu-q`, four CPUs, 32 GiB, and a 1,200-second solver
limit. The runner holds formula, proof, model and checker intermediates in
anonymous node-local `O_TMPFILE` objects; validation job 1683875 covered SAT,
UNSAT, forced crash and forced timeout cleanup. This follow-up does not restart
or replace that in-flight array.
