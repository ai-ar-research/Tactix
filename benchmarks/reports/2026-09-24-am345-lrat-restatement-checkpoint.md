# AM345: checked input restatements for converted LRAT

Unaccepted tooling-only checkpoint, 2026-09-24 UTC. Original inputs, solver and
checker binaries, rule/API behavior, proof routing and gate denominators are
unchanged. Independent affected validation is pending; no promotion.

## Cause and implementation

The routed repeated-literal rejection reproduces on `1 1 0; -1 0`, as well as
the retained five-clause conversion reproducer. Pinned drat-trim deduplicates
input literals in memory, then emits LRAT hints referring to the original
clause IDs. CakeLPR retains original multiplicities and cannot use a repeated
literal clause as the singleton assumed by those hints. Native DSR verifies
the same fixed refutations; the original routed LRAT fails. Main fixed probes
are `benchmarks/probes/scratch/am345-{duplicate-routing,lrat-restatement}.py`.

The repair keeps the original CNF and original converted LRAT intact. For each
non-tautological input clause containing a repeated literal, the adapter emits
a fresh LRAT RUP lemma containing its distinct literals, with the original ID
as its sole hint. Negating that lemma falsifies every literal of the original
clause, so the one-hint derivation is valid. It then deletes the original IDs,
remaps their uses to the derived IDs, and shifts converted proof IDs beyond
the prefix. Both positive and negative RAT hints and deletion IDs are mapped;
repeated deletion-line labels are preserved modulo the offset.

Tautological duplicate clauses are not restated as checker-divergent RUP
lemmas. No-duplicate and tautology-only cases retain the original LRAT path.
The validated DIMACS spelling now records the number of clauses containing an
in-clause duplicate, including duplicates separated by comments or lines.
It still preserves every literal, multiplicity, clause order and original ID.
The adapter streams clauses and proof lines and retains the duplicate-ID map;
it does not hold the entire LRAT. Both named and anonymous scratch are supported.

Both check_routed and runtime apply the adapter after successful drat-trim and
the existing genuine-empty-input bridge. CakeLPR must then verify the lifted
proof against the unchanged original clause sequence. Runtime records the
restatement count and actual CakeLPR command. Native LRAT is never adapted;
PBP/witnessed-DSR routing and optional explicit-DRAT conversion exclusions stay
unchanged. A skip is not a verified certificate. Plan section 5's routing and
original-input contract are preserved; there is no solver calculus change.

## Evidence and immutable authority

All six new layout/encoding subcases reject at CakeLPR before caller repair;
afterward they pass the routed checker and both runtime scratch modes. Tests
assert unchanged original bytes and positive adapter reachability. A fixed
RAT derivation checks negative input-hint remapping, input/derived deletions
and repeated line labels with real CakeLPR. Corrupting its required final
chain rejects. Separate tests corrupt a required duplicate-unit restatement
hint: real CakeLPR rejects in both maintained executing paths. An exploratory
mutation of an auxiliary lemma was not a reliable rejection control; the
committed controls deliberately affect required chains instead.

Final tests: proof-dispatch **34/34**, bench oracle **24/24** (22 unit plus two
pinned-binary tests), bench runtime **14/14**, anonymous scratch **3/3**,
runtime integration **7/7**, assert-style model tests, and full selftest pass.
The selftest independently checks known-bad model/proof wrappers and shrinking;
reference verdict coverage is 145/150 Kissat and 96/100 CaDiCaL, with nine
timeouts explicitly not counted as checked cases. Logs live in
`benchmarks/runs/20260924-am345-lrat-restatement/logs/{fixed,selftest}.log`.

Authority `benchmarks/runs/20260924-am345-lrat-restatement/manifest.json`:

- base `0ecc873202025f9d4ef15fa463d5a50c9870de22`;
- patch `fd429a279b019ecbf592ab6870cc7edd675386a543b4bf70c56ab1317e6e1574`;
- archive `294251d907ed296b49fb31b690cd095a23189cffb319e7707b0cddf995008ea4`;
- AM341 Debug 722f1eb5, unchanged Release cbe99910 and all seven checker pins.

Explicit Opus owner PID 3377189 / session 99280 owns
`...-am345-lrat-restatement-claude-fuzz`: old negative controls, ID/format
boundaries, corruption controls, named/anonymous execution, 8,000 generated
focused cases (2,000 per build/policy), original-CNF models and random replay.
Only status, terminal report and bounded handoffs are read by main. No Fable.

## Corrected full-suite status and open diagnostics

AM341 full Debug CTest is terminal: **178/179 passed**, total 3,181.99 seconds.
The demo passed; `sweep_proofs` failed at 27.21 seconds on corpus 033, seed
296326708, every-lemma Trestle rejection. Earlier AM341--AM344 live summaries
mistook the completed-test counter for the passing-test count. They are frozen
historical artifacts; this correction and PROGRESS supersede that live claim.
The authoritative log is `...-am341-unwatch/ctest-debug-all.log`.

Main diagnostic authority `benchmarks/runs/20260924-am345-sweep-diagnostic`
pins the corpus, runner and AM344 oracle plus checkers. Full-width array
**1716608**, six tasks, 12 CPUs/32 GiB each, no node constraint or throttle,
compares old Debug c0320acb, current Debug 722f1eb5 and Release cbe99910 in
ASCII/binary DSR. Each preserved proof is checked three times in each mode
(backward and every-lemma), with separate stage logs and outputs. All six tasks
are terminal and **36/36 pipelines verify**. All three solver pins emit identical
proof bytes per format (ASCII f0cf8f2e, binary 4971af16).

The local test default differs from the pinned checker: local dsr-trim
02824056 produces LSR f1664b0e that Trestle rejects; pinned dsr-trim 3b5c7737
produces LSR eb9772be that the same Trestle accepts, on the identical preserved
proof. `local-checker-comparison/result.json` records commands, hashes and
verdicts. The mutable build's solver hash equals the immutable current Debug.
This isolates the observed failure to the unpinned elaborator, not a difference
in solver output. The local checker binary was not replaced or rebuilt.
Focused pinned sweep CTest passed in 32.59 seconds; fresh full pinned CTest
(session 87252) is running, with separate `pinned-{sweep,full}-ctest.log` files. Every checker
override is explicit; the original failed suite is not relabelled green.

AM342 owner terminal REPORT.md is scoped PASS on F1/O3: 34,000 generated cases,
13,377 matching replays and no in-scope discrepancies. It explicitly retains
79 duplicate-routed rejections, four empty-input failures and three witnessed
reader-layout failures as known unaccepted debt; no combined acceptance.
Its terminal report is the authority, not the aggregate job exit status.

AM341's separate owner still owes its bounded handoff for four native DSR
rejections; its firing-floor top-up remains live. AM343's owner reports a
candidate huge-budget outer-wait OverflowError and continues affected testing.
AM344's fixed tooling leg passes; its oracle job 1716628 is submitted. Main
ncc diagnostic 1716522_2 still checks after UNSAT in 242.680 seconds; three
other diagnostic rows verify. None substitutes for full successor gates.

## Hygiene and continuation

Preserve owner campaigns, baseline/cache/tier authorities, source/checker pins,
all failed evidence and the completed main diagnostic. Active AM341 top-up,
AM343 focused arrays, AM344 oracle and main diagnostics are mapped to their
existing campaigns; unrelated sdsl arrays and interactive allocations are
untouched. No benchmark data or jobs were deleted/cancelled. The maintained
successful selftest cleans its own scratch. Terminal AM337 retains 272 complete
cleanup records. Documentation budget warnings remain, PROGRESS 349/350;
no automatic compaction was performed. Seven documentation CTests pass and all
293 tooling-file hashes match the frozen authority after testing.

Next finish the pinned sweep/full-suite validation and consume AM341's bounded
handoff, then close affected fuzz and full-tier gates before the requested AM333
matched-work comparison on the two late rows. The complete user objective and
all exclusions remain intact. Andrew deliverables:
`report-to-andrew/20260924T021500Z-lrat-restatement/report.{tex,pdf}`.
