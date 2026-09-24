# AM357: H4 semantic late-LRAT command audit

This is a bounded public-API diagnostic, not a repair or an N2 gate. No solver,
rule, proof checker, or harness file changed. It follows the AM356 canonical
current-pin checks and uses the second AM349 H4 handoff CNF, `case-idx142.cnf`
(9 variables, 31 original clauses, including repeated literal `5` in input
clause 2).

The handoff names `--ops=S --pair=7,-7;4,-4;4,7 --limits=0,0,4
--attach=late --finish=solve --lrat` as a repaired-pin rejection, but supplies
no preserved rejected certificate or complete executable command for this
second case. A standalone public C-API driver linked to the current AM355
Debug object build (CLI SHA-256 `c4f95a0f...`; driver SHA-256
`9e9fd58dc4b9fcb7eb5b39e1b1034cef1b2309bee1bfd2bb921e1fca141ef96a`)
imports that exact CNF, attaches LRAT either before import or after all clauses
but before reasoning, sets Congruence limits `(0,0,4)`, enumerates the public
semantic actions, and requests the three named pairs in order. The first
pair `(7,-7)` is present among three enumerated actions and itself returns
UNSAT/20, so the remaining two pairs and the optional solve tail are not
reached. This is the same first-action reach reported by the old driver's
diagnostic line.

| Source/timing | SLURM job | Result against original CNF | LRAT SHA-256 |
|---|---:|---|---|
| Current AM355 Debug, early | 1717353 | `s VERIFIED UNSAT` | `29e9bb28a3e95bf6f5df17fd1e61cce6c0d6c36d78b86f97171322ec908db54e` |
| Current AM355 Debug, late | 1717354 | `s VERIFIED UNSAT` | `eea8a61f609d1b5fb5af010579a37e19453b8bdd40d920c20f4bb5f5342a1` |
| AM349 repaired Release driver, reconstructed late command | 1717355 | `s VERIFIED UNSAT` | `b5e48609b225217155bbf0c4d19eb614b070563079d43bd5cbe00f7eef83cd45` |

All three jobs used `cpu-dense-hwulab-q`, one CPU, 8 GiB and a ten-minute
limit, and ended COMPLETED/0:0. Exact checker success text, rather than the
job state, is the verdict. The pinned CakeLPR hash is `ab9dc6c4...`.
The current early/late proofs derive the distinct-literal version of input
clause 2 from original ID 2 before using it; the late proof also derives a
second valid reconciliation copy. The old command verifies despite the
handoff's stated rejection, so this reconstructed command is **not a valid
negative control**. No causal claim that AM350 or AM355 repaired the reported
`idx142` failure follows from it. The original owner's exact failing command,
proof, and first checker failure location are still needed to close that
reported failure. This does not reopen the separately reproduced and repaired
canonical H1/H4 checks in the AM356 report.

The handoff's **other** `idx142` claim *does* have a reproducible negative
control: AM349's base `build-base` canonical-late driver (SHA-256
`78a69b54...`) returned UNSAT in job **1717357**, but CakeLPR rejected its
proof at line 7 (`clause not empty or singleton after reduction`; proof
SHA-256 `196ab760f46de0a63c5ca484d8bc9ea67171ecb810de9b29a75970958cd88e32`).
The current AM355 Debug canonical-late public driver returned UNSAT and
CakeLPR verified against the same original CNF in job **1717358** (proof
SHA-256 `e61f6ab8459be3db2f17c5c7fc7edd35c2fb7227cf2950f421bfff1ab6dc76cd`).
Both jobs were COMPLETED/0:0, illustrating again why SLURM completion is not
proof acceptance. This closes the *canonical* `idx142` reproduction on the
current pin; it does not supply the missing rejected semantic certificate.

The diagnostic source, five proof files and SLURM logs remain under
`benchmarks/probes/scratch/am356-h4.KFk3Ke/`. The earlier canonical driver
was restored byte-identically to the SHA-256 recorded by AM356; the semantic
driver is a separate binary. No Tier D task was submitted.
