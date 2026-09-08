# Rank32: redundant-clause deletion, not a missing forward action

## Outcome and next decision

The first differing Rank32 forward pass removes eight clauses in Tactix that
Kissat leaves behind. Complete large-clause and binary snapshots now establish
that each of those eight clauses contains a binary retained by both solvers.
The outgoing formulas are therefore logically equivalent: the extra clauses
are redundant by direct subsumption, not merely equal in clause counts.
Retain the valid simplification; do not disable it to reproduce Kissat's trace.
This does not establish a faster scheduling policy or explain the full runtime
regression. Later BVE differences, marking history, and real per-step cost
remain open. The next implementation priority is the separately proved Rank31
forward-unit continuation gap, not undoing these deletions.

The full Rank1–40 goal remains unmet. Current5741 takes7807.759s versus the
fixed Kissat6810.066s:1.1465026918x. All40 expected solves have arrived;
39/40 candidate and40/40 reference certificates are checked at this checkpoint.
The original29-checked solver snapshot remains frozen. No solver repair or
promotion is made in this checkpoint.

## Causal chain and validated evidence

Authority is under `benchmarks/runs/`:

- `20260908-5771-reuse-target-rank32-extended/summary.json` and
  `boundary-localization.json`: the separately verified40,102,204-row100K
  prefix is followed by15,050,563 matching suffix decisions. The first decision
  split is at conflict179463,decision55034320,level714. The first33 portable
  forward/BVE boundary rows agree. Then Strengthen ends with478403 versus478411
  irredundant large clauses. Initial counts agree; counts alone were not enough.
- `20260908-5779-rank32-forward-round/summary.json`: Tactix checks both full
  candidate streams,312412/312412 and505929/505929. Neither unit exits nor quota
  cuts explain this instance. Full ordinary300K traces and partial proof bytes
  exactly equal the parent. This distinguishes Rank32 from the Rank31 witness.
- `20260908-5786-rank32-native-forward-begin-formula/summary.json`: all entering
  large-clause multisets, physical clause order, literal poses and root values
  agree. There are46891 raw metadata differences; allocation-local `shrunken`
  is among them. Metadata fields are not all assumed semantically portable.
- `20260908-5787-rank32-native-forward-end-formula/summary.json`: Tactix-only
  large-clause copies0; Kissat-only copies8. Every extra is irredundant and
  contains positive literals217824 and217939. Original candidate snapshots are
  retained from5782/5783; only missing native snapshots were repeated.
- `20260908-5791-rank32-selected-forward/summary.json`: selected Tactix
  candidate297096,id4792702 is subsumed by binary(217939,217824). Native
  discovery filters the exact same ten-literal clause with `detail=1`, meaning
  only one variable is marked for subsumption. Both full ordinary streams and
  partial proof hashes exactly replay5771. This is a selected-action witness,
  not a claim that all eight actions were individually traced.
- `20260908-5794-rank32-forward-binaries/summary.json`: complete physical binary
  multisets agree at the first Strengthen entry and exit. Each side retains
  exactly one copy of(217824,217939) at both boundaries. The comparator verifies
  that all eight extra native clauses contain that pair. Their deletion is
  therefore valid in the shared formula, regardless of other root literals.

After the following BVE, active populations are93597 Tactix/93589 Kissat and
binary multiset differences are46 Tactix-only/55 native-only copies. These are
downstream of the now-classified forward effect, not evidence that the extra
subsumption was unsound. The second forward exit has46/48 binary differences.
The reference runtime on Rank32 is274.456s versus633.485s current Tactix and
316.715s predecessor5672. This changed-work regression is still a loss, not an
accepted runtime tradeoff. Root-reuse5718 lacks an isolated full40 timing view,
so no single-source timing attribution is justified.

Source-backed hypothesis, not yet a traced root cause: Tactix's
`kernel_touch_sync_fixed` marks newly fixed variables; native
`kissat_mark_fixed_literal` deactivates without marking `subsume`. Both forward
admission predicates count marked variables physically present in a clause.
That could explain the extra admission with a root-false literal, but exact
mark history was not recorded. Do not claim it proven or change the mechanism
without a common-state differential test and a complete relative-strength case.

## Validation and retained setup failures

All completed snapshots and selected-action runs use immutable5742/native
diagnostic binaries and exactly replay5771's ordinary traces and partial proof
bytes. They are stopped diagnostics, not independently checked final proofs or
clean timing measurements. Full candidate certificate gates remain separate.

Original5782/5783 native formula selectors used `strengthen_begin/end`, whereas
the standalone set hook requires `name=Strengthen|stage=begin/end`. Their normal
replays were correct but snapshots absent. Original runs were retained. Actual
positive/negative native-selector controls passed before the native-only
5786/5787 successors. No solver failure or changed correctness oracle resulted.

The original extended-prefix comparator5772 confused Boolean `common_reached`
with the requested numeric common count. Successor5778 uses the maintained
header-aware parser and four unit tests. No old evidence was edited to pass.
The new binary parser's first fixed test caught an eight-versus-seven-field
header expectation; it was corrected to the actual source-emitted framing
before comparison. It now preserves duplicate copies and rejects six malformed
controls. The formula parser also has six malformed controls plus duplicate
and pose/root validation. No solver code changed, so no duplicate source fuzz
campaign was launched for observer configuration.

Maintained benchmark tooling has18 passing unit tests:11 fixed-reference
protocol/score tests,4 prefix-schema tests,3 checked-verdict acceptance tests.
The new fixed-score case separates a genuine final timeout from a rejected
certificate. Unknown dataset labels admit SAT or UNSAT only after an independent
successful model/proof check, correct solver exit, and no timeout. Exact same
acceptance is exercised with actual positive and negative checker artifacts in
the new experiment's `oracle-validation.json`, not a mirror solver or fuzz run.

## Closed checks, background campaign, and isolation

Predecessor5712 is now80/80 checked/clean at1.110877x Kissat. Its frozen summary's
generic UNFUZZED limitation does not override prior Claude5715's terminal PASS
for5672. Root-reuse5727 B28+C5 is66/66 checked/clean,combined0.948091x production.
Current5741's5754 B28+C5 is also66/66 checked/clean: B0.774144x,C1.026787x,
combined0.934250x production, no timeout or solve loss. C alone is slower, so the
combined headline is not a claim that every regression instance improved.

User-approved breadth experiment:
`20260908-5784-experimental-tierd-paired-cpuq-4cpu-32g-1200s`, job1673402.
It runs both current Tactix and Kissat on all1166 frozen TierD inputs as2332
instance-major tasks. Each requests4 logical CPUs,32768MiB,cpu-q; the solver is
single-threaded and bound to one granted CPU. The user clarified1200s is the
solver cutoff, so the existing atomic solve/check/cleanup protocol gets a5-hour
allocation and16000s proof-checker allowance. There is no array throttle or node
constraint. This is not the canonical5000s gate or a replacement12CPU reference.

Both arms have independent experiment-local frozen binaries, checkers, runner,
matrix and launch file. Existing normal campaign scripts and pin hashes were
revalidated unchanged, including5712,5727,5754,5758 and the active diagnostics.
Only the maintained runner source and the new experiment's frozen copy gain the
UNKNOWN-label handling. The cpu-q nodes are disjoint from the development
partition, but filesystem and account quotas remain shared. At first queue
inspection65 tasks were running with4CPU/32G; the first24 completed records
were checked/clean. The companion audit owns the later checkpoint count.
Do not wait for this breadth campaign or pending proofs before developing.

Claude-owned fuzz5765 remains unresolved, not PASS. Owner CLIs8493/38524 ended
with interim messages;96672 failed before inference with ENOTFOUND. An external
network retry was explicitly denied because it could transfer repository
context. Explicit user approval is needed; no bypass or independent fuzz-job,
log, artifact, or report inspection was performed. Prior5672 PASS is not5741
coverage. Current-source promotion remains blocked on that gate.

No manual deletion was performed. Terminal diagnostic runners certify their
node-local input/partial-proof cleanup. Unique traces and pins remain local;
large raw streams remain hash-bound rather than being needed in Git. Uncertain
old fixed-fixture scratch is retained. Existing audits remain frozen; the
companion successor audit explicitly supersedes only changed living tooling,
PROGRESS and an earlier still-running native log observation. Push remains
unapproved; do not retry it. Andrew's new local TeX/PDF report is not sent.
