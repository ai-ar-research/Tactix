# Claude-owned public theory action v8 fuzz request

Own preparation, execution, validation, diagnosis, and terminal reporting for
the current public structured theory actions on the immutable sanitizer pin
that Codex will fill below. Follow `CLAUDE.md`, `DEVELOPMENT_PLAN.md` section
8.2, `.claude/skills/fuzz/SKILL.md`, and
`.claude/skills/slurm-jobs/SKILL.md`. This is soundness and reachability fuzzing,
not a performance benchmark.

Do not modify solver source, plans, calculus, general documentation, or frozen
reports; do not commit or push. You may extend `tools/fuzz/` as needed and use
the standard untracked fuzz stores. Publish the campaign's `STATUS.md`,
`FINAL_REPORT.md`, and any public reproducer summaries only in this directory.
Record the hash of every harness source before submission, and snapshot every
executed harness file through `tools/fuzz/slurm.sh`; no job may execute a
mutable live harness file.

This request supersedes
`benchmarks/runs/20261004-public-theory-fuzz-handoff/REQUEST.md` for the pinned
v8 campaign. Preserve its oracle, accounting, and failure-handling strength.
The expanded scope adds public X17 `NaryCspPrefix`, C API v8, typed replay, and
the opt-in native producer name `nary-csp`.

## Immutable solver and checker placeholders

Codex owns the solver, checker, and pin-manifest fields here and in
`MANIFEST.template.json`. Claude owns the harness snapshot, harness hash,
self-test, job, and result fields that only exist after it prepares the v8
harness. Claude must refuse a solver-leg submission while any field required
by that leg remains unresolved.

- Source commit: `fecd60cf4ceb1dcbc6346b85e0f90352b1068487`
- Solver: `/home/hwu/SAT-solving/build/pin-public-theory-v8-fecd60cf-sanitize-02/tactix`
- Solver SHA-256: `f0878952ef3ec8b30ef5fb7b110f11f854cb872c4d14a8f49955f08076281bdd`
- Pin manifest: `/home/hwu/SAT-solving/build/pin-public-theory-v8-fecd60cf-sanitize-02/PIN.json`
- Pin manifest SHA-256: `7b157e22b334cc6dab513301286b685374d38bb9580b22c622ae2cadc0e0bf1c`
- Build details: `GNU 13.1.0; CMake Debug; CMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer; TACTIX_ENABLE_EVENT_TRACE=OFF; TACTIX_ENABLE_RULE_JOURNAL=OFF; TACTIX_ENABLE_PROOF_SHADOW=OFF`
- Required runtime sanitizer environment: `ASAN_OPTIONS=detect_leaks=0; UBSAN_OPTIONS=print_stacktrace=1`
- Frozen checker collection:
  `/home/hwu/SAT-solving/tools/fuzz/failures/checkers/895f1989468d5e01a81fa647f9cb9739448b81507a4b78515e4e46a7441c44f8`
- Checker collection SHA-256 / content ID: `895f1989468d5e01a81fa647f9cb9739448b81507a4b78515e4e46a7441c44f8`

The expected build is a fresh Debug ASan+UBSan build with frame pointers. State
the exact compiler, CMake options, sanitizer flags, and any justified runtime
exception such as `ASAN_OPTIONS=detect_leaks=0`; the placeholder is not itself
authorization to assume those details. Verify the source identity, executable
hash, build manifest, checker manifest, and checker hashes before and after
every leg. The source checkout may move after handoff; the immutable solver,
copied harness snapshot, checker collection, and recorded hashes are
authoritative.

## Cluster contract

Run every solver fuzz leg through SLURM on `cpu-q`. Submit every array at full
width: no `%N` array-task throttle, no runner `--throttle`, and no node or
`--nodelist` constraint. Let SLURM place all eligible tasks. Record the exact
`sbatch` command, parsable job ID, submission time, array shape, requested
CPUs/memory/time, log path, harness snapshot hash, and terminal state. A job is
successful only when its expected artifacts and independent validations exist.
Do not use `srun` from the session and do not infer success from disappearance
from `squeue`.

## Required action matrix

The acceptance matrix contains exactly these six canonical action families:

| Catalog/API | Canonical family | Discovery route |
|---|---|---|
| X12 / v8-compatible | `Gf3AffineConsequence` | `gf3-affine` native producer |
| X13 / v8-compatible | `DirectCspSupport` | `direct-csp` native producer |
| X17 / v8 | `NaryCspPrefix` | `nary-csp` native producer |
| X15 / v8-compatible | `DirectPhpRefutation` | public native semantic instance enumeration |
| X14 / v8-compatible | `Gf3GaussianConsequence` | `gf3-gaussian` native producer |
| X16 / v8-compatible | `TseitinCycleBreaking` | `tseitin-cycle` native producer |

For every family, separately prove all of the following from solver-owned
counters/traces and independently checked artifacts:

1. the producer or DirectPHP semantic enumerator was called;
2. at least one exact typed offer was emitted on productive fixtures;
3. the controller selected and applied that offered action;
4. the application made an effective solver-state change (or terminal UNSAT
   for DirectPHP), rather than a NoOp or request-only event;
5. the terminal SAT model or UNSAT proof was accepted against the original CNF;
6. seeded record/replay reproduced the request, identity, producer evidence,
   action result, effective counter, verdict, and checker outcome.

`c controller-rule` request/application rows do not prove discovery.
`c controller-producer` calls/candidates/offers rows are required for the five
opt-in producers. DirectPHP must have equally explicit solver-owned evidence
for enumerator call and recognized offer. If the pinned solver exposes no such
DirectPHP source, report the gap and leave that family incomplete; do not infer
an offer from a request, attempt, verdict, or harness-side choice. Missing
counters, a NoOp, a timeout, an unchecked verdict, or a replay that merely
re-solves receives no reach credit.

## Required campaign legs

Run and report these legs separately. Never combine their totals.

### 1. DefaultPolicy compatibility control

Run the maintained regression corpus and at least 10,000 fresh generated cases
with the unmodified DefaultPolicy and canonical DSR. Require independent
original-CNF model checks for every SAT result and the maintained native DSR
route for every UNSAT result. Run the structured fixture matrix below under
DefaultPolicy as an additional control.

All five opt-in native producers and all six action families must have zero
producer calls/offers, attempts, applications, and effective transitions in
this control. Report non-terminating cases separately; they receive no semantic
oracle credit.

### 2. Controller-random broad control

Run the maintained regression corpus and at least 10,000 fresh generated cases
with a per-case seeded restart-controller policy,
`--controller-native-producers=all`, canonical DSR, and the same independent
oracles. This must use `--controller-policy`, never legacy `--random-policy`.

Report controller rule, search, loop, and per-family discovery coverage,
including calls, bounded work, candidates, offers, applications, effective
transitions, decline statuses, and terminal verdicts. Broad random CNFs are
compatibility evidence. Do not claim theory-action reach from this leg unless
the full six-item causal chain above is present for the relevant family.

### 3. Targeted structure-preserving theory legs

Generalize the maintained controller-v5 harness into one versioned v8 harness.
Run six separately attributable deterministic legs, at least 2,000 cases per
family (at least 12,000 total). For each family generate approximately 1,500
productive fixtures and 500 close negative/decline controls. Use
`--controller-families=<canonical-family>` and enable only the matching native
producer for the five producer-backed families. Do not inject static action
payloads for those families. DirectPHP must use its public semantic instance
enumeration rather than a fabricated payload. Search deterministic controller
seeds when needed so a real offer is selected before an early `ResumeSearch`;
record the seed-selection method and never change solver semantics to force
reach.

Each productive case must show producer/enumerator call, offer, application,
effective state change, checked terminal verdict, and replay. Require at least
1,500 independently checked effective cases per family. Each negative control
must show bounded discovery work, zero offer and zero effective mutation for
the deliberately damaged structure, followed by an independently checked
terminal verdict. Require at least 500 checked declines per family. If a
family's generator yields an accidental valid structure, classify it by the
observed causal chain rather than its intended bucket and replace the lost
negative case.

## Generator specifications

All generated CNFs must be deterministic from a recorded case seed. Start from
an authenticated positive object, apply semantics-preserving permutations of
variables, literals where the theory permits signed domains, clauses, equation
or tuple order, and disjoint noise that the action's declared source boundary
allows. Derive each near miss by one recorded mutation from the corresponding
positive object. The generator must independently compute the expected SAT/
UNSAT class (by exhaustive evaluation for small instances or a separate trusted
construction) before invoking Tactix.

### `Gf3AffineConsequence`

Retain the v5 complete ternary one-hot domains and complete forbidden-tuple
encodings. Productive cases contain two authenticated GF(3) equations sharing
a nonzero pivot domain, with a nontrivial resultant whose forbidden-tuple CNF
is not already present. Vary coefficients, right-hand sides, domain order,
signed external identities, and SAT versus minimally inconsistent surrounding
systems without creating a root unit. Independently derive the resultant and
verify every published clause is entailed.

For close declines, delete or alter exactly one required ALO/AMO or forbidden
tuple premise in the otherwise complete image, or make every equation pair
non-eliminating. Require bounded producer work, zero typed offers, zero action
application/effect, and a checked terminal verdict.

### `DirectCspSupport`

Retain the v5 exact ordered pair of disjoint signed one-hot domains and exact
cross-domain forbidden-pair table. Productive rows must have a nonempty proper
support whose support consequence is absent. Cover both direct clauses and
wide-support exact-OR extension publication, SAT and UNSAT surroundings, and
left/right orientation. Independently enumerate the table to validate the
support clause and the unique projection of any introduced helper.

For close declines, remove or corrupt exactly one domain cover/AMO premise,
overlap the two domains, or add an unclassifiable source occurrence touching
the selected domains. Ensure the mutation leaves no alternative valid domain
pair. Require bounded discovery, zero offers/effect, and a checked verdict.

### `NaryCspPrefix` (X17, C v8)

Generate at least three pairwise-disjoint complete signed one-hot domains and a
sparse strictly lexicographic n-ary forbidden-tuple relation. Use both arity 3
and the supported higher arities within the pinned limits, varying domain
sizes, signed external identities, tuple density, residual-domain choice, and
clause/literal order. Productive cases must include:

- partial residual exclusions, where an exact-AND prefix helper and at least
  one `(~p or ~v)` consequence are new;
- complete residual exclusions, where all residual values extend the same
  prefix to forbidden tuples and the action additionally publishes `~p`;
- multiple offers from one relation, deterministic pagination, and a second
  distinct prefix after the first commit so retained-owner revalidation and
  committed-key suppression are exercised.

Independently enumerate assignments to verify each forbidden tuple, exact-AND
definition, residual exclusion, helper unit, and model projection. Require the
native `nary-csp` producer and exact C v8/replay identity; an old v5 scalar or
static payload path receives no credit.

For close declines, remove exactly one ALO or one AMO edge from every candidate
domain collection, introduce one overlapping domain variable, or make the
tuple image malformed while keeping the rest of the relation intact. Ensure
there is no accidental alternative collection of the configured arity.
Require bounded `NaryCspPrefix` producer work, zero offers/effect, and a checked
verdict.

### `DirectPhpRefutation`

Generate exact small `PHP(H+1,H)` instances across multiple `H`, with canonical
pigeon cover clauses, hole collision clauses, variable renaming, and clause/
literal permutations. Productive cases must show the public semantic enumerator
discovering the exact instance, one recognized offer, terminal action
application, terminal UNSAT, and an independently accepted proof against the
original CNF.

Near-layout declines remove or alter exactly one pigeon cover or collision
premise, add an occurrence that violates exact ownership, or create an
ambiguous mapping while keeping the instance visually close. Use small sizes
whose mutated formula can be independently classified, including SAT near
misses. Require an enumerator call with bounded work, zero recognized offers,
zero effective action, and a checked terminal verdict.

### `Gf3GaussianConsequence`

Generate complete authenticated ternary-domain GF(3) systems whose exact source
is consumed by the bridge. Include chained closures that atomically publish
multiple supported units and separate inconsistent closures that publish the
empty conflict. Vary rank, equation order, coefficients, right-hand sides,
derivation depth, and source permutations while remaining within the bounded
certificate domain. Independently replay the Gaussian algebra and every RUP
consequence.

For close declines, delete or alter exactly one authenticated tuple/domain
premise, add an unconsumed source clause involving theory variables, or produce
only unsupported nonunit reduced rows. Require bounded producer work, zero
offers/effect, and a checked verdict.

### `TseitinCycleBreaking`

Generate authenticated odd-charge connected XOR components of admitted widths
2--4, with one and two components, variable/clause permutations, root-fixed
off-cycle edges, and alternate available cycles. Productive coverage must
include a sequence where X16 is effective, a separately selected `Propagate`
settles its unit, and a fresh second root census offers and applies another
eligible component or alternate cycle. Independently verify parity semantics,
the published unit, and the original-CNF terminal result.

Close declines must cover even charge, bad incidence/degree, ambiguous XOR
admission, and one unclaimed stored clause touching a component edge. Require
bounded producer work, zero offers/effect, and a checked verdict. Exact-identity
tampering belongs additionally in replay-negative coverage below.

## Replay, provenance, and checker requirements

For every family, retain at least one SAT and one UNSAT positive fixture when
the family admits both; DirectPHP is necessarily UNSAT. Record a trace and
replay it with the same pin, harness snapshot, checker collection, family
restriction, native-producer selection, seed, input, and typed identity.
Compare normalized traces, producer/enumerator records, application status,
effective counters, verdict, model/proof result, and provenance identity.

Add negative replay controls for changed producer selection, changed family
restriction, changed source state, changed typed payload or identity, truncated
trace, and Nary C v8 payload corruption (domain order, overlap, residual
ordinal, prefix coordinate, and limit/profile mismatch). Each must fail closed
for the intended reason without solver mutation.

Parse and validate every structural-provenance JSONL record. At minimum bind
the canonical action and semantic IDs, source formula identities and native
proof IDs, typed selection/limits, producer or enumeration evidence, exact
premises, planned and emitted proof events, installed clauses or terminal
certificate, extension/reconstruction projection, work/resource accounting,
and committed session/replay identity promised by that family's current public
interface. Cross-check recorded source clauses against the original CNF and
recorded additions against the proof.

Every SAT verdict requires the independent original-CNF model checker. Every
UNSAT verdict requires the frozen canonical native DSR route, including the
maintained every-lemma audits. Differential solvers may supplement these
oracles but cannot replace them. A proof checked only against a transformed
formula receives no credit.

## Harness and preflight requirements

- Extend `fuzz.controller_coverage` and its tests to parse and aggregate
  producer/enumerator calls, work, candidates, offers, applications, effective
  transitions, decline statuses, and replay identity for all six families
  without weakening rule/search/loop parsing.
- Provide one clear v8 `run.sh` entry point and synchronize the harness README.
- Snapshot every new script, generator, fixture, control, corpus input, and
  configuration used by a job. Record both individual hashes and the aggregate
  harness hash.
- Run `tools/fuzz/selftest.sh` against the frozen checker collection and pinned
  sanitizer Tactix before any campaign submission because acceptance logic is
  changing. Known-bad model, proof, truncation, every-lemma, counter, replay,
  and provenance controls must fail for their intended reasons.
- Run a small fixed preflight for each positive and decline generator and prove
  its expected offer/effect classification before submitting the full arrays.
- Validate aggregate accounting separately per leg and family:
  `requested = independently checked effective + independently checked
  declines + explicitly listed non-terminating + failures`. A timeout is never
  a pass.

## Failure handling and terminal report

Maintain a concise `STATUS.md` in this directory with the frozen matrix,
placeholder substitutions, source/harness/checker hashes, preflight results,
job IDs, log paths, and terminal states. Do not expose private chain-of-thought.

On a solver, sanitizer, invariant, model, proof, replay, provenance, or
accounting failure, stop the affected leg after preserving one bounded original
artifact and one minimized reproducer for each distinct failure class. Write a
public `REPRODUCER-<family>-<class>.md` here with the exact command, immutable
paths/hashes, seed, expected/observed behavior, and failure class so Codex can
debug it. Continue independent unaffected legs when sound. Do not patch solver
source.

Finish with `FINAL_REPORT.md` in this directory. It must disposition every
requirement, show required versus achieved counts for every family and causal
stage, list all gaps and non-terminating cases, and distinguish compatibility
evidence from effective action evidence. Do not launch target, WV, Rank,
B28+C5, Tier-D, or any other performance campaign from this handoff.
