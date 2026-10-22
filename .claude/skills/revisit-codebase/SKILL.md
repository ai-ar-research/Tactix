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

Treat `tools/` as maintained product code, not an archive. Inventory each script
against a current documented workflow, test, build target, release gate, or
retained artifact reproducer. Remove redundant implementations, superseded
one-off launch/debug scripts, and their stale documentation or build references
once they have no active job, unique evidence, or remaining caller. Prefer one
maintained parameterized tool over several near-duplicate scripts; preserve a
script whose exact frozen copy is required to interpret a retained run.

## Act on findings

- Apply safe behavior-preserving fixes in scoped commits.
- Route structural or performance-changing work to `revamp`.
- Route rule-contract work to `rule-dev` and `calculus`.
- Route coverage gaps to `fuzz`; scope and gate questions to `revise-plan`.
- Record unfinished material findings in PROGRESS with an owner and next action.

Validate cleanup according to risk: compile and focused tests during the sweep,
then the owning subsystem's full gate before declaring the review complete. Do
not mix speculative redesign into cleanup or manufacture a long nit list.
