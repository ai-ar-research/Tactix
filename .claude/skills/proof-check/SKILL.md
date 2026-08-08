---
name: proof-check
description: Verify and debug Tactix proof certificates with the checker required by the target competition year, validate checker failure behavior, and measure proof size or check cost. Use for proof-emitting changes, rejected certificates, or competition validation.
---

# Proof checking

Select the checker pipeline from the target year's official rules. For the 2026
submission, the project target is DPR/LPR checked by `cake_lpr`; retain native
DRAT/LRAT tools where they provide faster development diagnostics.

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
