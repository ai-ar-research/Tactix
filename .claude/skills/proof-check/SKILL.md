---
name: proof-check
description: Verify and debug Tactix proof certificates with the checker required by the target competition year, validate checker failure behavior, and measure proof size or check cost. Use for proof-emitting changes, rejected certificates, or competition validation.
---

# Proof checking

Tactix's canonical pipeline is native DSR, elaborated forwards by pinned
`dsr-trim -f -c` to LSR and independently verified by pinned Trestle `srcheck`.
Decode binary LSR with `dsr-compress` when required. This project decision is
owned by `DEVELOPMENT_PLAN.md`; verify competition-specific requirements
separately. DRAT/LRAT remain compatibility outputs for the earlier vocabulary.

## Acceptance

- Match the checker's documented success status and exit code exactly.
- Demonstrate rejection with a known-bad or truncated proof whenever checker
  setup, invocation, parsing, or acceptance logic changes.
- A valid UNSAT verdict with a rejected proof is a correctness failure.
- Validate proofs against the exact original CNF and preserve the command,
  checker version, stdout/stderr, and first failing proof location.

## Diagnose a rejection

1. Reproduce the smallest exact command and determine whether the fault is the
   solver, format adapter, checker invocation, or checker limitation.
2. Shrink the CNF while preserving the same failure.
3. Correlate the failing proof line or byte range with the emitting rule and
   application. The current solver does **not** provide this mapping; implementing
   proof-event correlation is a required phase 4.5 prerequisite recorded in the
   plan/PROGRESS, not an optional debugging convenience.
4. Inspect that rule's proof delta, antecedents, deletion timing, and state
   transition. Add a negative regression that fails before the fix.
5. Re-run the focused checker, fuzz self-test when applicable, and the broader
   native plus target-year proof pipelines.

Record proof bytes, bytes/conflict, and check time only when they inform a gate
or design decision.
