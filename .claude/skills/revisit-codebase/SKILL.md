---
name: revisit-codebase
description: Review the whole Tactix codebase after each subsystem or gate, apply safe behavior-preserving cleanup, and route structural, rule, coverage, or scope findings to their owning workflows.
---

# Review and clean the codebase

One reviewer sweeps `src/`, `tests/`, `tools/`, build integration, and relevant
docs. Review after every completed subsystem and phase gate.

Look for duplication, dead paths, stale TODOs, accidental verbosity, unsuitable
data structures, module-boundary violations, rule-versus-policy leakage, missing
tests/fuzz reachability, and documentation drift. Support findings with symbols,
tests, traces, or measurements; deduplicate by root cause.

## Act on findings

- Apply safe behavior-preserving fixes in scoped commits.
- Route structural or performance-changing work to `revamp`.
- Route rule-contract work to `rule-dev` and `calculus`.
- Route coverage gaps to `fuzz`; scope and gate questions to `revise-plan`.
- Record unfinished material findings in PROGRESS with an owner and next action.

Validate cleanup according to risk: compile and focused tests during the sweep,
then the owning subsystem's full gate before declaring the review complete. Do
not mix speculative redesign into cleanup or manufacture a long nit list.
