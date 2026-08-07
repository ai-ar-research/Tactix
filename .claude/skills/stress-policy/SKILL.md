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

The whole bug family is **"a free schedule breaks a precondition DefaultPolicy
meets by accident."** A targeted policy is how you provoke it: `AdversarialPolicy`
(off-fixpoint-decide stress) reproduced D-LEVEL-1 deterministically where
`--random-policy` gave 0/1000, and the same off-fixpoint inprocessing stress
surfaced the D-LEVEL-inproc unsoundness. Every witness-emitting rule family now
has one, and they are kept, not thrown away.

**Run this skill whenever a rule/feature lands, whenever you purify a guard**
(removing a conjunct legalizes schedules that were previously impossible), or
whenever you suspect a specific interaction. It complements the `fuzz` skill
(which *runs* the harness); here you *build the driver* the harness feeds.

## Worked references

`src/policies/adversarial_policy.{hpp,cpp}` is the original worked example
(off-fixpoint-decide stress); `src/policies/random_policy.cpp` is the generic
baseline; `src/policies/stress_common.hpp` holds the shared scaffolding. The
per-family drivers (`bce`, `bve`, `cce`, `congruence`, `factor`, `probing`,
`reuse`, `substitute`, `sweep`, `walk`) are the working set — read the one
closest to your rule and copy its shape.

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
  in minutes.
- Expose knobs via **env vars** (e.g. `TACTIX_ADV_BURST`) so the fuzz driver can
  sweep aggressiveness without a rebuild.

### 3. Wire it in — ONE row, not four edits to `main`
- Add `src/policies/<name>_stress_policy.cpp` to `CMakeLists.txt` (the
  `tactix_lib` OBJECT list).
- Add **one row** to `ROWS[]` in `src/policies/stress_registry.cpp`
  (`{"--<name>-stress-policy", <name>_stress_solve}`) and include its header
  there. `main` already has a single parse branch + single dispatch for the
  table. The nine policies that predate the table still carry their own
  branches in `main`; do **not** copy that shape for a new one.

### 4. Fuzz it — the four oracles, both proof formats (see the `fuzz` skill)
Build `debug` (ASan/UBSan, assertions on) in an **isolated worktree** — never
rebuild a `build/debug` that a running nightly uses. Then, with the
oracle pointed at the real kissat/checker bins (symlink them into the worktree):
```
tools/fuzz/run.sh --quick --solver "build/debug/tactix --<name>-policy" --oracle .../kissat --n 800 --max-vars 60
tools/fuzz/run.sh --quick --solver "build/debug/tactix --<name>-policy --lrat" --oracle .../kissat --n 800 --max-vars 50
tools/fuzz/fuzz.py corpus --solver "build/debug/tactix --<name>-policy" --oracle .../kissat
```
Both DRAT and native LRAT are mandatory (they catch different bugs — `fuzz`
skill). 0 failures on every leg is the bar.

**0 failures is only evidence if the leg could have failed.** A stress policy
that never fires the pattern looks exactly like a pattern nothing can break, and
that reading has been written up as evidence here before. Two checks, both
required before banking a green leg:
- **Count at the SOURCE** — a fire counter inside the predicate the policy is
  driving (not at the call sites, so reachability doesn't depend on each site
  remembering to increment), and require a NON-ZERO count. Have the stress mode
  bypass any production throughput cadence: a `!(++tick & 255)` poll gate means
  anything smaller than one poll window never reaches a first check, and fuzz
  CNFs are tiny.
- **Compare the `verdicts:` line against the control leg** — near-identical
  SAT/UNSAT/TIMEOUT counts mean the path never executed. A wall of TIMEOUTs
  means the policy isn't reaching verdicts at all; tune it to finish more cases.

### 5. On a failure
The bundle's `min.cnf` + the recorded seed reproduce it deterministically. Re-run
that CNF with `TACTIX_TRACE=1` and read the last steps before the abort/mismatch
— the per-step trace names the culprit rule and state. Then ask *which guard let
an illegal order happen, or which rule's implementation assumed a precondition
the schedule broke*: the fix is a tightened guard OR a kernel/rule repair, never
a patched symptom. Add the shrunk case to `tools/fuzz/corpus/regressions/` if it
is a distinct mechanism.

### 6. Keep or remove
- **Keep** the policy (commit it) if the pattern is a durable N1 stress axis
  worth re-running — the default outcome; every family driver in the registry is
  a kept one.
- **Remove** it (and its registry row/CMake line) only if it was a one-off whose
  pattern is now covered by a kept policy or the corpus.
Either way, the *finding* (mechanism + fix) is recorded in the commit, not just
in the throwaway code.

## When a feature lands — the standing rule (plan §5.3, §8.2)

Adding or changing a rule, or purifying a guard, is **not done** until the
schedules it newly legalizes are stress-tested this way, not merely covered by
the uniform `--random-policy`. If the uniform policy already saturates the new
pattern (rare), say so with the measured draw rate; otherwise write the targeted
policy. This is the N1 analogue of "a rule doesn't merge without its proof-delta."
