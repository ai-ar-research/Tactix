# AM352: public-conflict solve-entry checkpoint (unaccepted)

Source: `f34524233c101c62548d038d5cebbe2bafaf03a0`; `git archive` SHA-256 `866acbd69de16c61e892a94b6d9211f8fd14221d1945d935c8ba93c858b82b77`.
This is a correctness checkpoint, not a N1/N2/N3/N4 promotion or a Tier D benchmark authority.

## Cause and repair

The AM350 Opus terminal campaign reported bounded handoff H1 in `benchmarks/runs/20260924-am350-late-proof-claude-fuzz/triage/congruence-assert/reproducer/HANDOFF.md`. Its 14-variable, 21-clause input and 45 legal public API actions ended with a non-root `Propagate` conflict. `tactix_solve` then used bare `kernel_backtrack(k,0)`: the decision trail disappeared, but `conflict_valid` remained true. GDB on the old Debug driver stopped at `congruence_rup_chain` with `level=0`, `conflict_valid=true`, and a one-literal trail. This was an entry-state violation, not an LRAT-chain derivation defect.

DefaultPolicy now uses S1 `restart_backtrack_to(k,0)` at solve entry whenever there is a decision level or recorded conflict. Restart clears a stale record or discharges a surviving root conflict through native analysis/proof. The Congruence assertion and guard are unchanged. The S1 implementation ledger records the policy-owned composition; the abstract S1 calculus was already correct.

## Causal and focused checks

- The new `test_api_c` case makes a legal public decision, records a `Propagate` conflict, then calls `tactix_solve` with native LRAT and binary DSR attached. The pre-fix AM351 Debug objects abort at `congruence.cpp:1687` when linked with the new test; AM352 Debug returns SAT with both forced values. `test_api_c`, `test_restart_reduce`, `test_congruence`, 300 fixed random-policy seeds, 288 root-conflict-reuse fixtures, and `test_solve_reset` pass.
- The exact H1 45-action driver linked against AM352 Debug returns SAT with LRAT attached before clause 1 or after clause 21. `tools/fuzz/checkmodel` independently accepts each returned 14-variable model against all 21 original clauses. Old AM350 Debug aborts at both attachment points, per H1 handoff.
- AM351's separate F4 population repair made a tautology-only variable inactive. Consequently `test_online_api`'s old one-tautology fixture no longer exercised its intended general-SR producer and failed on both AM351 and AM352. Replacing that discarded clause with a live `(1 ∨ 2)` fixture restores the producer-path test; AM352 `test_online_api` passes. This is a test-fixture correction, not a rule/guard change.
- Documentation size, proof-rule synchronization, diff whitespace, and comment hygiene checks pass. The first AM352 full CTest invocation caught an initial first-person source comment; the corrected source passes `check-comment-hygiene.py` independently.

## Validation still open

The first AM352 full Debug CTest invocation is still running its long Lucky, symmetry-backend, and scripted-demo tests. It was started with *relative* checker environment paths, so `micro_proofs` and `sweep_proofs` skipped; those runs are **not** proof evidence. Rerun both with absolute paths to the immutable AM350 checker pins after the live CTest completes. Rerun comment hygiene under CTest or accept its independent post-edit pass explicitly. Rebuild from the final source before pinning fuzz binaries: the running CTest executable predates a comment-only source edit.

The AM350 Opus terminal focused campaign covered 50,500 structured cases with no other new solver failure, but it is not a broad acceptance gate. AM349 and AM351 Opus campaigns remain active/queued. AM352's separate Opus request is `benchmarks/runs/20260924-am352-root-restart-request.md`; until its terminal report verifies public-conflict-to-solve reachability and all model/proof oracles, this repair is not fuzz-accepted. Native DSR and LRAT have only focused AM352 checks so far.

Tier D remains unsubmitted. The requested future protocol is the original winner resource envelope: one Tactix task per Tier D instance, 4 CPUs, 32 GiB, 5,000 solver seconds, checker allowance 45,000 seconds, `cpu-dense-hwulab-q`. The full N1--N4 and fixed performance gates remain open unless the user changes the submission trigger.
