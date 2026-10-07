# Bounded failure handoff — integrated cumulative fuzz, candidate 4096d41b3

**Failure class:** controller/action-lifecycle defect → unexplained nontermination.
A random-controller **native `NaryCspEquality` (X18) offer** is dispatched after
search has changed the root formula, the action returns **`InvalidRequest`
(InvocationStatus 9, `applications=1`)**, and the controller treats that as
terminal: `c ERROR: controller execution status=9 action=NaryCspEquality`,
`s UNKNOWN`, exit 0. There is no proof/model rejection, sanitizer report or
assertion. Fails identically with the X20 producer disabled, so X20 is not the cause.

**Expected:** a self-offered native payload that no longer matches the live
state is a stale/inapplicable decline (the random policy's
`DiscardUntilContextChange` settlement) or is never offered, and search continues
to a checked verdict. DefaultPolicy answers SATISFIABLE on the same CNF.

## Binding

- Source: commit `4096d41b35362e244e165ec1ac704728fdb55f4e`, archive sha256 `2e4f9859…f789b8`.
- Release pin `build/pin-integrated-4096d41b3-rel/tactix` sha256 `506ada0f…c367`.
- ASan pin `build/pin-integrated-4096d41b3-asan/tactix` sha256 `626bda31…85dfe`. Same result, no sanitizer output.
- Reproducer `x20rand-s24.min.cnf` (12 vars, 34 clauses, sha256 `a9a2a35f18580ea13cebddd816a3b4cea2c70594e7b16bb9bec84561fe06311f`), ddmin-shrunk (job 1751479) from
  `x20rand-s24.cnf` (`630db1b3…c6e1`, targeted task `x20-random-unsat-forced-bin-x-s0-24`,
  fixture seed 2100143024, job 1751475). Deterministic: about 0.01 s on Release.

## Command

```
tactix --dsr --controller-policy=2100643024 \
  --controller-families=NaryCspPrefix,NaryCspEquality,NaryCspEqualityAmo,Propagate \
  --controller-native-producers=nary-csp,nary-csp-equality \
  --controller-restart-period=1 --controller-trail-retention=full \
  x20rand-s24.min.cnf proof.dsr
# -> stderr: c ERROR: controller execution status=9 action=NaryCspEquality ; stdout: s UNKNOWN ; exit 0
tactix --dsr x20rand-s24.min.cnf proof.dsr     # DefaultPolicy: s SATISFIABLE (exit 10)
```
Add `nary-csp-equality-amo` to the producer list: same failure.

## Evidence (`min-rel.trace`, `min-rel.feedback.jsonl`, `min-*.stdout/.stderr` here)

1. seq 1 `NaryCspPrefix` native offer, domains `[[1,7,9,10],[2,3,4,8],[5,6,11,12]]`, Completed (arena 95→107).
2. seq 2–27: ResumeSearch, then three decide/propagate/conflict/analyze/restart cycles. Root state reaches arena 112, touch 27.
3. seq 28: `NaryCspEquality` native payload
   `nary-csp-equality[3,4,1,7,9,10,4,2,3,4,8,4,5,6,11,12,0,1,2]` at state 112/27
   (producer line `"NaryCspEquality" 5 1 0 17 17 3 3`) → result `5 9 1 1 0 …`. The
   feedback outcome is `status 9, result 1, applications 1, work 1`. The trace then ends with `verdict 0`.

Also reproduced by `x20rand-s2.min.cnf` (48 clauses, seed 2100643002) and
`x20rand-s7.min.cnf` (45 clauses, seed 2100643007), each with the same command
and its own seed. These are 3 of 60 `x20-random`/`unsat-forced` targeted tasks.

## Scope

- Reachable only with explicitly enabled X17/X18 native producers under the
  random controller. DefaultPolicy keeps these rows disabled, and the broad L2 leg (producers off) never reached it.
- After a repair, rerun the `x20-*`/`drv` targeted kinds plus the 3 reproducers, then L2 if controller settlement changes.
