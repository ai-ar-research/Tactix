# Correct occurrence validated:32 displaced heap entries

Repeated-pop5988/job1675945 completed normally and cleaned owned scratch.
[Strict validation5987](../runs/20260908-5988-bve-repeat-heap/summary.json) proves
complete ordinary300K trace, partial proof and BVE candidate-journal replay per
arm. It captures the initial heap and all four physical pops of462198 at the
selected conflict; the unique8966-member witness is occurrence2, not1.

| Image | Tactix/native members | Shared-field differences |
|---|---:|---|
| Initial |88946/88946|None|
| First pop |62185/62185|None|
| Second pop, motivating witness |8966/8966|32 positions only|
| Third pop |7447/7447|78 positions only|
| Fourth pop |21130/21127|7980 positions,92 scores,76 positive and49 negative occurrence counts|

The first three pop images have no membership, assignment, retired or retry
differences. At the second pop, the first different *chosen* sift child is at
index278: Tactix selects335015, native477747. Their right children are244396
versus477747. Earlier atindex16 the right children differ but both select the
same left child; do not confuse first different input with first different
choice. The reconstruction places60705 at8930/8954, exactly matching5972.

The later fourth-pop membership difference is three candidate-only members:
426240,426249,488999. It is not the earliest topology divergence. No score or
tie-comparison change is justified by these captures; both algorithms operate
on different earlier subtrees.

New5992/job1675996 reuses validated5949 binaries and traces225614,60705,477747,
244396,5409,3343,3531,242204. All eight identities are declared in the new
manifest before submission. Comparator5993 requires complete ordinary/proof/BVE
journal replay and two initial images per subject before any interpretation.
No solver implementation or timing denominator change; no duplicate fuzz work.

The preceding [full-runtime report](2026-09-08-bve-runtime-and-repeated-pop.md)
remains the outcome authority:5962 is1.116453× fixed Kissat, not a gain. Latest
[scoped audit](2026-09-08-bve-runtime-validation.json) has31/40 full candidate
certificates checked,64/66 BC; checkers remain asynchronous. Cpu-q5784 has1087
checked-valid results,598 solver timeouts, two checker timeouts(384 and native451)
and the four known status mismatches. All six unverified/status cases remain
invalid/unscored, with original runners and pinned inputs unchanged.

Next: validate5992, align repeated trigger occurrences using actual state and
trace the first displaced subject to the earlier update/push. Current5962 is
unfuzzed investigation material; no promotion, push or goal completion.
