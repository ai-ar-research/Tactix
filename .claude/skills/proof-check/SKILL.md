---
name: proof-check
description: Verify and debug Tactix proof certificates with the checker required by the target competition year, validate checker failure behavior, and measure proof size or check cost. Use for proof-emitting changes, rejected certificates, or competition validation.
---

# Proof checking

`DEVELOPMENT_PLAN.md` §5 and `tools/bench/proof_oracle.py` own the current
certificate route. A declared witness-free native DSR proof is validated and
converted losslessly to DRAT, elaborated by drat-trim and independently checked
by CakeLPR. A witnessed DSR proof uses pinned **no-option**
`dsr-trim <checker-cnf> <dsr> <lsr>` (backward/core-trimmed), binary-LSR
decoding with `dsr-compress -d` when needed, and Trestle `srcheck`.
Forward `dsr-trim -f` is an additional every-lemma diagnostic, not the
canonical acceptance command; `--delete-implied-units` is not enabled.
A complete
startup PBP takes VeriPB then CakePB. Explicit DRAT/LRAT are compatibility
outputs. Verify competition-specific requirements separately.

## Acceptance

- Match the checker's documented success status and exit code exactly.
- Demonstrate rejection with a known-bad or truncated proof whenever checker
  setup, invocation, parsing, or acceptance logic changes.
- A valid UNSAT verdict with a rejected proof is a correctness failure.
- Validate proofs against the exact original CNF and preserve the command,
  checker version, stdout/stderr, and first failing proof location.
- For routine regression runs, apply `DEVELOPMENT_PLAN.md` §8.4's bounded
  checking contract. Give the complete pipeline one shared deadline, preserve
  stage outcomes and distinguish clean exhaustion from rejection/error. Use
  comparable baseline evidence for check-cost review; do not relabel censored
  checking as verified or automatically launch an extended recheck. Full
  verification and fuzz acceptance keep their own requirements.

## Diagnose a rejection

1. Reproduce the smallest exact command and determine whether the fault is the
   solver, format adapter, checker invocation, or checker limitation.
2. Shrink the CNF while preserving the same failure.
3. Correlate the failing proof line or byte range with the emitting rule and
   application. The current solver does **not** provide this mapping; implementing
   proof-event correlation is a required phase 4.5 prerequisite recorded in the
   plan, not an optional debugging convenience.
4. Inspect that rule's proof delta, antecedents, deletion timing, and state
   transition. Add a negative regression that fails before the fix.
5. Re-run the focused checker, fuzz self-test when applicable, and the broader
   native plus target-year proof pipelines.

Record proof bytes, bytes/conflict, and check time only when they inform a gate
or design decision.
