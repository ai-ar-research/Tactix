# `dsr-trim` forward-elaboration repair

## Disposition

Direct-SR R16 remains unchanged at source `298c3467`. Claude fuzz job `1689026`
found valid solver-emitted DSR certificates that pinned `dsr-trim` accepted but
elaborated into LSR certificates rejected by Trestle. The retained failures are
in the unverified elaborator, so the repair updates and patches `dsr-trim` while
keeping Trestle as the independent acceptance boundary.

## Diagnosis

The four retained failure classes reduce to two forward-elaboration defects:

1. The CNF parser deletes input tautologies internally but did not emit matching
   LSR deletion lines. Later SR checks therefore used a different formula from
   Trestle. Duplicate clauses exercise the same normalization boundary.
2. An SR addition can make the global formula contradictory before another DSR
   line is parsed. The elaborator tried to print a nonexistent next candidate
   instead of emitting an empty clause with the unit-propagation hints that
   derived it.

Upstream commit `0b8cc0ef4d228c059994923cbc66abeb9a42ea14` already includes the
earlier deleted-tautology hash fix. The repository patch
[`dsr-trim-0b8cc0e-forward-elaboration.patch`](../../tools/checkers/patches/dsr-trim-0b8cc0e-forward-elaboration.patch)
adds the missing downstream deletions, emits the early empty-clause derivation,
and prevents forward mode from reading backward-only UNSAT-core state.

All active Tactix DSR pipelines now request forward elaboration explicitly. This
checks every DSR line and avoids relying on the separate backward dependency
pruner. Binary DSR and compressed LSR remain the normal stored formats.

## Verification

| Check | Result |
|---|---|
| Pinned upstream | `0b8cc0ef4d228c059994923cbc66abeb9a42ea14` |
| Repository patch SHA-256 | `a5653c3a2d399c3f26613339aedc9d9238dc5c2b13b1ef8c0848430ecea6c21c` |
| Patched `dsr-trim` SHA-256 | `a1d7251cc615472f6a795107cb3a5f667054e535189ad09383f35e34d5b23463` |
| Pinned Trestle SHA-256 | `f0eef5c48e2378c6fb5dfee4c7dc68ad0ff96d359067483fbf484d266f3aeb18` |
| Checker setup self-test | PASS: DRAT/LRAT, ordinary DSR, tautology+duplicate SR, early contradiction, invalid DSR, and invalid LSR controls |
| Retained `1689026` representatives | 5/5 pass `dsr-trim -f` to compressed LSR, decompression, and Trestle |
| Python tool tests | 66/66 pass |
| R16 micro-proof and competition checks | PASS with forward DSR elaboration and Trestle verification |

The checker repair is necessary but does not close the R16 fuzz gate. A fresh
Claude-owned campaign with the repaired immutable checker pin must pass before
the direct-SR increment is accepted.
