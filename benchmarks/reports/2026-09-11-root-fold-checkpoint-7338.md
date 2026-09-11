# Root-state storage: staged prototype and validation checkpoint

Frozen September 11, 2026. Production remains c2a95a6. This checkpoint preserves
an isolated representation prototype; it is not a runtime win or promotion.

## Why the earlier prototype was not promoted

There is no minimum-positive-gain rejection rule. Clause-arena7302 remains
available as an isolated memory/runtime tradeoff. Full7312 reran every Rank1–50
instance: 8124.477s versus its preceding full view's8096.416s, +28.061s. Its
three-wave targeted assessment7309 has adverse median times on all five ranks,
while Rank31 peak RSS is consistently78928KiB lower. Keeping that evidence and
implementation is useful; calling it a speedup is unsupported. The fixed Kissat
denominator remains7239.010s; the full ratio is1.122318798,813.0769s above1.01x.
At this checkpoint49/50 full7312 certificates are checked; remaining checking
continues asynchronously. No native reference or full50 run was restarted.

The bookkeeping answer remains audit7185: ordinary Release uses O3/NDEBUG with
event trace, rule journal, proof shadow and conflict stop OFF. Direct CDCL does
not use timed registry dispatch. Some optional counters, rule clocks and dormant
runtime guards remain. The joint ablation did not consistently help; that is
not proof of zero intrinsic overhead. Shared external measurement overhead and
the solver-only timing boundary were not changed.

## Rank25 mechanism and new design

Ordinary Rank25 takes224.225s versus native246.184s but peaks at2484880 versus
888820KiB. Longer runtime therefore does not explain this case. Snapshot7327
directly reads3015698 fixed Active variables among3586539 internal variables
at C15000/D106337. Eight selected per-variable arrays occupy294.15MiB; old and
new reconstruction allocations coexist at that boundary. This identifies a
storage mechanism, not the attribution of every excess byte. The failed exact
RSS reproduction and280KiB GNU-time/proc discrepancy remain in report7328.

Kissat physically folds fixed roots; CaDiCaL also archives external proof-unit
identities. Isolated7330 adapts that mechanism without merging distinct proof
objects. The corrected source is `.claude/worktrees/runtime-root-watermark-7333`;
its Release and independent BC binary SHA256 is
`c57390e35390ef4b0a23360e5ebe8cc6d034155df22c89abcaf766f260f94c4f`.

- Permanent external values and unit IDs move to parallel cold arrays. External
  identities are never reused; model reconstruction and public value reads
  consult that archive. The initial archive costs9 bytes per external slot,
  not zero storage.
- At the existing DefaultPolicy Reduce compaction call, unreferenced fixed
  root-prefix slots are removed from hot arrays, with one representative kept.
  Roots still named by live clauses/binaries or non-root reasons stay hot.
  No hidden clause simplification, proof emission, cadence or threshold change
  is added. Existing callers default to the legacy non-folding service.
- Queue/live stamps, heap ordering, trail positions, propagation debt, proof
  maps and phase arrays are remapped. Capacity-retaining scratch is shrunk at
  actual folding. CDCL's direct per-variable array accesses remain unchanged.
- Scheduling/public logical-root counts include archived facts; physical
  array bounds and active-population subtraction retain the hot prefix count.
  Late true-level roots after decisions remain hot until backtracking puts
  them in the root prefix. This prevents compaction itself changing the
  prefix-based material watermark.

There is no measured Rank25 memory saving yet. The prototype does not add a new
startup compaction call, nor solve dense lifetime/reconstruction-growth costs.
It must still pass broad API/scheduling/closure and real-instance gates. Trace
state digests and watched-external diagnostics do not yet encode archived facts;
do not use their raw pre/post identity as a semantic equivalence oracle.

## Tests, actual failures and their disposition

7334 links new fixed tests against the actual7330 library. Both fail as intended:
folding eight roots to one suppresses Eliminate's next new-root invitation;
folding a late true-level root changes the logical prefix count without an
inference. The successor fixes both and passes those same tests. This is a
required migration coupling, not a new scheduling policy or tuned constant.

7335 passes two independent122/122 Release suites and121/121 trace tests.
Nine focused tests pass twice under unsuppressed ASan/UBSan/LSan, including
root storage, both regressions, compact stamps, kernel/reconstruction,
propagation cursors and C API surfaces.7336 independently checks64 UNSAT proofs
and64 original-formula SAT models across Release and sanitizer builds, both
DRAT/LRAT modes, root signs/orders and external holes. Fold/control proof bytes
and SAT model lines match; two known-bad proofs are rejected. Generated inputs,
proofs and model outputs totaling16582 bytes are removed after checks.

Do not describe the full Debug suite as passing.7331 finishes115/121 passing:
five earlier failures plus the intentionally terminated obsolete final demo
(1127.80s in that test). Read-only stacks found the demo in Debug invariant
checking on the scripted-policy side with no archived roots. It was stopped
only after preserving the migration regressions and validating their successor;
7331's planned micro-proof stage never ran. Its complete log is retained.

7337 reproduces all five earlier failures on untouched7302:

| Debug test | Preserved failure |
|---|---|
|event_trace_zero_overhead|Debug retains EventTrace symbols; the zero-symbol assertion fails.|
|forward_unit|Large watch retained while globally disconnected.|
|substitute_collapse|Zero LRAT hint ID reaches the proof writer.|
|substitute_retry|Zero LRAT hint ID reaches the proof writer.|
|subsume_material|Replacement ID differs from next_clause_id.|

These are inherited failures, not waived checks. The two Substitute fixtures
construct clauses with default IDs and only later attach LRAT; that is a
source-backed fixture-setup lead, not an independently checked correction.
The other invariant failures still need an actual-call-path diagnosis before
the root prototype's broad gate. Do not suppress assertions or assume all
failures are harmless because Release passes.

7332 also preserves two validation-tool failures: its parser expected a
different CTest summary spelling despite passing121/120 suites; the initial
sanitizer run failed in the sandbox/ptrace environment.7335 uses the emitted
CTest spelling and executes outside that environment with all sanitizers still
enabled. No original log/helper was rewritten or successful writer rerun.

## Next action and retained boundaries

Resolve the inherited Debug fixture/invariant failures with bounded actual
consumer reproducers and independent proof checks; preserve7333/7335 pins.
Then run unchanged B28+C5 before three uninstrumented paired Rank25/31 memory
and runtime waves, preserving closures4/32/33/49/46. Rank31 remains the largest
time excess at264.665s; root-storage work does not close it by assertion. Full50
and current-source fuzz are required before promotion. No new SLURM benchmark
was submitted in this checkpoint and no campaign script was changed.

The revamp skill guided staged hot/cold migration and rollback to7302; calculus
documents the unchanged abstract fixed facts. No production source, global
gate or plan contract changed. Existing denied fuzz access/transfer and push
boundaries remain in force: no retry, artifact inspection or waiver. Andrew's
new TeX/PDF report is local and unsent under
`report-to-andrew/20260911-0954-root-fold-checkpoint/`. Audit7338 owns hashes,
aggregate checks, active proof status, staging scope and retained failure data.
