# Sweep small-environment solve lifecycle

## Outcome

Isolated5522 on5510 removes two related small-environment shortcuts. It passes
Release86/86 twice and trace85/85; matching Release SHA256 is
`67867f4b999b55b494b5ec8f291096bc6f3682f4c99de5792871e057b21e0f4d`.
It is unpromoted and current-source fuzz is pending. Causal5529 closes all
18,593,182 decisions through19376; the first timing pair is0.894657x5510.
The Rank1–40 target remains unmet; the last full comparison is1.165337x Kissat.

## Evidence and reference behavior

Validated5515 matches all11023 native first-call seed/environment-size records,
then processes seven additional seeds. Aligned5520 joins effort records to
their seed/environment instead of shifting indices when zero-effort records
are absent. All33 omitted environments have270 variables/one clause, costing
26836 native ticks. The first is340186: Tactix0 versus native814 ticks/one solve.
The common prefix costs54798904 versus54824412 ticks; the seven extras use25505.
Equal sizes are not a proof of equal clauses/models, and36 total effort records
differ; not every discrepancy is assumed to be a skipped initial solve.

The embedded engine is not missing one-clause support. Twelve fixed cases
link actual5510 SubSolver objects and the pinned native5414 Kitten library:
widths2/3/17/270, three consecutive environments each. Initial/post-flip ticks,
flip counts, random generator and model hashes match byte-for-byte. This is a
fixed engine comparison, not a complete native Sweep or CaDiCaL run. Actual5510
Sweep objects skip all four one-clause fixtures. Source/hash authority:
`benchmarks/runs/20260908-5522-sweep-small/reference-validation.json`.

Kissat always runs the initial solve after environment construction. CaDiCaL
also solves one-clause environments but declines a singleton environment.
The project tie-break selects Kissat's complete lifecycle, including empty
environments around a live seed; fixed/retired seeds remain inapplicable.

## General repair and tests

`sweep_process_seed` no longer returns merely because it has fewer than two
variables or clauses. Its builder still returns false for no usable clauses
or a fixed/retired seed, preserving that public contract. Sweep distinguishes
a constructed clause-free live seed from an absent environment through the
variable list. It then uses the existing import, phase randomization, initial
solve, model refinement, proof capture and exact effort path. No synthetic
tick charge, new option/constant, bound, primitive or proof vocabulary is added.

New fixed tests exercise four one-clause widths, bounded solves, empty/singleton
and invalid public requests, fixed/retired decline, repeated sessions, and
absence of false equivalences/units or formula mutation. A connected dense
three-seed fixture pins the enclosing boundary: importing each environment
costs one actual Kitten tick, the inclusive inner limit cuts the solve, equality
admits a second attempt under the strict outer limit, and the third stays queued.
Existing complete DRAT/LRAT, model, public API and rule suites also pass.

Initial tests exposed the builder-empty return, so the repair includes it.
The budget fixture initially expected a stop at equality, then omitted dense
connection for its third unwatched large occurrence. Its setup/expectations
were corrected without changing budget or admission code. Earlier failed logs
are retained. No soundness, model or checker failure was observed.

Source patches and final tested pins/logs are frozen under
`benchmarks/runs/20260908-5522-sweep-small/manifest.json`; main production source
is unchanged. Claude's terminal5492 report is PASS on5473:60,000 cases,
55,036 checked verdicts, zero failures and360 direct reachability cases.
That report is trusted without independent artifact inspection. New5539 is
Claude-owned under live session92433 and covers5522, including5510. It requires
direct retained-entry/small-solve reachability and the model/proof/stress oracles.

## Predecessor timing and successor measurements

All three5517 pairs are complete solver snapshots, with zero checks complete
at the frozen snapshot. Candidate5510 takes438.175/438.003/438.503 seconds;
predecessor5473 takes410.609/411.332/409.072. Median paired ratio is1.067134,
range1.064841–1.071946. Within-arm complete non-timing statistics and proof
hashes agree. Arm timing spreads are0.114%/0.550%, both below2%.
Thus the entry repair alone is consistently slower, not a useful runtime win.
These are changed-work comparisons, not isolated unit-cost measurements:
candidate/predecessor conflicts7363082/6339726, decisions193889493/201581008,
propagations3469878658/3825684273. Snapshot authority is5517/solver-checkpoint.json.

Successor jobs are full-width and unthrottled with no node constraint:

- Trace5529/job1672251 is terminal/clean. Comparator5533 validates exact native
  whole-parent hashes and18,593,182 candidate/native decision rows, all equal.
  Both selected calls match all11023/9343 seeds and environment sizes, with
  identical7/4 units. Aligned5534 finds first-call totals54824414/54824412 ticks
  and117361 solves each; second-call54824417 ticks/94212 solves agree exactly.
  First remaining cost difference is seed91930,3748 versus3747 ticks. This does
  not prove identical physical clauses/models or parity beyond the stopping point.
- B28+C5 5530/job1672252: all66 expected solver verdicts;64 checked/clean at
  07:33UTC, two T105 checks pending. Predecessor5516 is also64/66 checked.
- Timing5531 wave0/job1672253:389.931/435.844 seconds=0.8946572626903203x;
  candidate conflicts5584514/7363082, decisions235573239/193889493,
  propagations3435968189/3469878658. Fewer conflicts and more decisions make
  this a changed-work comparison, not a unit-cost claim. Proofs pending.
  Wave1/job1672326 also returns expected UNSAT:388.231/439.077 seconds.
  Final wave2/job1672385 owns4-5; score all three via5536. Replication is
  required before accepting a timing gain; all checks continue asynchronously.
- Full5535/job1672328: unchanged80-task Rank1–40 matrix, native pin, runner and
  allocation/checker contract from5485. General causal closure plus the first
  pair trigger investigation, not acceptance. No ratios multiplied across runs.
- Trace5537/job1672356 extends Rank32 to100K conflicts. Comparator5538 requires
  exact full native5479 replay and the complete repaired5529 candidate prefix
  before localizing the next divergence. Watcher85047 remains live.

Main production tests pass77/77 in5.70s; documentation budgets pass with five
low-reserve warnings. Measurement-only helpers reuse the fixed runners and
existing validators with explicit pin/input/membership assertions; no new
solver behavior requires additional unit tests for those campaign bindings.

Next: finish three timing pairs, score all40 full pairs and inspect their
work/runtime/memory losses, localize the extended trace, and revisit checks and
Claude's fuzz outcome asynchronously. Pending checks never close a checked
gate or authorize final promotion. No manual deletion or production change.
