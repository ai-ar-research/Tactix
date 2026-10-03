# Lost WV100 solve: matched diagnosis and bounded recovery

The first matched pair on `snw_16_9_CCSpre` completes with independently
verified SAT models for both pins. Repaired source `375b621b` takes 1,035.187
seconds versus predecessor `f6f01982` at 1,046.445 seconds. Complete normalized
printed work and certificate hashes are equal. **This pair does not reproduce
a candidate slowdown; the original WV100 gate remains NOT ACCEPTED.** Its
lost retained solve and complete gate accounting remain in the
[terminal-gate report](2026-10-03-terminal-substitute-regression-loss.md).

## Matched evidence and limits

SLURM `1747087` is COMPLETED, exit `0:0`, elapsed 34:50. The sequential
predecessor/current runs use the same 12-logical-CPU/32-GiB allocation on
`node017`, identical affinity and solver CPU, the immutable input and original
1,200-second solver cap. Both original-formula model checks return verified,
exit zero, without timeout. The log records twenty pin checks before and after;
all twenty still verify. The
[validation manifest](2026-10-03-snw16-matched-retention-diagnosis/validation.json)
binds raw rows, complete analysis, launch files and terminal metadata.

| Measurement | Predecessor | Candidate |
|---|---:|---:|
| Solver wall seconds | 1,046.445 | 1,035.187 |
| Inprocessing slot seconds | 253.048 | 250.897 |
| Search/scaffolding seconds | 793.018 | 783.945 |
| Vivify seconds | 147.682 | 145.508 |
| Instructions | 5,178,110,235,247 | 5,178,103,124,802 |
| Cycles | 3,974,276,563,543 | 3,933,400,938,042 |
| Page faults | 432,795 | 432,758 |

Both record 3,936,628 decisions, 1,220,557 conflicts, 7,077,999,954
propagations, 35 inprocessing phases and 39,296 restarts. Certificate SHA256
is `a9704ff6d3381a8b3a85d5ab67146719aeffe42c263b43d7885b67b085025353`
for both. These are SAT results: independent model verification establishes
their result; certificate equality is correspondence evidence.

Candidate wall time is 1.076% lower; instructions differ by about 0.000137%,
cycles by -1.029% and page faults by -37. This does not support a large
source-induced work or instruction-cost increase in this pair. It does not
establish the cause of the original 1,200-second timeout, whose hardware
counters were absent and final search was incomplete. Order/cache/occupancy
effects remain possible, and SAT model bytes are not compared. The
[analysis](2026-10-03-snw16-matched-retention-diagnosis/analysis.json) retains
all native timings, work and hardware counters.

## Preregistered next measurements

Before submitting additional work, the
[bounded protocol](2026-10-03-snw16-matched-retention-diagnosis/retention-followup-preregistration.json)
fixes exactly one reverse candidate/predecessor pair and one complete WV100
recovery campaign. It permits no automatic retry loop, tuning, node constraint,
array throttle, increased solver/checker allowance or substitution of an
isolated retry for the complete gate.

- Reverse pair `1747088`, watcher `40486`: identical pins/input/caps/oracles,
  candidate first, to expose repeat spread and order sensitivity. A timeout,
  work divergence or material candidate slowdown requires diagnosis rather
  than another automatic repeat.
- Full WV100 recovery `1747089`, watcher `10695`: all original 100 instances,
  accepted baseline and the exact retained resource/retention/censoring
  contract. The maintained scorer must accept all rows, with no missing result
  or checker rejection. Independent model/proof oracles remain active; each
  complete proof pipeline retains 1,800 seconds and the original comparable
  cost bound.

Both submissions are observed live. The
[preflight](2026-10-03-snw16-matched-retention-diagnosis/retention-followup-preflight.json)
verifies seventeen recovery and twenty reverse pin bindings, unchanged
resources/caps/input/binary identities and zero solver execution during
preparation. Only the failed gate is rerun; the other three passing sets are
not repeated. All original negative evidence remains frozen.

No source, allowance, reward or learning change occurred. Debug `1746924`
and Claude-owned fuzz remain live; all 795 source bindings are retained.
Next: collect these two bounded follow-ups and exact Debug/fuzz acceptance,
then audit every original objective requirement. Neither the first pair nor
submission of a recovery campaign establishes whole-goal completion. No
cleanup, unrelated cancellation or push occurred.
