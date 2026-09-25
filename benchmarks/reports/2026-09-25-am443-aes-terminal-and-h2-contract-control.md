# AM443: paired AES proof completion and a small H2 checker-contract control

Date: 2026-09-25. This records terminal evidence and a checker-only control.
No solver, public action, proof emitter, checker, benchmark pin, or checker
contract changed. It is not N1--N4 or Tier-C acceptance.

## Paired AES checker diagnosis is terminal

SLURM array `1723443_0/1` completed `0:0` in 1:38:03 and 1:38:13. The
immutable two-arm manifest, launch hash, pins, logs, and final JSON records
are in `benchmarks/probes/am434-aes-extended/`; the launch manifest SHA-256
is `66479c8aae5ec62d101141de8d0c12be7c2be926b5528267ed5b2a3f8bb06f80`.
Both tasks solved the exact original
`tierC/aes_decry_2_rounds.debugged.cnf.xz` as UNSAT in 100.178 and 99.721
seconds, respectively. Their 238,948,969-byte native DSR certificates have
the same SHA-256
`6a04b0150c06f05d4772afa170604e4308495c1565210bdda77ef32f30db4a62`.
The validated checker CNF has the same SHA-256 as the original decompressed
CNF, `6b3f878c...`; it preserves the clause sequence and multiplicities.

| Arm | `dsr-trim` no-option | decode | Trestle `srcheck` | Complete chain |
| --- | ---: | ---: | ---: | ---: |
| Candidate | verified, 1,055.645 s | 1.669 s | verified, 4,716.160 s | verified, 5,780.266 s |
| Parent | verified, 1,033.130 s | 1.669 s | verified, 4,748.280 s | verified, 5,789.923 s |

Both records mark `valid=true`, `proof_check.result=verified`, exact checker
status `s VERIFIED UNSAT` in both stages, and complete certificate/derived
artifact cleanup. Thus the earlier matched Tier-C 4,800-second checker
timeouts on this pair were resource limits, not rejected certificates. The
original Tier-C campaign remains **unscorable** at 29/30 checked per arm:
this separate diagnostic used a 45,000-second checker allowance, and does not
retroactively alter the frozen matched resource contract. AM434 owns the
earlier campaign details.

## Small checker-retained-reason control

The frozen AM369 `dsr-trim` binary has SHA-256 `3b5c7737...`. A two-variable
checker-only input and ASCII DSR stream are:

```
p cnf 2 2
1 0
-1 2 0
```

```
d -1 2 0
-2 -2 0
```

The input/proof SHA-256 values are `f74c9d60...` and `767f0379...`.
With the proof truncated after the deletion, frozen no-option
`dsr-trim -f --emit-valid-formula-to /dev/stderr` prints **both** original
clauses. The same command with `--delete-implied-units` prints only `1 0`.
On the full two-line stream, no-option forward checking rejects the SR addition
(`No UP contradiction for RAT clause 2`); diagnostic option mode reports
`s VALID`, correctly without an empty clause. No-option backward checking
alone misleadingly reports `s VERIFIED UNSAT` after deriving an empty clause,
but the independently pinned Trestle `srcheck` returns `s INVALID` on its LSR.
This is a useful negative control for requiring the **complete** canonical
DSR→LSR→Trestle route; it is not an UNSAT solver result.

At the no-option forward boundary, the checker formula contains unit `1` and
reason `-1 ∨ 2`, so it entails `2`. Adding `-2` would turn this satisfiable
checker formula unsatisfiable. A substitution witness for that direct addition
therefore cannot make it a sound satisfiability-preserving SR step against the
retained formula. The diagnostic option removes the reason first. This is a
checker-only logical control, not a proof that no possible alternative DSR
derivation exists for every public X7 trajectory. AM436/439 supply the exact
public H2 trajectory: its 16-clause, 314-byte no-option refutation rejects in
both directions, while the option-mode elaboration passes Trestle. AM404/407
show option mode is non-monotone on other archived proofs. No checker option or
legal X7 surface is changed here.

Next: develop a general canonical proof-state/derivation repair or present an
explicit audited alternative checker contract for user choice. Complete
affected Opus fuzz and cumulative correctness/performance gates before any
Tier-D submission.
