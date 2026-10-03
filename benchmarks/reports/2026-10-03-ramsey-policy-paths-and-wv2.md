# Ramsey policy execution, replay and terminal WV2 collection

## Productive shared-policy coverage

Array `1747971` completed all three targeted cases using immutable Debug and
proof-shadow pin `pin-ramsey-policy-fb174175-debug-01`, bound to 832 source/support
files. It uses the current worktree snapshot, not HEAD alone. The inputs are the
previously certified original 19-vertex, signed/sparse 19-vertex and signed/sparse
18-vertex Ramsey encodings.

Each case executes configured DefaultPolicy and the actual RandomControllerPolicy
loop with proof disabled, with proof and feedback recording, and with proof and
deterministic replay. DefaultPolicy enables the explicit experimental Ramsey
profile before closing startup. Random selection uses a declared two-action
collection: the same Ramsey action with either zero certificate allowance or the
productive bounded profile. Seed 16 is found by a bounded selection-only control
to witness NoOp followed by productive UNSAT. This is targeted repeated-loop
coverage, not broad random fuzz or performance seed selection; ResumeSearch
remains in the real selector's domain.

All six executions per input return UNSAT through Ramsey without CDCL decisions
or conflicts. The twelve emitted certificates (two policies times record/replay
times three inputs) match the independently checked predecessor certificates
byte for byte. Proof/no-proof namespace, helper projection and logical-ID parity
hold. Debug proof shadowing reports no unbacked deletions. The random controller
transcript and both policies' complete deterministic feedback replay succeed.
No fresh external checker invocation is claimed by these equality checks.

The collector separately checks raw after-action counters: one default request,
two random requests, no proof additions on the random decline, and productive
recognition/certificate work, nodes, additions and helper counts on completion.
Default native feedback reports the explicit 20-million-work allowance and the
actual recognition-plus-preflight count; atomic emission is the subsequent
bounded replay, not included a second time in that counter. Reward remains
uncomputed.

Primary evidence is
`benchmarks/runs/20261003-ramsey-policy-paths/collection.json`, with exact
commands, certificate and trace hashes, source bindings, runners and the
reviewable `policy-paths.cpp` test snapshot. The CMake-discovered test remains in
the coupled unaccepted worktree as `tests/test_ramsey_policy_paths.cpp`.
Production profiles, scheduling acceptance, broader fuzz and full gates are
still owed. These tests do not establish nonterminal Ramsey deductions or
arbitrary live symmetry.

## WV2 is terminal on the older candidate

The maintained scorer now accepts all 89 rows: 48 model-checked SAT, 29 checked
UNSAT, two clean proof censors and ten retained solver timeouts; total completed
solver time is 4,101.655 seconds. Both censored proofs satisfy the bounded
comparison protocol. The two previously pending tasks were 14 and 51.
`wv2-score.json` owns the result and its baseline/scorer bindings.

Together with the prior B28+C5 and WV100 passes, this leaves Rank1–50's oddball
proof-cost regression as the failed performance gate on that older candidate.
No old campaign accepts the newer Ramsey source or a future enabled profile.
The Walk behavioral cause is documented; its production allowance decision is
still separate and unresolved.

## Full Debug preparation and next work

A complete build is running in `build/ramsey-public-dev`; only the earlier
focused executable set was built previously. The new full-suite pin will copy
all executables and CTest metadata, bind the exact compiled-source archive and
runtime support, and avoid interpreting later mutable source edits as changes
to an immutable binary. Repository documentation checks remain checks of the
repository at their execution time, not proof about future source. The older
225-test job remains running its last scripted-default case and cannot accept
this source.

Finish the full build, freeze and submit the complete suite, and hand the new
immutable snapshot to the Claude fuzz owner for both-policy/effective-action
coverage. Keep all44 targets, compact continuing-CDCL deductions and eventual
constructor retirement in scope. No source promotion or push is claimed.
