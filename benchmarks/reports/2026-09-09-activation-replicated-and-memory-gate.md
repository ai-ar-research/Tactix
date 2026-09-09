# Activation replication and position-key memory gate

## Decision

The activation-stamp repair 6424 has a repeatable Rank 45 benefit and repeatable
adverse effects on ranks 17, 33 and 43 versus predecessor 6383. Keep its causal
repair and adverse-case investigation isolated; do not describe it as an
aggregate improvement. No production solver source changes in this checkpoint.

The expanded target already has the required matched added-ten reference:
`benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json` binds the original
40 and the checked 6108 additions. Its 7239.010 seconds remain unchanged.
No cpu-q timing is substituted and no Kissat timing is refreshed.

## Complete repeated activation measurements

Frozen solver checkpoints:

- `benchmarks/runs/20260909-6443-activation-three-wave-runtime/solver-checkpoint-6445.json`
- `benchmarks/runs/20260909-6464-activation-adverse-three-wave-runtime/solver-checkpoint-6466.json`

Both contain all 18 expected solver outcomes, three paired waves with reversed
middle-wave arm order, and exact certificate bytes/hash and non-timing statistics
against each arm's own full-run oracle. At the checkpoint, 14/18 and 8/18 were
independently checked and clean, respectively. Pending checks remain unverified;
the original snapshots will not be rewritten as later checks arrive.

| Rank | Candidate median s | Parent median s | Median paired ratio | Complete paired range | Candidate / parent range over median |
|---|---:|---:|---:|---:|---:|
| 32 | 324.689 | 327.993 | 0.993979 | 0.989860–1.001397 | 0.858% / 1.596% |
| 35 | 498.961 | 501.912 | 0.997206 | 0.987822–1.000012 | 0.612% / 0.927% |
| 45 | 44.336 | 70.984 | 0.624589 | 0.624187–0.627451 | 0.677% / 0.283% |
| 17 | 76.742 | 26.859 | 2.851908 | 2.847761–2.875858 | 0.792% / 0.376% |
| 33 | 353.733 | 297.710 | 1.188180 | 1.182193–1.209513 | 1.529% / 0.765% |
| 43 | 125.475 | 119.559 | 1.051205 | 1.040718–1.056934 | 0.836% / 0.879% |

Every arm satisfies the 2% spread gate. Ranks 32 and 35 remain essentially
neutral at this scale, not new cost wins. Rank 45 improves about 37.5%; the
adverse effects are not plausibly erased by the measured repeat spread.
These are changes versus the predecessor, not versus Kissat. The three adverse
witnesses were selected from the complete frozen full50 table before their runs.
No selective extra repetitions or changed matrix follows these results.

The complete full50 result remains 8731.653 / 7239.010 = 1.206194355,
71.425 seconds worse than 6383. The added ten contribute 1200.091 / 428.944 =
2.797780130, with 771.147 seconds of the total 1492.643-second gap. Older checked
candidate 6169 remains better at 1.160931951. These cross-window full-view
figures do not isolate execution cost. Authority: frozen activation full50
report 6441, not a sum of selected timing witnesses.

## Work versus cost and the next discriminating steps

Ranks 17, 33 and 43 change certificates and search statistics; their losses are
not isolated instruction-cost regressions. Rank 33 current/parent conflicts
are 5,189,038 / 4,223,604, and decisions 178,461,826 / 148,810,135 in full6441.
Rank 32 instead preserves the predecessor's full work/certificate and matches
native conflict/decision counts. Its remaining native gap still justifies
engineering-cost investigation, without equating differently counted native
propagations. Both classes of gap remain in scope.

The already-submitted Rank 33 three-arm 1M diagnostic (6486, job 1678096 since
09:56:36 EDT) extends the validated 31,494,584-row common prefix. Validator 6488
requires the complete prior overlap and hash before comparing suffixes. The
Rank 35 focused Vivify replay (6483, job 1678093 since 09:51:28) tests a
pre-split work difference and requires exact own ordinary traces and partial
proof before interpreting candidate logs. Neither first difference establishes
causation, and partial proofs are not final-proof acceptance.

## Position-key memory experiment

The separate shared-blocker experiment is now complete: frozen 6456 has all
18 expected exact-parent work/certificate outcomes, 10 checked at snapshot.
Paired medians for ranks 32/35/45 are 1.003083 / 1.008333 / 1.042617;
complete ranges are 1.000960–1.005235 / 0.999642–1.008973 /
1.041748–1.043117. Every arm spread is below 2%. Reject 6448 as a convergence
improvement: Rank 45 has a repeatable approximately 4.3% execution-cost loss
and the other witnesses show no gain. No composition into 6475 and no extra
selective waves. Its prior source-control and BC evidence remain frozen.

Source 6475 remains independent of shared-blocker source 6448. Its actual-object
480-case ordering control preserves the exact digest and changes measured sort
allocation from 1,054,280 to 632,568 bytes: two temporary key buffers narrow
without changing four radix passes, aliases, public rules, ticks or scheduling.
This is not a claim of 40% lower solver RSS. Release passes 109 tests in both
independent builds; trace passes 108.

The next matrix is predeclared as ranks 10, 25 and 47: the highest full50 peak
RSS; a large 3.83x native memory excess; and the highest-memory added-ten case,
respectively. The parent peaks are 15,967,668, 3,408,252 and 4,818,644 KiB.
Frozen checkpoint 6480 contains all 66 expected outcomes and all 33 candidate
work/certificate identities against 6424; 64 outcomes were checked and clean.
B/C runtime ratios versus production are 0.727488 / 0.927397, respectively,
ordinary guard observations rather than isolated cost claims.

Campaign 6491 uses three six-task paired waves, candidate 6475 versus 6424,
with both arms bound to the same exact full-run work/proof oracle. It measures
full-solver peak RSS and sampled VmPeak separately, alongside solver runtime.
Unknown samples remain unknown. A neutral RSS result rejects any whole-process
memory-saving claim even when the temporary allocation saving is exact.
No hard 16 GiB acceptance or full50 competitiveness follows from this subset.
Helpers 6491/6492/6493 own preparation, gated submission and once-only summary.
Wave 0 was submitted as job 1678179 at 10:16:34 EDT. Later waves require all six
preceding expected solver outcomes and exact work/proof identity, not completed
proof checking. No other campaign's runner, manifest, matrix or pin was changed.

## Validation, hygiene and limits

New maintained `paired_memory.py` has seven fixed tests: complete pairs and
median versus best-of, input order, missing/duplicate waves, unknown VmPeak,
unknown RSS, invalid samples without mutation, and invalid arm/wave identities.
All 83 benchmark tests pass. No solver/observer/checker change needs additional
solver tests here; the existing source suites and exact-work campaign guards
remain required. The unchanged runner is copied to new immutable pins only.

Runtime resources remain 12 logical CPUs, 48000 MiB, 1200 solver-only seconds,
16000 checker seconds, and five hours; bounded diagnostics retain 25 minutes.
No hard node or throttle, no serialized solver arms, no live checker-scratch
inspection or manual deletion. All proof checks continue asynchronously.
Shared-blocker timing 6454 is sealed by once-only checkpoint 6456.

The first new audit attempt failed before writing an artifact: its campaign
list mistakenly applied bounded-trace `stop_conflict` checks to the runtime
memory campaign. The corrected audit keeps the runtime campaign in accounting
and its separate resource/matrix checks, not bounded-diagnostic checks. This
is a reporting-tool integration error, not a solver/model/proof failure or an
excluded measurement. The fresh audit reruns all checks before landing.

Production, cpu-q5784, fixed references and prior frozen results are unchanged.
Current-source fuzz remains Claude-owned and unavailable; no inspection, denied
transfer retry, push retry or promotion. Rank50 material tracking and private
FlatLists redesign remain unapproved. Living docs remain within required caps.
The goal is active and unmet. Audit 6494 owns later counts, exact job accounting,
integrity, tests and cleanup; PROGRESS owns the next action. Andrew's report is
local under `report-to-andrew/20260909-1020-activation-memory/`, not externally sent.
