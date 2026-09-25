# AM387: bounded H2 DSR proof-order diagnostic

Date: 2026-09-25. Checker-only diagnostic; no solver, rule, or production checker change. The original-CNF H2 reproducer and its parent-identical proof are owned by the [AM369 H2 handoff](../runs/20260924-am369-condition-explicit-claude-fuzz/handoffs/H2-preexisting-dsr-sr-watch-pointer.md). This result does **not** close N2.

## Result

The pinned no-option `dsr-trim` rejection is sensitive to the relative order of the first explicit root unit `17 0` and six later CCE deletions that its UP closure can turn into implied-unit reasons. Merely moving any one of those deletions ahead of `17 0` fails forward checking. Moving all six ahead fails because `17 0` is then no longer RUP. An exhaustive 64-subset checker-only search found exactly four subsets whose rewritten proof passes **both** no-option backward and `-f` forward `dsr-trim`, with each resulting LSR accepted as `s VERIFIED UNSAT` by pinned Trestle `srcheck`:

| Mask | Deletions moved ahead of `17 0` |
|---:|---|
| 22 | `d -8 -9`, `d -9 15`, `d -19 20` |
| 23 | mask 22 plus `d 7 -20` |
| 54 | mask 22 plus `d -20 21` |
| 55 | mask 22 plus both optional deletions |

The sixth candidate `d -15 16` cannot move in any accepted subset; it is needed to derive `17` at that point. Mask bits, in order, are `d 7 -20`, `d -8 -9`, `d -9 15`, `d -15 16`, `d -19 20`, `d -20 21`. Every other proof line retains its original order. This is a bounded proof-order witness, not a generally valid transformation: selecting a safe order requires respecting both the unit's RUP antecedents and the checker's dynamic implied-unit reasons.

## Reproduction and provenance

Run `python3 benchmarks/probes/am387-dsr-order-subsets.py` from the repository root. It mechanically constructs each mask in a temporary directory and invokes both pinned checker modes and Trestle on passing forward candidates. The input CNF SHA-256 is `4b0fa70871df4cdaf32f492a0e893e24fde02fbc78db709a118e38fb7def7baa`; ASCII proof `68ca3ab1d9ff8de01970da58ef15ea8024c092885b81c4336e8832fa7da27b07`; `dsr-trim` `3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee`; Trestle `srcheck` `19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945`. The script asserts the proof layout and requires the exact Trestle `s VERIFIED UNSAT` status, not just process exit zero. It changes no frozen input and deletes its temporary variants.

## Interpretation and next action

`dsr-trim`'s default path ignores deletion of a clause that is currently the reason for a UP-derived literal. In H2, the first explicit unit can create checker-only implications through clauses the solver later deletes, leaving extra checker-visible clauses at an SR line. Mask 22 breaks three such implication chains early enough without losing the RUP derivation of `17`. This **suggests** a checker-visible proof-state divergence, but the subset search does not establish a general emission algorithm or prove all subsequent solver states correct. Do not hardcode this mask, narrow CCE/X7 legality, or silently change the canonical checker route. A general repair needs a proof-state invariant that accounts for deletion/UP dependencies, or an explicit checker-contract decision with replacement positive and negative controls. Then run current-source proof/model, random-policy, and matched performance gates before N2 acceptance.
