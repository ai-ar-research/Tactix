# Rank147 LONGTYPE deleted-clause fix

The retained Rank147 proof isolates a width bug in the audited LONGTYPE build
of `dsr-trim`.  The proof is SHA-256
`95542b2c19bc49dfc503eb0a71d23c259122bf998bd967472209edefbe7a68d9`
(709,553 bytes), byte-identical to the proof previously accepted by the
canonical 32-bit checker.  Its normalized original CNF is SHA-256
`d4a999db60e61564eebc46338d1cc9b1ad94b9ade22913049f87338cab731af3`.

## Defect

The source archive is
`benchmarks/runs/20261007-rank147-longtype-deleted-clause-fix/source.tar`,
SHA-256 `397557639afc3a58b7a46979a4bb77e09b664b049b7017d71355435ba08632ad`,
at upstream commit `0b8cc0ef4d228c059994923cbc66abeb9a42ea14` with the repository's
audited capacity patch (SHA-256
`edd70e504e573fbf9801a1c9f80a05fae04e2bcadd763a1e2a1b923df3afc6`).
In `src/global_data.c`, the deleted-clause predicate returned the raw sign-bit
mask through an `int` return type:

```c
#define IS_DELETED_CLAUSE(x) ((x) & SRID_MSB)
```

Under LONGTYPE the mask is bit 63.  Conversion to 32-bit `int` discards that
bit and returns zero, so backward checking does not restore clauses that were
soft-deleted after the candidate under examination.  On the failing proof,
the raw clause value `-9223372036854754358` is deleted, but the old predicate
returns zero.  The canonical checker restores the missing watch clauses and
proves line 110 by RUP; the broken LONGTYPE checker enters SR and rejects the
next line as `No UP contradiction for RAT clause 731`.

The source fix normalizes the mask to a Boolean before the return conversion:

```c
#define IS_DELETED_CLAUSE(x) (((x) & SRID_MSB) != 0)
```

The frozen patch is `fix.patch`, SHA-256
`52592ba37c46b422d77021dd2bf9c46a72f3cc476eddf90239dc114ab253deb8`.
The focused C reproducer reports `broken=0 fixed=1` and makes GCC emit the
corresponding conversion warning for the old expression.

## Validation

SLURM job 1755133 ran on `cpu-q` with four CPUs and 32 GiB, with no node
constraint.  It completed `0:0` in 17 seconds.  All frozen checks passed:

- the unpatched LONGTYPE checker reproduced the exact line-111 rejection;
- canonical 32-bit and fixed LONGTYPE backward checking both verified UNSAT;
- both emitted the same binary LSR, SHA-256
  `e9d97cd144d2ed657555935945158c2507b290d7f9f19b7064ffb23b7a729363`;
- fixed LONGTYPE decompression emitted text LSR SHA-256
  `67cfd9181c2ed3efcf3c5ddaf0b855de9b6e262afe896a71dcd904d4a279fe89`,
  independently accepted by Trestle/srcheck SHA-256
  `19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945`;
- the canonical and unpatched LONGTYPE forward routes produced byte-identical
  LSR output, isolating the bug to backward deletion restoration rather than
  binary parsing or the forward proof engine; and
- canonical 32-bit and fixed LONGTYPE both rejected the malformed empty-proof
  negative control.

The fixed immutable pins are:

| Pin | SHA-256 |
|---|---|
| `fixed-pins/dsr-trim` | `30207e6f0a6e3b5374976e6ef06528cea21c429383223ba46ae1020b8d7ff564` |
| `fixed-pins/dsr-compress` | `8ddab8302509ff6c3ebe1b6534698d64ff072b202b5b5f903dc4898bf73dd531` |

The campaign manifest, launch record, complete command/output tails,
terminal record, source archive, patch, and reproducer are preserved under
`benchmarks/runs/20261007-rank147-longtype-deleted-clause-fix/`.

This establishes the checker correction on the retained small proof.  It does
not alter submitted campaign pins, reclassify the existing Rank147 retry row,
or promote the fixed checker into an accepted qualification oracle.  The new
pin requires an explicit profile and independent review before use for
acceptance.
