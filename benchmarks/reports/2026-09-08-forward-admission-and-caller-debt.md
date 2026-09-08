# Forward admission and caller-closure checkpoint

## Outcome

The Rank1–40 objective remains **unmet**. Frozen continuation candidate5798's
full view5811 solves all40 expected verdicts in **7675.246s**, versus the fixed
12CPU Kissat reference's **6810.066s**: **1.1270442900x**. This is a cross-window
measurement, not an isolated causal speedup over5758's7807.759s. The first
complete solver snapshot had29 checked candidate results; subsequent checks
continue asynchronously. Never overwrite that snapshot.

New isolated5830 removes a demonstrated C2 admission producer mismatch. Two
independent Release builds pass94/94; trace passes93/93. Eight new fixed tests
and28 independently checked DRAT/LRAT proofs pass. Its B28+C5, Rank32 causal
replay and full40 measurements are submitted independently. No runtime gain,
checked gate, current-source fuzz pass or production promotion is claimed.

Authorities:

- [5798 full solver checkpoint](../runs/20260908-5811-forward-unit-ranks1-40/solver-checkpoint.json)
  and [work decomposition](../runs/20260908-5811-forward-unit-ranks1-40/next-losses.json).
- [5830 source/build authority](../runs/20260908-5830-forward-admission/manifest.json).
- [B28+C5 launch](../runs/20260908-5836-forward-admission-BC-gate/launch.json),
  [causal launch](../runs/20260908-5837-forward-admission-rank32-prefix/launch.json),
  [full40 launch](../runs/20260908-5838-forward-admission-ranks1-40/launch.json).

## Where the remaining gap is

| Rank |5798 seconds|Fixed Kissat seconds|Extra seconds|Conflict ratio|
|---|---:|---:|---:|---:|
|32|626.521|274.456|352.065|2.510381|
|37|489.930|346.372|143.558|1.190294|
|31|391.113|264.532|126.581|1.403354|
|27|341.326|245.726|95.600|See bound work record|

Rank32's proof bytes and all published non-timing counters are unchanged from
5741. Its propagations are1.394433x and decisions1.278883x native. Pure unit
cost therefore cannot explain its runtime gap: substantially different work
is being performed. That does not prove every extra simplification is harmful.
Rank37 and27 are also unchanged in published work/proof bytes by5798; Rank31,
35 and39 change. Counter/time quotients across different trajectories do not
isolate instruction cost. Memory remains an independent open gate.

Parent5798 B28+C5 has66 expected solver outcomes. B is305.666/409.555s
=0.746337x production; C is758.559/704.823s=**1.076240x**. Combined0.954995x
must not hide the C regression.64/66 certificates were checked at the initial
full-view checkpoint. Pending checks are unverified, not accepted passes.

## Recovered Rank31 work and corrected observation boundary

[5810](../runs/20260908-5810-forward-unit-rank31-prefix/summary.json) validates
the repaired300K stop, complete traces and cleanup.5798 matches76,952,511
parent decisions before its intended effect at conflict179823 and76,952,513
native decisions before the next split, also179823. The first forward exit
has12,104 fewer irredundant large clauses,1281 more binaries and4 more fixed
variables than5741. Continuation repairs real missing work, but does not close
the whole native trajectory.

[5815 physical snapshots](../runs/20260908-5815-rank31-forward-residual-formula/summary.json)
replay each cached arm's ordinary traces and partial proof bytes exactly.
Both physical first forward-end snapshots contain493638 irredundant large
clauses. Native's portable Strengthen-end counter says493635. This is **not
evidence of a bad native counter**: its raw formula hook occurs inside
`forward.c`, whereas `eliminate.c` emits the portable end row after reconnecting
all irredundant clauses, resetting propagation and flushing connected units.
Tactix's corresponding caller step is missing.

[Root-normalized follow-up](../runs/20260908-5815-rank31-forward-residual-formula/root-normalized-followup.json)
still has16 unmatched clauses per arm; each selected Tactix clause is shorter.
Root-false storage alone does not explain these differences. The first incoming
and outgoing binary multisets match; later BVE boundaries diverge. No incoming
full formula or native post-caller formula snapshot was captured, so neither
complete input identity nor the exact fate of three clauses is claimed.

Two actual-code fixed controls establish the caller gap independently:

- [Tactix5826](../runs/20260908-5826-forward-caller-debt/summary.json) runs the
  actual Eliminate scheduler with a one-candidate forward quota. A read-only
  next-slot meter sees unit5 unassigned while the shared cursor reports a
  fixpoint. Sparse phase exit later derives5 and the original SAT model checks.
- [Native5829](../runs/20260908-5829-forward-caller-native/summary.json) runs
  the same fixed clauses through actual native forward with zero effort, then
  its actual reconnect/reset/flush sequence. Unit5 is pending after forward
  and derived before BVE scheduling.

The remaining repair must perform full connected propagation **and** satisfied
clause cleanup, including proof-safe reasons and conflict handling. Merely
calling Tactix's current BVE cleanup helper is incomplete. Do not change5798's
partial forward propagation into whole-formula propagation inside the pass.

## Rank32 producer witness and isolated5830

[Fixed native/Tactix controls](../runs/20260908-5817-forward-mark-observer/fixed-control/summary.json)
show that fixing a root can synthesize an admission mark in Tactix but not
native forward. That initially proved possibility only.

[5822's neutral real replay](../runs/20260908-5822-forward-mark-rank32-replay/summary.json)
now establishes provenance: at conflict179463, round9, the selected ten-literal
clause has one marked variable217864 before synchronization. Root-false217821
changes from unmarked to marked during `kernel_touch_sync_fixed`, and no other
selected variable changes. The clause therefore passes the two-mark admission
test. Native5791 filters the same clause with one mark. All ordinary300K trace
hashes and partial proof bytes match5771 exactly; six malformed-frame controls
also pass. No timing is inferred from observer runs.

5830 removes only C2's call to `kernel_touch_sync_fixed`. It preserves actual
clause-addition marks on fixed variables, C1/CCE's existing shared helper,
canonical unfiltered operation and unrestricted public single-action access.
No kernel primitive, global contract, option, constant, budget, proof vocabulary
or benchmark/formula selector is added. CaDiCaL's broader fixed-literal
candidate exclusion is not adopted; native Kissat is the scheduling tie-break.
The inference remains C1/C2; admission marks are policy advice, not soundness.
The5760/5741/5798 baseline sources and all submitted artifacts remain frozen.

The eight new state/public-action/model cases cover both sparse and disconnected
representations, root-only marks, genuine addition marks on fixed variables,
unfiltered canonical calls and explicit instances. Inherited17 continuation
state/model cases and four supplementary interruption/cleanup cases pass.
Independent checkers validate16 continuation proofs plus12 direct C2 conflict
proofs: binary, already-connected large, and not-yet-connected candidate
conflicts, each in both representations and DRAT/LRAT. Existing full CTest
proof/API/guard tests remain green. These are fixed regressions, not fuzz.

## Hygiene, limitations and next action

The maintained decision-prefix comparator gains chunked complete-row comparison;
four tests exercise all chunk boundaries, equality, differing rows, EOF and bad
framing. The benchmark suite passes22 tests. Snapshot parsers retain negative
controls; the new mark parser adds six. Documentation uses existing size,
reference, inventory and contract validators; no validator is weakened.

An initial fixed scheduler fixture accidentally selected preprocessing, so its
observation assertion failed; setting its intended search-time conflict state
corrected the fixture. No solver invariant, model or certificate failure was
found. The BC preparer initially encountered an unpinned `__pycache__` directory;
it now copies only files and verifies its partial unsubmitted copy before
resuming. Two full-view CLI calls supplied directories/files in the wrong
positions and stopped before creating a campaign. The corrected invocation
uses the authority directory and binary paths. These are tooling corrections,
not solver defects. Failed-before-measurement logs are retained, not rewritten.

The cpu-q breadth experiment5784/job1673402 remains independent and immutable:
1166 inputs ×2 solvers,4CPU/32GiB,1200s **solver only**,16000s checker within a
5h allocation. Completed tasks report scratch cleanup. No manual deletion was
performed; small fixed proofs and unique evidence remain retained. Partition
separation does not remove shared filesystem/account contention.

Current-source fuzz remains blocked on the previously denied Claude-network
approval; neither retry nor bypass occurred, and no Claude-owned fuzz artifacts
were inspected. Prior5672 PASS does not cover5798 or5830. Push remains
unapproved and is not retried. Production remains `c2a95a6`.

Next: validate5837 against the complete cached native early stream and suffix,
reporting any earlier divergence; score5836 B and C separately and5838 full40
against the unchanged denominator. Continue provisionally while proofs check.
Then repair the separate full-connection caller boundary with direct state,
model and DRAT/LRAT conflict regressions. Neither diagnostic fidelity alone nor
a partial timing improvement satisfies the1% objective.
