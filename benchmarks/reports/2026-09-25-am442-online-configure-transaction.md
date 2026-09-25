# AM442: rejected online configuration preserves the C input session

Date: 2026-09-25. Scope: the three additive C online-configuration entry
points and their input-finalization boundary. This is an API lifecycle repair,
not a rule, scheduler, proof-calculus, default-policy, or benchmark change.

## Cause and general repair

Previously `tactix_online_configure`, `_v2`, and `_v3` called `input_complete`
before converting and validating their request. A rejected program therefore
returned `-1` after `finish_input` had set `input_completed`; even a partially
typed clause could no longer be closed through `tactix_add_checked`. An invalid
request also prevented late proof attachment.

All three functions now convert and configure first, and finalize input only
after successful configuration. `OnlineStrategyExecutor::configure` validates
and constructs a replacement before changing its program/runtime. The order
change is valid for both accepted and rejected requests: C payload conversion
reads the stable `external_to_internal` map via `kernel_internalize`, while
`finish_input` retires unactivated variables but does not remap those literals;
the deferred proof replay remaps proof IDs, not variables. The old accepted
route already permitted requests naming an unactivated/retired variable,
because `kernel_internalize` does not reject `VarStatus::Eliminated`. There is
therefore no new action surface or proof-identity interpretation. Rejected
requests leave the stream open; accepted requests still finalize before any
online step can execute. The public header now states this boundary.

This source inspection resolves AM438's stated concern about a mechanical
reorder for this *specific* boundary; AM438 is frozen historical evidence.
It does not solve the separate need for a session-owned live DefaultPolicy
when implementing production online learning.

## Tests and independent certificates

`tests/test_online_api.c` now exercises nine combinations: v1/v2/v3 crossed
with proofless, early LRAT, and late LRAT attachment. Each starts with a
partially typed clause, rejects a malformed rule ID during conversion,
finishes that clause, rejects a zero StepId after conversion/executor
validation, then (v2/v3) rejects malformed nested payloads. Each adds more
input after every rejection. A valid program succeeds and closes input;
post-success checked addition returns `-1`. Proofless cases return SAT with
the expected original-variable values. Proof-attached cases add a conflicting
unit before the successful configuration and return UNSAT.

The six retained 10-byte LRATs are under
`benchmarks/probes/am442-online-config-proof/`. Early and late proofs are
byte-identical within each version: SHA-256 `3513d241...` for v1 and
`9db98000...` for v2/v3. CakeLPR binary SHA-256 `ab9dc6c4...` reports
`s VERIFIED UNSAT` for all six against the exact original fixture CNFs
`tests/fixtures/online-config-3.cnf` (v1) and `online-config-4.cnf` (v2/v3).
A deliberately mismatched pair (v1 proof with the four-variable CNF) prints
`Checking failed` and no verified token, despite checker exit code zero;
acceptance therefore uses the exact verdict, not process status alone.

Focused current-source Debug and Release each pass `api_n1`, `api_instances`,
`online_api`, `online_all_actions`, and `proof_output_failure` (5/5). A Debug
ASan+UBSan build passes `online_api` and `proof_output_failure` (2/2), leak
detection enabled. The full current-source Debug job `1723391` was still
running its final scripted demo at this snapshot, and was pinned *before*
AM442; it does not validate this change. No real-instance solver run or
performance claim is made. Affected fuzz and any wider API gate remain due.

## Open work

The canonical no-option H2 DSR rejection, broad N1/N2 acceptance, N3/N4
gates, production online-learning architecture, final Andrew readiness
report, and post-goal full Tier D remain open. Paired AES checker job
`1723443_0/1` was still running and is independent of this API repair.
