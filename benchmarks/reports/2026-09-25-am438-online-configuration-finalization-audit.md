# AM438: online configuration and input-finalization boundary

Date: 2026-09-25. Source audit only; no API, solver, proof, checker, policy,
benchmark pin, or acceptance gate changed. This is an interim learning-readiness
finding, not the requested final report after all fixes.

`tactix_online_configure`, `_v2`, and `_v3` each call `input_complete(s)` before
converting or validating the supplied program (`src/api/api.cpp:1502-1581`).
`input_complete` replays pending input proof, calls `finish_input`, and sets
`input_completed`; `tactix_add_checked` subsequently returns `-1`. The
`OnlineStrategyExecutor::configure` replacement itself is transactional, but
an invalid program can still close the surrounding C input session before it
returns `-1`. This follows directly from control flow; no runtime experiment
or promised rollback behavior is claimed. The public C header documents the
configure signatures but does not promise that rejected configuration leaves
input open. Existing v3 tests cover atomic program replacement, not this
input-lifecycle side effect.

Moving `input_complete` below validation is **not** a safe mechanical fix.
`finish_input` retires absent variables, settles proof identities, may attach
and replay a pending LRAT/DSR proof, and arms the proof-population shadow.
Program conversion checks literals through `kernel_internalize`, so moving it
before finalization may admit a literal that the completed input would retire;
moving executor configuration before finalization may change which state an
accepted request names. A transactional redesign needs a specified admission
phase, a validation view equivalent to post-finalization identities, and tests
for valid/invalid v1--v3 requests with and without attached proof. Until then,
do not claim invalid configure is side-effect free or make a one-line reorder.

For online learning more broadly, `tactix_solve` still constructs a fresh
stack-local `DefaultPolicy` via `default_solve` each synchronous call, whereas
the C session owns only the separate `OnlineInprocessExecutor` (`api.cpp`,
`default_policy.cpp:6160`). AM430 made borrowed policy/scheduler owners
noncopyable but did not create a live default-policy step boundary. Production
in-run learning therefore still needs (1) a session-owned policy lifetime,
(2) default-exact safe-point slots for pass dose/order, and (3) owned branching
and full reduction arms; the current reduction seam selects victims only after
default-owned candidate collection. A staged refactor should first retain
byte-identical default action/proof traces, then expose one owned switch at a
time, checking original-CNF proofs/models, N1 reachability, fuzz, and matched
performance after each cutover. This is an architecture recommendation from
source structure, not an implemented or measured improvement.

At the audit snapshot, current-source full Debug job `1723391` had passed
186/187 tests and was still running `demo3_scripted_default`. AM434's paired
AES extended-checker tasks `1723443_0/1` were both running pinned Trestle
checks. Neither is a terminal verdict. H2's canonical no-option DSR rejection,
N1--N4, full learning readiness, and the post-goal Tier-D submission remain
open. See AM436/437 for the independently pinned H2 diagnostics.
