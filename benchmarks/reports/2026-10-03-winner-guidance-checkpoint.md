# Winner integration: unaccepted guidance and graph-search checkpoint

This is a development checkpoint, **not an accepted solver increment**. All 44
portfolio targets remain open. Accepted production source remains `375b621b`.
The [snapshot manifest](../runs/20261003-winner-guidance-checkpoint/manifest.json)
binds the 43-file uncommitted patch, focused logs and microcontrol outputs to
base `043d2202`. The patch passed reverse-application checking against the
worktree. It excludes PROGRESS and this evidence, and contains new files as
well as tracked-file edits. Keep it as recovery/provenance, not a build pin.

## Implemented work in the snapshot

- Candidate identities bind exact publications with external-variable mapping,
  formula provenance, producer objective and scoped effort. Generation is
  separate from resettable statistics and saturates without identity reuse.
  Partial signed assignments preserve unknown auxiliary values. Malformed or
  wholly unspecified publications leave the previous candidate intact.
- Target import preserves saved/best phases and allows native target capture to
  replace advice. Walk imports privately, compares candidate and ordinary
  assignment on the same current walkable CNF with charged preparation, and
  exports only a strict improvement. A stale reference, incomplete preparation
  or absent walkable objective leaves phase memory intact. Root/watch cleanup
  may already have happened; there is no rollback.
- Default Walk slots select exact candidates; random selection and pure
  observation expose them. Typed C/C++ requests, `walk2` text round trips and
  after-action records carry identities. Action-space version is 2; existing
  feedback/transcript envelope versions remain 1. The owning contract is
  [controller-interface.md](../../docs/controller-interface.md).
- Internal graph helpers recognize indexed signed column covers using stable
  external identities, infer the reference graph/separation projection, build
  all 24 degree/BFS ordering seeds and perform bounded iterative window DFS.
  They never mutate Kernel/proof state. Recognition rejects unsupported
  layouts; a graph score is not a Boolean certificate. These helpers are not
  yet callable through Lucky, DefaultPolicy or random selection.

The DFS uses the reference remaining-degree/recent-window ordering key,
simplified algebraically to avoid repeated neighbor counts. Random ties are
stable in shuffled order. Metered insertion sorting and explicit frames replace
uninterruptible comparators and recursion. These choices have small-graph
correctness evidence only, not reference runtime or scheduling-parity acceptance.

## Verification and limitations

The mutable Debug build `build/dev-winner-guidance-debug` compiled the CLI and
focused executables with GCC 13.1.0 and proof shadow enabled. Twelve focused
CTest entries passed in 19.71 seconds: Walk, C online API, feedback inspector,
feedback trace, graph recognition, graph search, local-search guidance, online
inprocessing, policy feedback, random controller, random online program and
restart controller. The complete output is retained in `focused-tests.log`.
These are fixed local microtests, not benchmark experiments or a full Debug gate.

Graph tests cover signed rectangles, malformed columns, retired identities,
compaction, recognition storage limits and every smaller work allowance for the
fixture. All 64 simple four-vertex graphs were checked: construction produces
scored permutations; uncapped window search agrees with an independent
brute-force permutation oracle. Early stops preserve only completed candidates.
Guidance tests cover unknown polarities, identity lifetime/saturation, root and
retired variables, target supersession, preparation boundaries and stale C API
requests. A first recognition test failed because it wrote a retirement flag
into a status vector that compaction had cleared. The fixture now reestablishes
status storage and resolves the external identity before marking it retired;
the corrected test passes. This was not a solver failure.

Fourteen native fixed microcontrol signatures were compared with the retained
accepted-build output. Thirteen are byte-identical across verdict, proof hash,
work, state, phase and score fields. Only `profile=6 pigeons=6`, the selected
structural Walk profile, changes as intended. This establishes correspondence
only for these microcases; it is neither universal equivalence nor performance
evidence. No historical benchmark campaign was rerun.

Documentation checks passed for section references (77), rule inventory (30),
proof-guide synchronization, effort specifications, default configuration,
reference-option coverage and citations (45 resolved; 200 ambiguous basenames
remain skipped). The feedback inventory includes 442 native fields. The wiring
checker initially rejected two new fields and its obsolete two-field Walk
request binding. Its updated explicit selection/install/restore bindings pass,
and 264/264 missing-binding negative controls are rejected. Size checks have
zero errors and seven existing low-reserve warnings; no compaction was performed.

Some incremental Make invocations reported sub-second dependency-file clock
skew. Successful exits and focused results are retained, but this mutable build
is not an immutable source/binary acceptance pin. Full Debug, independent
original-CNF model/proof validation, Claude-owned both-policy fuzz, effective
reach/replay and all four regression gates remain required. No new target
closure or N1–N4 pass is claimed. No solver/checker/fuzz campaign was submitted;
queue metadata showed only unrelated interactive allocation `1747502`.

## Next complete increment

Finish graph pair-swap repair/annealing and metered private Boolean completion,
validate whole live/root assignments and reconstructed original-CNF models, and
publish useful partial candidates when construction cannot finish. Add the
complete typed Lucky action, public/C serialization and observation/feedback,
and shared default/random invocation. Resolve native/online decline-status
parity and source-backed default effort/scheduling profiles before acceptance.
Internal helpers or consumer APIs alone do not complete the graph increment.
Then use immutable pins for independent oracles, both-policy fuzz, full Debug,
B28+C5, Rank1–50, WV100 and WV2–89, preserving the bounded-checking protocol.
Symmetry, hypergraph construction, Ramsey certification and certificate-cost
investigation remain in scope. No accepted scope, target or gate is weakened.
