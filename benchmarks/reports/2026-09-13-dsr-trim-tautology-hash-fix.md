# `dsr-trim` input-tautology hash fix

## Disposition

Native-DSR fuzz job `1688465` failed because pinned upstream `dsr-trim` commit
`ffe486a4` mishandled tautological clauses in the input CNF. The retained solver
proof was valid after the checker bookkeeping defect was repaired. Tactix's DSR
writer and formula import did not require a change.

## Reproducer and diagnosis

Claude's [terminal audit](2026-09-13-native-dsr-claude-fuzz/terminal-report.md)
retained case `d000848`, seed `914600848`: a 264-clause CNF containing repeated
literals, duplicate clauses, and seven tautologies. On Slurm job `1688571`, the
exact old solver pin returned UNSAT and the pinned checker exited 1 with:

```text
Fatal error: Clause 3 was already deleted.
```

An empty DSR stream reproduced the same checker failure against the raw CNF.
Removing only the tautological input clauses made the checker pass; repeated
literals and duplicate clauses did not trigger it.

At commit `ffe486a4`, `parse_cnf(..., DELETE_TAUTOLOGIES)` marks each tautology
slot deleted. `prepare_dsr_trim_data()` nevertheless iterated every formula slot
and called `add_formula_clause_to_ht(i)`, revisiting a deleted clause. The
repository patch
[`tools/checkers/patches/dsr-trim-ffe486-tautology-hash.patch`](../../tools/checkers/patches/dsr-trim-ffe486-tautology-hash.patch)
skips slots for which `is_clause_deleted(i)` is true during formula hash-table
initialization.

## Verification

| Item | SHA-256 / result |
|---|---|
| Old pinned `dsr-trim` | `114b3662a0f3c7783c5f96964c1ab3bd25ed6808a50d66fdc111366421aa0833` |
| Patched `dsr-trim` | `f9f68939a97f5e1e2692a3299088cb8d7dccabcc6276bdf6b0214e01c305194f` |
| Repository patch | `f0583a9ebeda9e3c295862f474a85b8807989478603de76bd7c25481deba770a` |
| Exact old solver proof after patch | `dsr-trim`: `s VERIFIED UNSAT`; decompressed LSR: 964 bytes; Trestle: `s VERIFIED UNSAT` against the unchanged raw CNF |
| Setup/self-test job `1688572` | COMPLETED, exit 0; DRAT, LRAT, ordinary DSR, and raw-tautology DSR-to-LSR-to-Trestle lanes passed |

`tools/checkers/setup.sh` now checks out the exact upstream commit, verifies that
its diff equals the pinned repository patch, builds it, and runs the raw-input
tautology regression through both checker stages. The patch only prevents a
deleted parser slot from entering an internal hash table. It does not normalize
or rewrite the CNF or proof passed to Trestle.

## Acceptance consequence

Job `1688465` remains a failed fuzz checkpoint because its immutable pin used
the old checker. Replacement job `1688573` used identical Release and Debug
Tactix binaries and changed only the pinned checker and repaired fuzz harness.
Claude's terminal audit passed with zero failures across all audit modes and
stock fuzz legs, closing the checker-dependent native-DSR fuzz gate.
