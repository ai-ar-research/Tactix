---
name: compete
description: Implement or verify SAT Competition 2026 sequential-main-track compliance, including DIMACS parsing, output and exit contracts, proof checking, resource behavior, repository packaging, and system-description requirements.
---

# SAT Competition 2026 compliance

The official 2026 rules, output specification, and no-hardware-restrictions
submission page are authoritative. Keep this prose synchronized with automated
checks and maintain an explicit gap list for requirements that cannot yet be
tested.

## Solver contract

- Stream DIMACS robustly: comments between clauses, arbitrary whitespace,
  duplicate literals, large files, and a missing final newline.
- Emit exactly one `s` line. SAT uses exit 10 and a complete original-formula
  model on `v` lines ending in `0`; UNSAT uses exit 20 and a valid proof;
  unresolved or malformed input uses another exit status.
- Keep diagnostic output on `c` lines. Flush output and proofs on termination.
- Validate SAT models independently. For UNSAT, validate the project certificate
  pipeline owned by `DEVELOPMENT_PLAN.md` §5 through `proof-check`; native DSR
  and complete startup PBP are not restricted to LRAT-compatible output.
  Verify target-year official acceptance separately; project checker success
  alone does not establish competition compliance.

`tools/compete/check.sh` is the executable specification. Add a regression to
it for every compliance bug and keep a visible list of official requirements
that remain manual.

## 2026 package

Package from a clean checkout in a private GitHub repository with:

- top-level `build.sh`;
- top-level `run.sh <instance> <output-dir>`, producing
  `<output-dir>/proof.out`;
- source, licenses, and attribution needed by the rules; and
- a 1–2 page IEEE-style system description including required AI disclosures.

Make packaging year-parameterized even though this skill is pinned to 2026.
Rehearse the wrapper, output checks, model checker, and the selected complete
proof pipeline under the official 32 GB constraint. Run solver measurements
through `slurm-jobs`.
