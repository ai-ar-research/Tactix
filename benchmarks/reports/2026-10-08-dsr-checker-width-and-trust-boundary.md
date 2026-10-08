# DSR checker: 32-bit capacity, the 64-bit deletion bug, and the trust boundary

This report consolidates the checker evidence behind fixed44 closures. It adds the
current c2 observations and states which components are trusted. Primary sources:
[capacity audit](2026-10-07-dsr-checker-capacity/README.md),
[deletion-bug explanation](2026-10-07-dsr-longtype-checker-bug-explained.md),
[long64-fixed profile](2026-10-07-long64-fixed-checker-profile.md) and the
[independent review](../runs/20261007-long64-fixed-independent-review/DISPOSITION.md).

## 1. Why a 64-bit build is needed: 32-bit capacity overflow

The pinned upstream `dsr-trim` (commit `0b8cc0e`, canonical32 binary
`3b5c7737…`) stores clause IDs, formula entries and literal-database capacities in a
4-byte `srid_t`. Its growth rule `RESIZE(x) = (x * 3) >> 1` overflows signed `int`
at x = 715,827,883. UBSan reproduces this without a large allocation (job 1754053).
On multi-GB proofs the overflowed capacity becomes an impossible allocation, and
the checker dies within about 100 s with `Ran out of memory on xrealloc()`, far
below the memory limit.

| Row | Proof | Outcome with canonical32 |
|---|---|---|
| 627 | 5.57 GB (am546) | `xrealloc` failure after 91 s |
| 654 | 12.8 GB (am546) | `xrealloc` failure after 71 s |
| 1004 | 4.75 GB (am546); 5.30 GB (c2 + Gaussian search, job 1756520) | `xrealloc` after 100 s; exit 255 after 92 s at 4.5 GB RSS |

These are capacity crashes, not proof rejections. The upstream remedy is the
supported `make long` (`-DLONGTYPE`) build, which makes these types 8 bytes. It
passed 33 directed controls: 16 valid proofs across widths, formats and modes, each
independently verified by Trestle; 16 invalid proofs failed closed; and Trestle
rejected an invalid LSR (job 1754053).

## 2. The deletion bug in the first 64-bit build

Under `LONGTYPE` the deleted-clause marker moves to bit 63, but
`is_clause_deleted()` returns `(x & SRID_MSB)` through a 32-bit `int`. On the
audited compiler that conversion yields 0. Backward checking therefore skips
restoring user-deleted clauses that earlier proof steps need. This produced false
rejections such as Rank147's `No UP contradiction for RAT clause 731` and, very
plausibly, Rank569's `Clause … not in watch pointers`.

The fix is one macro, `IS_DELETED_CLAUSE(x) (((x) & SRID_MSB) != 0)`
(`fix.patch` `52592ba3…`). It changes bookkeeping only: no proof rule is added,
no inference is bypassed and no acceptance flag is relaxed. The evidence (job
1755133) does not rest on the modified checker accepting its own output:

- the unmodified 32-bit checker accepts the same Rank147 proof;
- the repaired 64-bit checker emits a byte-identical LSR;
- the unchanged Trestle verifies that LSR against the original CNF.

The resulting **`long64-fixed`** profile (`dsr-trim` `30207e6f…`,
`dsr-compress` `8ddab830…`, Trestle unchanged `19bf79f4…`) was independently
reviewed and **ACCEPTED** for explicit use. Across 587 corpus and mutant cases it
agrees with canonical32. The reviewer recorded these limits:

- no proof beyond 32-bit capacity (offsets ≥ 2³¹) was tested;
- int-typed allocators still overflow near 1.07 × 10⁹ clauses;
- large-proof time and memory were unmeasured;
- a signed `1L << 63` in `MSB64` remains a UBSan finding.

## 3. Current observations on candidate c2

- **Rank627 verifies.** UNSAT 1,229 s with a 4.21 GB proof. `dsr-trim` 4,623 s
  at 27.8 GB peak RSS, `dsr-compress` 143 s, Trestle 1,034 s at 18.3 GB: verified
  end to end in 5,809 s (`benchmarks/runs/20261007-fixed44-c2/results/022.json`).
  The historical `xrealloc` failure is gone.
- **Rank569 does not fit in 32 GB.** UNSAT 3,658 s with a 4.12 GB proof.
  `dsr-trim` was killed at **33.4 GB** after 7,855 s. That is memory, not a width
  bug or a rejection. The user keeps the 32 GB limit. A follow-up is measuring the
  32-bit vs 64-bit memory ratio on shared proofs and comparing with Kissat SC2024,
  whose 3,305 s proof verifies.
- **Rank1004** was solved by the Gaussian-search arm, but checked with canonical32
  and therefore crashed. It must be rechecked with `long64-fixed`.

## 4. Trust boundary

A closure counts only when (a) a formally verified final checker accepts a
refutation of the hash-bound original formula, or (b) the model checker validates a
model against the original CNF. Everything upstream of the final checker is an
untrusted elaborator: a defect there can cause a crash or a false *rejection*, never
a false acceptance.

| Component | Role | Trust status |
|---|---|---|
| Input CNF | cached instance, SHA-256 bound in each manifest | trusted input |
| `canonical_checker_cnf` (`tools/bench/proof_oracle.py`) | respells the CNF for pinned readers: comments/whitespace removed, header 0→1 unused variable; clause order and multiplicity unchanged; malformed input rejected | **trusted, unverified** (small Python) |
| Tactix proof (DSR/DRAT/PBP) | solver output | untrusted |
| `dsr-trim` (upstream + forward-elaboration/bitmask patch + LONGTYPE + deletion fix) | DSR → binary LSR elaboration | untrusted elaborator; review showed some invalid inputs pass it alone but are rejected end to end |
| `dsr-compress -d` | binary → text LSR | untrusted format converter |
| **Trestle `srcheck`** (FormalSAT/trestle `03ac9418`, text mode) | final DSR/LSR verifier against the original formula | **trusted**; core check `SR.checkLine` is Lean-verified. The repository patch only adds a binary-LSR parser/driver (86 added lines, none changed), which the acceptance route does not use (`proof_oracle.py:1041-1046`) |
| Python DSR→DRAT conversion, `drat-trim -L` | witness-free route elaboration to LRAT | untrusted |
| **`cake_lpr`** (`a4323b2`) | final LRAT verifier | **trusted**; CakeML-verified |
| VeriPB → **CakePB** | PB route | CakePB trusted (verified); VeriPB untrusted |
| `checkmodel` / `modelcheck.py` | SAT model check against the original CNF | **trusted, unverified** (simple evaluation) |
| `runtime_task.py` / `proof_oracle.py` result logic | `valid` only if every stage reports `verified`, the last being the final checker's `s VERIFIED UNSAT` (`proof_oracle.py:211-237`, `runtime_task.py:765-769, 965`), and the verdict and solver exit code match | **trusted orchestration**; reviewed, unverified |

So the `long64-fixed` repairs live entirely in an untrusted elaborator. They can
change which valid proofs are accepted, but they cannot make an invalid proof pass
Trestle. The residual trusted-but-unverified code is the CNF respelling, the text
parsing and I/O glue around Trestle's verified core, the model checkers and the
runner's result classification. The unused binary-LSR patch is compiled into the
Trestle binary. Removing it, or verifying it, would shrink the trusted build.
