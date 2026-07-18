---
name: stress-policy
description: Write a TEMPORARY policy that drives a specific N1 schedule (a proof-rule sequence, an off-fixpoint state, a new rule's interactions) aggressively, then fuzz it to verify soundness+proofs survive that schedule. Use whenever a new rule/feature lands, a guard is purified (a removed precondition opens new schedules), or a specific rule interaction is suspected — the generic `--random-policy` samples such patterns too rarely to trust.
---

# Stress-policy: targeted N1 schedule verification

The project's core guarantee is **N1** — *any* legal rule order/instance is sound
and emits a valid proof (plan §1.1, §5). `DefaultPolicy` exercises one fixed
schedule; `--random-policy` (RandomPolicy) exercises *uniformly random* legal
schedules. But a rare-but-legal pattern — e.g. "decide off a propagation
fixpoint", "apply rule X immediately after rule Y", "run the inprocessing slot
with no M1 between rules" — is drawn so rarely by the uniform policy that it is
effectively untested. When a feature makes such a pattern legal, **write a
temporary policy that maximizes it**, and fuzz that.

This has already paid off twice: the `AdversarialPolicy` (off-fixpoint-decide
stress) reproduced D-LEVEL-1 deterministically when `--random-policy` gave 0/1000
(the uniform policy almost never decides off a fixpoint); the same off-fixpoint
inprocessing stress surfaced the D-LEVEL-inproc unsoundness. See
[[n1-handoff-hazard-family]] — this whole bug family is "a free schedule breaks a
precondition DefaultPolicy meets by accident," and a targeted policy is how you
provoke it.

**Run this skill whenever a rule/feature lands, whenever you purify a guard**
(removing a conjunct legalizes schedules that were previously impossible), or
whenever you suspect a specific interaction. It complements the `fuzz` skill
(which *runs* the harness); here you *build the driver* the harness feeds.

## Worked reference

`src/policies/adversarial_policy.{hpp,cpp}` is the committed, worked example
(off-fixpoint-decide stress). `src/policies/random_policy.cpp` is the generic
baseline. Read both before writing a new one — copy their shape.

## Procedure

### 1. Name the risky pattern precisely
What schedule/state does the new feature make legal that DefaultPolicy never
produces? Write it down as a concrete rule-application pattern. Examples:
- *off-fixpoint decide* — `Decide` while `propagate_head < |τ|` (D-LEVEL-1).
- *off-fixpoint inprocessing* — an inprocessing rule with no M1 drain before it.
- *interposed rule* — apply Restart/Reduce between two halves of a sequence.
- *canonical-batch after batch* — two whole-round inprocessing applies back to
  back with no propagation between.

### 2. Write a temporary policy that MAXIMIZES that pattern
File pair `src/policies/<name>_policy.{hpp,cpp}` mirroring `adversarial_policy`.
Requirements — it must:
- **Drive the pattern hard** (bias every choice toward the risky action) *while
  still reaching verdicts* — the semantic oracles (differential/model/proof) only
  fire on SAT/UNSAT, so a policy that never terminates tests only the invariant
  checker. Mix in enough normal progress (resolve conflicts via
  `ConflictAnalysis`, reach SAT/UNSAT) that a good fraction of cases finish.
- **Only ever apply a rule whose `guard` holds** (that is the N1 contract; a
  guard-violating apply is not a bug in the solver, it is a bug in your policy).
  Draw among applicable rules / legal instances, exactly like RandomPolicy.
- **Be deterministic per seed** (xorshift on a seed arg; no wall-clock/rand).
- **Check invariants PER STEP**, not every-256: `kernel_check_invariants(k)`
  after every apply. The every-256 default hides bugs whose state is repaired
  before the next check (it is why `--random-policy` under-reports). Gate an
  optional per-step trace (rule name + level/trail/head/flags) on an env var
  (`TACTIX_TRACE`) — that trace + a shrunk `min.cnf` is what pins the mechanism
  in minutes (see [[n1-handoff-hazard-family]]).
- Expose knobs via **env vars** (e.g. `TACTIX_ADV_BURST`) so the fuzz driver can
  sweep aggressiveness without a rebuild.

### 3. Wire it in
- Add `src/policies/<name>_policy.cpp` to `CMakeLists.txt` (the `tactix_lib`
  OBJECT list).
- Add a `--<name>-policy[=SEED]` flag in `src/main.cpp` (copy the
  `--adversarial-policy` branch: set a bool, parse an optional `=SEED`, dispatch
  to `<name>_solve` in the `res =` ternary).

### 4. Fuzz it — the four oracles, both proof formats (see the `fuzz` skill)
Build `debug` (ASan/UBSan, assertions on) in an **isolated worktree** (never
rebuild a `build/debug` a nightly uses — see [[working-style]]). Then, with the
oracle pointed at the real kissat/checker bins (symlink them into the worktree):
```
tools/fuzz/run.sh --quick --solver "build/debug/tactix --<name>-policy" --oracle .../kissat --n 800 --max-vars 60
tools/fuzz/run.sh --quick --solver "build/debug/tactix --<name>-policy --lrat" --oracle .../kissat --n 800 --max-vars 50
tools/fuzz/fuzz.py corpus --solver "build/debug/tactix --<name>-policy" --oracle .../kissat
```
Both DRAT and native LRAT are mandatory (they catch different bugs — `fuzz`
skill). 0 failures on every leg is the bar. Watch the `verdicts:` line: a wall of
TIMEOUTs means the policy isn't reaching verdicts — tune it to finish more cases.

### 5. On a failure
The bundle's `min.cnf` + the recorded seed reproduce it deterministically. Re-run
that CNF with `TACTIX_TRACE=1` and read the last steps before the abort/mismatch
— the per-step trace names the culprit rule and state. Then ask *which guard let
an illegal order happen, or which rule's implementation assumed a precondition
the schedule broke* (the fix is a tightened guard OR a kernel/rule repair — never
a patched symptom; see [[n1-handoff-hazard-family]]). Add the shrunk case to
`tools/fuzz/corpus/regressions/` if it is a distinct mechanism.

### 6. Keep or remove
- **Keep** the policy (commit it) if the pattern is a durable N1 stress axis
  worth re-running — like `AdversarialPolicy`. Note it in the `fuzz` skill's leg
  list if it should join routine testing.
- **Remove** it (and its flag/CMake line) if it was a one-off to validate a
  single feature and the pattern is now covered by a kept policy or the corpus.
Either way, the *finding* (mechanism + fix) is recorded in the commit + the
relevant memory, not just the throwaway code.

## When a feature lands — the standing rule (plan §5.3, §8.2)

Adding or changing a rule, or purifying a guard, is **not done** until the
schedules it newly legalizes are stress-tested this way, not merely covered by
the uniform `--random-policy`. If the uniform policy already saturates the new
pattern (rare), say so with the measured draw rate; otherwise write the targeted
policy. This is the N1 analogue of "a rule doesn't merge without its proof-delta."
