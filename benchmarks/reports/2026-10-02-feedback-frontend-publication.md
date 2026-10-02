# Both-policy frontend feedback and phase-publication checkpoint

The CLI now records or verifies common feedback for DefaultPolicy and the
controller-driven random policy. Native inprocessing delivery also waits for
full phase publication and scope restoration. Two concrete gaps in the prior
partial checkpoint are corrected: metadata added interruption-fuzz polls, and
slot delivery could precede enclosing native publication/cleanup. This remains
partial shared-policy preparation, not fresh whole-goal acceptance. Stable
architecture and gates remain in DEVELOPMENT_PLAN.md §§5.6/9.4; concrete
contracts belong to docs/controller-interface.md.

## Frontend and purity changes

`--feedback=FILE` records feedback-v1; `--feedback-replay=FILE` verifies the
chosen policy's regenerated deterministic events. Both work with either policy
and independently of controller transcripts. RandomControllerPolicy exposes
the same non-owning receiver replacement setter as DefaultPolicy.

Provenance contains SHA-256 identities of the running executable and original
input file bytes, including compressed containers, plus policy/seed, base
configuration, proof mode/presence, BVE gates and declared solver environment
settings. Source-literal inventory and per-rule cursor/order keys retain
environment overrides without scanning unrelated environment values. Builds
and inputs must remain immutable; argument output paths are excluded from
identity. Local streaming SHA uses no external process/dependency and is tested
against empty, `abc` and million-byte vectors, fragmented input, binary files,
pure repeated finishing, missing/directory inputs and Python's independent
hashes of actual CLI builds/inputs. The environment inventory is source-audited.

Aliased input/proof/transcript paths and incompatible policy/feedback flags fail
before opening outputs. Feedback setup, header, opening, write or replay errors
do not steer solving: the selected policy completes, emits its actual verdict
and reconstructed model/proof, then the frontend reports the feedback error
and exits 1. Ordinary successful solver exit codes are retained. Disabled
frontend feedback constructs neither the receiver nor provenance hashes.

The previous native outcome mapper called `kernel_terminating()`. That const
query intentionally mutates interruption-fuzz state/counters, so enabled
feedback changed the poll sequence. The mapper now reads external stop state;
completion infers native interruption events from counter changes made by the
application itself, preserving terminal/conflict/rejection precedence and the
original return value. A retained control compiles the prior helper body,
renaming only its function: three description reads fire three fuzz stops and
change RNG state, while the current helper fires zero and leaves it unchanged.
Actual native-poll fixtures compare feedback-on/off state, counters and return
values and cover terminal priority. Feedback descriptions contain no extra
termination poll.

## Publication completion

Some heuristic publication occurs after a slot returns; productive Sweep can
delay its import-ordered retirement union until a followup or phase exit.
Slot-level delivery therefore did not establish fully restored phase state.
The native phase now retains immutable leaf records until its existing exit
and occurrence/context-scope destruction finish. `InprocessPhaseExit` describes
native redundant-retirement flush, retirement publication, occurrence closure,
watch reconnection and sparse propagation using existing shared mechanisms;
native scheduling/bookkeeping remains in DefaultPolicy. Measurements and leaf
snapshots stay at their original completion points. Publication order, rule
calls, clocks, budgets and native return values are preserved.

New receiver assertions check cleared factor handoff, released phase occurrence
cache and restored inprocessing context, in addition to work/phase settings,
watches and pending/speculative state. Linking the current fixture against
unadapted preserved `a4cfabe3` DefaultPolicy rejects its earlier slot delivery
at exactly that phase-lifetime assertion. The current positive fixture passes.
The existing framework snapshots each leaf independently; delivery can remain
deferred through a whole phase. Its memory/throughput cost still needs the
goal's overhead acceptance, not an assumed zero-cost claim.

## Verification

Normal and journal/event-trace Debug each pass fourteen focused checks:
`scheduling`, `inspect_policy_feedback`, `feedback_frontend`, `solve_reset`,
`controller_execution_signature`, `default_execution_signature`,
`feedback_provenance`, `feedback_trace`, `native_policy_feedback`,
`online_inprocess`, `policy_feedback`, `preparation_execution`,
`sequence_followups` and `substitute_publication`. Terminal totals are 128.42
and 134.04 seconds; frontend controls take 100.02 and 105.78 seconds.
These include repeated launches hashing large Debug binaries, and concurrent
comparison compilation. They are not instrumentation-overhead measurements.

Fixed frontend cases compare disabled, recorded and replayed verdict/stdout
and certificate bytes for both policies on SAT/UNSAT inputs. Each build checks
two proofs independently through drat-trim/CakeLPR and two original-CNF models.
There are two distinct inputs and four policy/input combinations, repeated
across builds. Controls cover metadata/configuration/proof changes, missing
files, truncation, trailing records, action divergence, timing-only changes,
relocated identical inputs, gzip/xz sources, symlink/hardlink/output aliases,
independent controller-transcript replay and deterministic interruption modes.
Corrupted runtime streams preserve exact solver output/certificate bytes;
opening/header failures also retain solver completion. Existing lifecycle,
observation and both-policy feedback purity/replay tests remain green.

Preserved `a4cfabe3` DefaultPolicy source and its two original policy/scheduler
headers compile without adaptation; only that translation unit is replaced in
the comparison binary. Native machinery and the fixed harness are shared.
Ten pigeonhole fixtures (five profiles, each SAT and UNSAT) match preserved,
current feedback-off and feedback-on signatures. All 25 normal or 45
diagnostic artifacts per arm are byte-identical, including DRAT, reconstructed
models and exact journals/event traces. Per build, five proofs and five models
verify independently and ten feedback streams pass strict inspection. These
are ten distinct fixtures repeated across builds, not twenty benchmark cases.
Each build now records 4,592 actions and 115 search intervals, including six
phase exits. Counts do not establish full effective action/configuration reach.

Thirteen documentation/configuration/boundary gates pass (21.03 seconds).
The queue-backed progress check reports zero errors/warnings. Accepted
foundation pins and baseline digests remain unchanged. Companion
[validation](2026-10-02-feedback-frontend-publication/validation.json) retains
source/build digests, terminal logs, control evidence, signatures, independent
oracles and inspection summaries. Retained reproduction scripts are copied
to `build/` and take the selected mutable build path; the comparison accepts
an additional `diagnostic` argument.

## Remaining work

Reconcile all 28 operation groups and finish remaining parameter,
representation, occurrence/publication completion boundaries and duplicate-body
removal. Broad preserved-default correspondence, fresh full Debug, both
Claude-owned fresh 100,000-seed legs plus corpus/targeted effective reach,
all four DefaultPolicy regression sets and measured enabled/disabled overhead
remain required. SHA provenance adds optional per-run setup work; whole-phase
record retention adds enabled memory cost. Neither is performance-accepted
by these fixtures. The old foundation acceptance remains a comparison authority.

No SLURM campaign or real-instance experiment was submitted. The only live job
observed was unrelated interactive allocation 1745040, left untouched. Frozen
evidence/pins and unrelated work are preserved; source changes remain local
with no push. No policy fallback, rollback, forced search resumption, global
loop budget, scalar reward or learning algorithm was introduced.
