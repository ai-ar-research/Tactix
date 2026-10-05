# Fixed-44 current-evidence audit

This is a read-only reconciliation of the frozen 44-row Tier-D miss ledger
against saved exact evidence through source `9821d2e96`.  It does not infer a
recovery from a benchmark name, an old coarse matcher, an unchecked solver
verdict, or an opt-in constructor.  No solver or checker was run for this
audit.

## Result

The ledger now contains **three checked recoveries: ranks 147, 630, and 744**.
Rank 147 was the only stale structural-missing classification found.  There is
no remaining row for which the saved evidence supports the narrower claim
“probably already solved by current certified Tactix; confirm with one short
exact preflight.”  The current accounting is therefore:

| Disposition | Count | Ranks |
|---|---:|---|
| Already checked solved | 3 | **147, 630, 744** |
| Likely stale; one short exact preflight warranted | 0 | none |
| True open under the fixed certified-result contract | 41 | all other fixed-44 ranks |

“True open” means that no exact saved artifact closes the row.  It does not
claim that an ordinary CDCL run can never solve it.

## Ranked audit

### 1. Already checked solved

1. **147, Ramsey R(4,4), 18 vertices.** Current exact recognition accepts all
   6,120 source clauses and the current terminal Ramsey derivation returns
   verified UNSAT in 0.167 solver seconds plus 7.705 proof-pipeline seconds.
   The certificate SHA-256 is
   `95542b2c19bc49dfc503eb0a71d23c259122bf998bd967472209edefbe7a68d9`.
   This is the stale ledger entry: the historical LymphoSAT matcher admitted
   only the 19-vertex coarse profile, while current Tactix validates the full
   signed K4 clause image for either 18 or 19 vertices.
2. **630, Ramsey R(4,4), 19 vertices.** The same current certified derivation is
   already checked on decompressed input SHA-256
   `9c8ddd5ad86590ef636f46a61ecf4732abf42dadee70fd2be3c0783c08e7160f`.
   The saved current comparison records 0.167 solver seconds and 7.968 proof
   seconds.  Rank 147 differs only by deletion of the nineteenth vertex: 18
   variables and 1,632 clauses.
3. **744, saved-certificate recovery.** The exact 401,269,091-byte Tactix
   certificate, SHA-256
   `f63206c14c4d7a27d32148defdecebcb7370a145ccd7bdb891064af2853fc7c4`,
   has two independently verified historical records on identical original
   and checker CNFs.  The tighter pipeline took 9,615.897 seconds, inside the
   fixed target's 14,400-second checker deadline.  It does not meet the newer
   1,800-second regression cap.

### 2. Likely stale after one short exact preflight

**None.** The nearest-looking candidates fail the evidentiary test:

1. **263** has already had the relevant repaired-action screen.  All 1,328
   affine requests applied effectively, but the 300-second run returned
   `UNKNOWN` after materializing 63,334 clauses and spending 46.1% of the run
   in propagation service.  This is negative mechanism evidence, not an
   unchecked likely recovery.
2. **401** enters the root-fixed X17 producer 224 times in the focused
   preflight, spending 1,138,621,224 work with zero candidates and zero offers.
   The next step is a bounded detector census and a detector repair.
3. **991** recognizes the graph interval image, but all 232 authenticated
   directions are already live.  X18 adds zero clauses and consumes 58.4% of
   the 30-second run.  The retained constructor's checked SAT result does not
   satisfy the certified Boolean-action contract.
4. **499 and 654** have only old independent-set image signals.  The current
   certified `IndependentSetCover` action requires a caller-selected legal
   cover; no saved current strict-image admission plus native cover producer
   exists.  A recognition-only run could refine attribution but cannot close
   either row.
5. **540, 557, and 896** have historical affine coordinates/candidates, but no
   saved current completed action schedule or checked result.  Existing affine
   target probes show that exact caps, action selection, and downstream
   composition are substantive missing mechanisms, so a short default rerun is
   not evidence-backed as a likely recovery.

### 3. True open rows, ranked by evidence readiness

The first tier has a current exact action boundary and should precede any new
cohort run:

| Priority | Ranks | Exact disposition / next certified mechanism |
|---:|---|---|
| 1 | **263** | Replace repeated static row export with a persistent authenticated GF(2) matrix that exports trail-relevant certified reasons/conflicts. |
| 2 | **401** | Add a one-call bounded X17 detector census, then repair the zero-candidate recognition boundary for root-fixed inputs. |
| 3 | **991** | Publish graph constraints beyond duplicate interval binaries; repeating X18 is ruled out. |

The second tier has a strong saved structural lead but lacks a current native
payload producer or a completing certified composition:

| Priority | Ranks | Missing mechanism |
|---:|---|---|
| 4 | **499, 654** | Strict independent-set image admission, native legal-cover generation, and useful certified cover/count consequences. Rank 654 also needs a compact accepted UNSAT proof. |
| 5 | **540, 557, 896** | Productive certified affine consequence scheduling and composition with the residual scheduling/counting constraints. |
| 6 | **497** | Clause-authenticated comparator-network consequences; fixed phase lists are outside the contract. |
| 7 | **190, 453, 754, 887, 1144, 1161** | Exact domain/n-ary relation recovery and a native offerer. The saved seven-row census produced no action; six rows were policy/guard nonreach and rank 1161 entered once with zero candidates. |
| 8 | **1040** | Exact station-repacking independent-set/domain recovery before any payload can be formed. |

The third tier is certificate-side work.  A complete exact-certificate identity
audit finds no accepted copy for these thirteen rows:

| Priority | Ranks | Missing mechanism |
|---:|---|---|
| 9 | **291, 541, 1129** | Compact proof/checking. Current regeneration is byte-identical to the frozen certificates and all three still exceed the 1,800-second pipeline cap. |
| 10 | **82, 226, 489, 511, 569, 627, 654, 1004, 1075, 1085** | Accepted proof route or materially smaller certified derivation. Rank 654 is also listed in tier 2 because its saved search structure and its certificate failure are independent blockers. |

The final tier has no current exact theory attribution sufficient to justify a
structural preflight:

| Priority | Ranks | Required evidence first |
|---:|---|---|
| 11 | **97, 323, 343, 367, 410, 479, 762, 781, 899, 941, 1009, 1118, 1163** | Source-bound structure or saved-state search attribution; names, gate counts, and winner ownership do not authenticate a Boolean action. |

The tables account for all 44 rows.  Rank 654 appears in two blocker tables but
is counted once: 3 checked plus 41 open.

## Immutable evidence inventory

| Artifact | SHA-256 | What it establishes |
|---|---|---|
| `report-to-andrew/20261003T164617Z-tierd-portfolio-misses/portfolio-misses.csv` | `b2444220c04a4e1a05bbe3d21c4bc73d2e119bad625fb31a9a3ac7d773c0b4f6` | Frozen 44-row denominator and original outcomes |
| `../2026-10-05-rank147-ramsey-recovery/evidence.json` | `f0bfae9b0cd74120f82b4ce452326381533945841d662658da79083c1a2307bf` | Current checked ranks 147/630 comparison and exact inputs |
| `../20261005-gap44-certificate-identity-audit/audit.json` | `ef88f204e7c940fa82fb1d76b4750696a0fb5dcde7ee1ef16d163071dc07410f` | Exact certificate/CNF audit of all 14 proof-only rows; only rank 744 closes |
| `../20261005-gap44-proof-recovery-small4/report.json` | `ad1a1d9a5d9dccceca597be49fafc45cbff2e484cde01125b0cb1a62f2b0c52b` | Byte-identical current regeneration for ranks 291/541/744/1129; zero checks under 1,800 seconds |
| `../2026-10-05-rank263-repeated-affine-screen/manifest.json` | `cbe6a68a517cca9daa8a937b2f9fb2d79ce6191b909afeb30d7525200b885431` | Rank263 repaired lifecycle: 1,328 effective rows, then 300-second `UNKNOWN` and static-export scaling boundary |
| `../20261005-rank401-x17-root-fixed-preflight/report.json` | `dec0fee42d4ef7a8ff19772da4fe488f10db112cb837ad7fb0ffb77d591165c0` | Rank401 224-call zero-candidate preflight |
| `../2026-10-05-rank991-x18-duplicate-preflight/manifest.json` | `5c90dec537c5d7674c703ce6715bcebb7b1b643ee666eb846c3043083400d2cd` | Rank991 duplicate-only X18 preflight |
| `../20261005-gap44-nary-domain-census/census.json` | `957e42c8d22f91e977fe5d492f1683e4dadb42912b01fe6a91c298e8eaca88c1` | Seven-row domain/n-ary no-recovery census and its reach limits |

This reconciliation raises the fixed-ledger checked coverage from 2/44 to
**3/44**.  It does not alter the frozen CSV or `PROGRESS.md`.
