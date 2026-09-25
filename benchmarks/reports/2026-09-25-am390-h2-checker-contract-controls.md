# AM390: H2 implied-unit deletion contract and negative controls

Date: 2026-09-25. Checker-only option experiment on the frozen AM369
`dsr-trim` SHA-256 `3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee`
and Trestle SHA-256 `19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945`.
No solver, rule, canonical oracle, checker install, or public API changed. The
historical no-option route remains authoritative; see [AM389](2026-09-25-am389-dsr-checker-pin-split.md)
for the separate workspace-checker drift.

## Exact result

The 21-variable, 42-clause H2 proof still exits 1 on the frozen **no-option**
backward checker at `[line 18] Clause 60 not in watch pointers for literal 22`.
On the *same* CNF and proof, frozen `dsr-trim -f --delete-implied-units`
reports `s VERIFIED UNSAT`, and independent frozen Trestle reports
`s VERIFIED UNSAT` on its LSR. The option changes which requested deletions
are honored; it is not a proof-independent solver repair.

The existing `tests/micro_proofs.sh` implied-unit-deletion control is
contract-specific. Its CNF contains `-5` and `(-55 ∨ 5)`; the proof deletes
that binary, adds a witnessed unit `55`, then closes an independent UNSAT
tail. With no-option checking, the binary is retained as the checker reason
for `-55` and the witness is invalid. With `--delete-implied-units`, the
binary is actually deleted: `55` is then satisfiability-preserving, and
both forward `dsr-trim` and Trestle verify. This acceptance is therefore
**expected under the changed formula**, not evidence that the option accepts
an invalid witness.

As a paired negative control, remove only `d -55 5 0` from that proof. The
binary then remains present even with `--delete-implied-units`; frozen
forward checking exits 1 at `Error: [line 1] No UP contradiction for RAT
clause 41`. The separate invalid witnessed-unit fixture (`1 1 0` on the
five-variable, seven-clause input in `tests/micro_proofs.sh`) also rejects in
forward mode with the option at proof line 1. These bounded controls do not
prove all option paths sound; they identify the acceptance-contract change
the tests must reflect if the user approves it.

On current AM388 Debug, the **unchanged** fixed `micro_proofs` suite passes
1/1 in 37.54 seconds with frozen no-option checker paths (AM389). A diagnostic
variant that omitted only the no-option-specific control's source lines
314--342 and wrapped the same frozen checker with
`--delete-implied-units` reached `micro_proofs OK`; every remaining fixed
proof/model test passed. The unchanged suite with the option fails exactly at
`FAIL: reference pipeline accepted a deleted implied-unit reason` after
11.17 seconds. This is not a production test edit or a new acceptance gate.
The independent proof-oracle unit suite passes 66/66 on current HEAD:
`PYTHONPATH=tools/bench python3 -m unittest tools.bench.test_proof_oracle
tools.bench.test_proof_oracle_binaries tests.test_fuzz_proof_oracles`.

## No simple no-option reorder established

Checker-only H2 variants show why publishing the existing root units
`18,19,4,9` before the 28 early deletions is insufficient: the frozen
no-option checker still retains all six implied-unit reasons and fails at the
same SR addition. Adding explicit units for the complete observed implication
closure before those deletions removes the six ignore messages, but the proof
then fails a later witnessed step (`Reduced clause 48 claims contradiction`
at forward line 20). A redundant unit may be sound yet make a later SR witness
invalid; neither bulk unit materialization nor a fixed deletion shuffle is
a general repair. AM387's four accepted orderings are bounded witnesses only.

## Decision and next action

The pending user choice is whether a **fully audited**
`--delete-implied-units` route may replace the historical no-option route.
If allowed, update the calculus/checker contract, replace the contract-specific
negative control with the paired retained-binary negative, and validate broad
original-CNF proofs, models, invalid certificates, and the frozen competition
path before changing production oracles. If no-option must remain, a general
proof-state/emission design is required; these reorder experiments rule out
the two simple unit-publication variants. Either way, H2 and combined N2
remain open, as do Claude-owned fuzz, N1/N3/N4 and the post-goal Tier-D run.
