# X17 offerer checkpoint: terminal default-policy regressions

The four authorized default-policy regression arrays for source `7dcd36101`
and Release binary SHA-256
`c652bed6a98e07d4362146240ccd161c4082065def7c50ccbcc8472d542ae05c`
are terminal.  They use the frozen matrices, original solver limits, one
1,800-second deadline for each complete proof-checking pipeline, and the
baseline plus `max(30%, 20s)` completed-proof cost rule.  No baseline solver or
saved `d170b4a5` portfolio comparison was rerun.

| Set / job | Solver verdicts | Independently checked | Checker censor | Solver timeout | Solver seconds | Score |
|---|---:|---:|---:|---:|---:|---|
| B28+C5 / `1748622` | 33/33 | 32 | 1 | 0 | 980.299 | accepted |
| Rank1--50 / `1748655` | 50/50 | 42 | 8 | 0 | 7,718.832 | **rejected** |
| WV100 / `1748669` | 100/100 | 98 | 2 | 0 | 2,785.492 | accepted |
| WV2--89 / `1748673` | 79/89 | 77 | 2 | 10 | 4,086.751 | accepted |

All 125 SAT verdicts have original-CNF model checks, and every completed UNSAT
pipeline verifies.  The thirteen clean proof censors are incomplete checking,
not checker rejection.  WV2 retains exactly 48 SAT, 31 UNSAT and ten
600-second solver timeouts.  Its two censored UNSAT certificates are unchanged:
`bphp_p23_h22` is 1,400,232,109 bytes with SHA-256
`efe0cf3f8b59d57108dd10afc7e323e32e1022b393d2dea37d47d7750143788d`,
and `linked_list_swap_contents_safety_unwind76` is 1,326,093,180 bytes with
SHA-256
`7b4070e30f9d244b43665150737e3675df47260954fd5cdf190432c7aae75363`.
The older extended-check AM549 run verified those exact streams in 2,141.116
and 2,626.569 seconds; the current 1,800-second censors therefore show the
declared bounded protocol, not a new proof regression.

B28+C5 remains below its 999.87615-second aggregate ceiling.  Rank remains
below its 8,521.52175-second aggregate ceiling, but task 44,
`oddball_19_4_ttf.normalised`, fails the proof-cost requirement.  The frozen
accepted proof is 300,450,048 bytes and checks in 234.253 seconds.  The current
proof is 380,374,478 bytes and checks in 319.565 seconds, above the 304.5289
allowance.  It is a genuine changed search/proof trajectory: conflicts rise
from 3,221,300 to 4,091,615 and the certificate hash changes.

The existing [Walk regression diagnosis](2026-10-03-native-ramsey-and-walk-regression.md)
isolates the trigger.  Importing partial-model phases into Walk consumes part
of the fixed action effort, leaving fewer flips and changing the CDCL
trajectory.  A diagnostic that retained ordinary gap filling and restored the
pre-import flip allowance reproduced the exact compact baseline proof.  That
diagnostic spends additional effort and therefore is not itself an accepted
production repair.  The open engineering task is an explicit fixed-effort
allocation that preserves useful import without silently expanding the action
budget, followed by the normal regression gates.

These arrays bind `7dcd36101`, before the subsequent public C v8/replay/feedback
surface was committed.  X17 remains disabled in DefaultPolicy, and focused
tests establish that the new public surface does not schedule it implicitly;
nevertheless this report does not relabel the arrays as exact-source evidence
for a later commit.  Full current-source acceptance, effective Claude-owned
fuzz, checked target recovery and the all-44 objective remain open.
