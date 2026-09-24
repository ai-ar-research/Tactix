# AM341: root-fixed watch removal contract — unaccepted checkpoint

Date: 2026-09-24 UTC. This is a causal assertion repair and validation checkpoint,
not solver acceptance, a performance claim or promotion. N1–N4, all tier
denominators, the four startup-certificate targets and the two late-row targets
remain unchanged. No excluded row was reopened.

## Diagnosis and implementation

Claude's terminal AM339 report handed off seed 340726072, original 39-variable /
180-clause CNF and 177-clause minimum. Old AM339 and AM337 Debug pins abort in
`kernel_unwatch_large`; Release returns independently certified UNSAT. Bounded
authority: `benchmarks/runs/20260923-am339-parser-liveness-claude-fuzz/triage/unwatch-assert/reproducer/`.
The owner's description of a broken watch invariant was a failure hypothesis;
direct Codex debugging established a narrower cause: an over-strong assertion.

SLURM jobs 1716420/1716421 identify the call from ConflictAnalysis's level-ranked
watch repair after sparse reduction. Job 1716422 records displaced literal 35,
clause ref 40, value -1, assignment level zero and connected large watches.
That job subsequently exits 1 because the diagnostic requested a nonexistent
`collect_watermark` field; its preceding value/level observations are retained,
not treated as a successful complete diagnostic script.

Sparse collection intentionally flushes root-fixed literal watch lists while
retaining unswept prefix clauses. `kernel_check_invariants` already permits this
absence, and the existing sparse-prefix collection unit test demonstrates it.
N1 permits decisions before propagation, so a later conflict can promote the
absent root-fixed slot. Unwatch now permits absence exactly when
`kernel_root_value(k,l) != 0`, matching that invariant. It still removes an
existing entry, including one attached after a flush; absence on a non-root or
unassigned literal remains fatal. No rule guard, proof delta, action surface,
policy or Release behavior changes. Engineering ownership is implementation
notes §12; M3's abstract calculus is unchanged.

New tests construct the six-variable sparse-prefix/decision/conflict-repair
sequence in both watched positions and then re-propagate through the repaired
pair. Separate fixed-slot tests cover both signs and present/flushed entries;
three Debug death tests retain detection of missing unassigned/non-root slots.
Regression 037 preserves the owner's exact minimized CNF and random seed.
The initial positive fixture exposed the old assertion, then required explicitly
enabling conflict reuse to test that path; both corrected build modes pass.

The owner separately reported a UBSan error in `test_random_online`: its
sequence-repeat coverage array had three entries for four supported modes.
The array now derives its size from the last repeat enum; the all-modes coverage
assertion includes the fourth entry. Per-invocation three-mode arrays remain
unchanged because their generator still selects exactly three modes. This is
a test-only bounds repair, not a solver change.

## Immutable authority and validation

Authority: `benchmarks/runs/20260923-am341-unwatch/manifest.json`.

- Base commit: `5e4af489f3ecf20201a7efca34c8cf242a0e580b`.
- Source patch: `e5473fd3f3fb701294497fc49e4e5d406cca48af8180db6e594fee5aa3f23ddf`.
- Source archive: `165e36080a5a418fc85a4641d7b8e1caaf281cc80f774cabede064a820a2df7e`.
- Debug pin: `build/pin-unwatch-debug-20260924`, SHA256
  `722f1eb5c9cba0fbbb59ae7dbddcc4c07578586f49ba33bc395b232942f1020c`.
- Rebuilt Release is byte-identical to AM339's immutable
  `build/pin-parser-liveness-release-20260923`, SHA256
  `cbe999105947efd3782bf4abc7ec469136ec0b0551322df1039bd5e250e596c2`.
- All seven proof checker pins are unchanged and hash-verified. The archive
  includes the new test and corpus files as well as the maintained AM340 tooling.

Focused Debug kernel/analyze/random_online/unwatch_fixed: 4/4 pass. Focused
Release analyze/random_online/unwatch_fixed: 3/3 pass. Fresh aggregate Debug
build succeeded; CTest has **178/179 passed, demo still running** at this
checkpoint. Do not claim full completion. Session 3825 / ctest PID 1930485,
log `...-am341-unwatch/ctest-debug-all.log`. Existing compile warnings include
sub-millisecond filesystem clock skew and unrelated source warnings.
Documentation size, option coverage, rule/policy boundary and diff whitespace
checks pass; hard-budget warnings remain. No documentation compaction occurred.

Direct bounded validation job **1716498**, cpu-dense-hwulab-q, 2 CPUs / 16,000 MB,
completed exit 0 in nine seconds. The small allocation is a sequential exact
reproducer check, not broad fuzz or a performance measurement. Both original
and minimum × Debug/Release × random/replay × six modes (automatic, native DSR,
ASCII DSR, binary DRAT, ASCII DRAT, native LRAT) give **48/48 verified UNSAT**.
DSR passes backward and every-lemma Trestle audits; explicit DRAT passes pinned
drat-trim; native LRAT passes CakeLPR. Proof bytes agree between Debug and
Release for every corresponding mode/execution. Both old-Debug controls abort
at the original assertion. Commands, proof hashes, logs and results are in
`...-am341-unwatch/validation-1716498/`; the frozen archive supplies its oracle.
Pre/post pin verification passed. No altered checker acceptance rule is used.

Explicit Opus owner PID 1934171 (session 34151) is PREPARING in
`...-am341-unwatch-claude-fuzz/OWNER_STATUS.md`. Its immutable request is
`...-am341-unwatch/claude-fuzz-request.md`; execution log is `logs/claude-owner.log`.
Scope: rerun affected 50k Debug random matrix, sanitizer/regression execution,
and fill AM339's missing count-PB controls, actual tamper control and random-PBP
reachability floors. Passing unchanged Release/parser legs are preserved, not
repeated. Codex owns debugging and reads only published status, terminal reports
and explicitly bounded handoffs. No Fable or fallback to Fable is used.

## Terminal AM337 full-gate snapshot

All 272 tasks in campaigns `20260923-am337-proof-lifetime-{rank1-50,b28c5,wv100,wv2-89}`
are terminal. Their Release pin is **40b0de22**, not AM341's Release pin.

| gate | independently checked | disposition |
|---|---:|---|
| Rank 1–50 | 50/50 | 7,266.692 solver seconds < 8,115.735 target |
| B28+C5 | 33/33 | 916.196 seconds < 952.263 target |
| WV100 | 97/100 | two genuine-empty-input missing-LRAT failures; ncc checker timeout |
| WV2-89 | 82/89 | 78 checked within 600 s; six solver timeouts; one CakeLPR reader failure |

All four requested startup PB certificates verify on the combined AM337 pin.
The last ncc task 1715991_89 ended FAILED, independently recorded NOT-VERIFIED:
solver 162.181 s; drat-trim emits `s TIMEOUT` after 40,000.949 s, total check
40,116.714 s. Source `drat-trim/drat-trim.c` defines a 40,000-second internal
default, explaining why the 57,600-second outer allowance did not keep it alive.
This is not a verified row and not evidence that merely extending the inner
limit will suffice. Earlier per-row/performance detail remains in the frozen
AM337/AM340 reports; nothing is silently transferred to the successor candidate.

## Open correctness and acceptance work

AM339's terminal report is FAILED with the assertion and tooling handoffs.
AM340's terminal report is also FAILED on affected tooling surfaces, despite
rejecting both original false proofs on all seven surfaces and finding zero
soundness-class discrepancies in 35,000 cases. It hands Codex bounded F1
(optional conversion predicate uses original layout) and O3 (tier runner's
success-substring/exit-status acceptance). Both are still unrepaired here.

Other explicitly reported debts remain: legal comment-inside-clause model
parsing; routed repeated-literal CNF/LRAT mismatch; genuine-empty-input LRAT
bridge; malformed DSR rejection classification; DSR reader layout limitations;
and the nested drat-trim timeout. AM340 classifies these as pre-existing/fail-
closed observations, not clean acceptance. Preserve its exact classifications.
Finish affected fuzz and these repairs before successor full routed gates and
promotion. Then perform the requested AM333 matched-work owner comparisons for
hcp_CP18_18 and 6g_6color; neither under-600 target is achieved yet. Do not reopen
the user's excluded rows or lower the full WV2-89 success requirement.

## Hygiene and handoff

All 272 final gate JSONs report artifact_deleted and derived_artifacts_deleted
true with cleanup_pending false. The new bounded diagnostic certificates and
traces remain decision-changing evidence. No files or jobs were deleted or
cancelled. Queue audit preserved unrelated t4-ws500 (1714784), sweep-repeat
(1716485) and interactive allocations; new direct validation is terminal, and
the live CTest/Opus processes are preserved. Existing baselines, caches, campaign
roots, reports, pins and owner-private evidence remain untouched.

Andrew deliverables: `report-to-andrew/20260924T012000Z-unwatch/report.tex` and
`report.pdf`, compiled twice. PROGRESS owns live status and the next concrete
actions; this report is frozen checkpoint evidence, not a completion claim.
