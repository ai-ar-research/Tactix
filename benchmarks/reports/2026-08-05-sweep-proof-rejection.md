# Sweep emits an invalid refutation — the Tier C solve "Sweep costs" is an N2 defect, not a schedule

**Date:** 2026-08-05 · **Frozen on writing** (doc discipline: reports are never edited).

**What was asked:** paired draws on the Tier C instance that V5 Sweep's scheduling gate (S-f) costs, to
justify the shipped decision to keep Sweep scheduled at 100 per mille.

**What was found:** that instance does not lose the solve to search. It loses it to a **rejected proof**,
and the rejection is deterministic and perfectly separated by Sweep — **14/14 Sweep-ON draws rejected,
14/14 Sweep-OFF draws verified**. Two independent checkers reject it in two proof formats. This is an
**N2 breach** (proof certificates on every path), and it is live at HEAD.

---

## 1. Provenance

| | |
|---|---|
| Source window | `20260805-002318-C-sf2`, git **`60c5671`** (from that run's own `manifest.json`) |
| Allocation shape | `partition=cpu-dense-hwulab-q cpus/task=1 mem/cpu=8G throttle=%40`, `--nodelist=node017` |
| Instance | `tierC/asconhashv12_opt64_H4_M2-LOD9_m0_2_U19.c.cnf.xz` (UNSAT, 126 995 vars / 297 208 clauses) |
| Paired draws | array **`1583887`**, 28 tasks, node017, `build/pin-sf/tactix` (sha256 `15e98af61aebb22d`) = the exact binary that window measured |
| HEAD reproduction | array **`1583915`** tasks 1–2, `build/pin-sfn2/tactix` (sha256 `14296a31d92f8eeb`), built at **`52a65ca`** — `git diff d9fa5d1..52a65ca -- src/ include/` is **empty**, so this is `d9fa5d1`'s solver behaviour |
| HEAD LRAT | array **`1583921`**, same binary, `--lrat` + cake_lpr |
| Archaeology | array **`1583919`**, `build/pin-stagebfull/tactix` (sha256 `78db417aa27046fe`) = git `33940b8`, pre-S-f |
| Raw output | `benchmarks/probes/scratch/sfdraw-raw/`, `…/sfn2-raw/` (gitignored, regenerable) |

## 2. The flip was never a timeout — read the instance, not the aggregate

The S-f Tier C headline was ON 406.25 (28/30) vs OFF 339.77 (29/30), i.e. "Sweep costs a solve". The
`results.json` entry for the one instance that flipped says otherwise:

| arm | verdict | solve time | proof bytes | checker | scored |
|---|---|---|---|---|---|
| `sf2` (Sweep ON, 100‰) | UNSAT | **552.794 s** | 988 108 777 | drat-trim **failed in 20.554 s** | **unsolved** |
| `sweepoff` (`TACTIX_SLOT_DISABLE=Sweep`) | UNSAT | 546.427 s | 981 784 646 | drat-trim VERIFIED in 103.248 s | solved |

Sweep ON **answered UNSAT 6.4 s (1.2 %) slower** — inside noise on a 550 s solve — and lost the point to
the certificate. Re-scoring that instance on its answer puts the ON arm at **344.67 PAR-2 (29/30)**
against OFF's 339.77 (29/30): **+4.90, under one 6.72 same-window noise floor.**

The window's own `aggregate.log` said this at the time and it was not acted on:

```
  !! PROOFS REJECTED/MISSING (1): tierC/asconhashv12_… [c verification time: 20.521 seconds]
  …
CORRECTNESS BUG(S) FOUND — stop performance work; follow the fuzz skill's failure procedure.
```

## 3. Paired draws — pre-registered protocol, K′ = 10

12 perturbations mechanically independent of Sweep (Sweep reads no chrono level, no bump rate/limit, no
minimize depth; no `TACTIX_SWEEP_*` or `TACTIX_SLOT_*` knob is touched), each in both arms, plus a
replication control. Pre-registered before submission: a **clean draw** requires BOTH arms' conflict
counts to move off their own p00 value; floor K′ ≥ 6; add a batch rather than reinterpret the rule.

| draw | perturbation | ON wall | ON confl | ON sweep rnd/equiv | ON proof | OFF wall | OFF confl | OFF proof | clean |
|---|---|---|---|---|---|---|---|---|---|
| p00 `r-*` | — | 710.4 | 2 993 227 | 50 / 49 | **REJECTED** (21 s) | 724.9 | 2 983 029 | VERIFIED (102 s) | — |
| p00 `rep-*` | replication | 712.8 | 2 993 227 | 50 / 49 | **REJECTED** (21 s) | 741.6 | 2 983 029 | VERIFIED (102 s) | — |
| p01 | chrono90 | 662.4 | 2 728 244 | 47 / 176 | **REJECTED** | 646.2 | 2 731 136 | VERIFIED | ✓ |
| p02 | chrono105 | 636.4 | 2 739 012 | 47 / 89 | **REJECTED** | 578.9 | 2 499 784 | VERIFIED | ✓ |
| p03 | chrono115 | 593.6 | 2 509 546 | 45 / 24 | **REJECTED** | 651.9 | 2 737 250 | VERIFIED | ✓ |
| p04 | bumprate17 | 581.0 | 2 695 200 | 47 / 30 | **REJECTED** | 604.2 | 2 695 655 | VERIFIED | ✓ |
| p05 | bumprate19 | 702.6 | 2 993 227 | 50 / 49 | **REJECTED** | 725.8 | 2 983 029 | VERIFIED | inert |
| p06 | bumprate21 | 627.8 | 2 724 702 | 48 / 152 | **REJECTED** | 638.2 | 2 734 644 | VERIFIED | ✓ |
| p07 | bumprate23 | 668.8 | 2 856 577 | 47 / 18 | **REJECTED** | 611.8 | 2 620 591 | VERIFIED | ✓ |
| p08 | bumplim8 | 654.3 | 2 736 873 | 48 / 23 | **REJECTED** | 770.2 | 3 232 320 | VERIFIED | ✓ |
| p09 | bumplim9 | 715.9 | 3 743 217 | 52 / 43 | **REJECTED** | 700.2 | 2 974 554 | VERIFIED | ✓ |
| p10 | bumplim11 | 661.1 | 2 741 177 | 47 / 35 | **REJECTED** | 546.7 | 2 289 417 | VERIFIED | ✓ |
| p11 | bumplim12 | 499.4 | 2 510 765 | 44 / 21 | **REJECTED** | 550.2 | 2 750 085 | VERIFIED | ✓ |
| p12 | mindepth1000 | 595.8 | 2 993 227 | 50 / 49 | **REJECTED** | 583.3 | 2 983 029 | VERIFIED | inert |

**Controls.** Determinism is exact: `r-on`/`rep-on` both 2 993 227 conflicts / 988 MB; `r-off`/`rep-off`
both 2 983 029 / 982 MB. The wall spread between bit-identical runs (710.4 vs 712.8; 724.9 vs 741.6) is a
0.3–2.3 % in-window contention estimate. **Reachability holds both ways:** every ON draw ran Sweep
(44–52 rounds, 18–176 equivalences) and every OFF draw reports `c sweep 0 rounds` — neither arm was
vacuous ([[ab-arm-identical-artefacts]]).

`bumprate19` and `mindepth1000` were **mechanically inert here** (byte-identical trajectories to p00 in
both arms) and are excluded exactly as pre-registered. **K′ = 10**, above the floor of 6; no second batch
is owed.

## 4. Two verdicts, pointing in opposite directions

**(a) The TIMING difference between the arms is a lottery.** Over the 10 clean draws the ON arm is slower
on 5 and faster on 5, spread −115.9 s to +114.4 s on a ~600 s solve. Sweep's effect on this instance's
runtime has no sign. This is the fifth headline decider tested this way and the fifth to come back a
lottery **on the timing question** — the standing prior holds.

**(b) The SOLVE LOSS is not a lottery. It is a defect, and it is perfectly separated.**

> **14/14 Sweep-ON draws → rejected. 14/14 Sweep-OFF draws → verified.**

drat-trim's diagnosis has the same shape every time, at a different lemma each draw:

```
c WARNING: RAT check on proof pivot failed : [7401781] -1704 1456 0
c RAT check failed on all possible pivots
c failed at proof line 5760692 (modulo deletion errors)
```

The rejected lemmas span widths — unit (`-1374 0`), binary (`-1451 -1702 0`, `-1865 1412 0`,
`1423 1788 0`), ternary (`1500 -1782 -2060 0`), 5-ary (`324 -326 -2122 323 -329 0`) — so this is not
confined to the binary clauses an equivalence substitution emits. The four draws that share p00's
trajectory (`r`, `rep`, `p05`, `p12`) fail at the *identical* lemma `[7401781] -1704 1456 0`, which is
the determinism control for the defect itself.

## 5. It reproduces at HEAD, in both proof formats, on both checkers

| leg | binary | arm | solve | conflicts | sweep | proof | checker | result |
|---|---|---|---|---|---|---|---|---|
| `head-on` | `pin-sfn2` (HEAD) | ON | **123.6 s** | 97 549 | 12 rnd / 9 equiv | 12.5 MB DRAT | drat-trim | **NOT VERIFIED**, line 252 026, lemma `1436 -1004 -1666 -329 0` |
| `head-off` | `pin-sfn2` (HEAD) | OFF | 113.1 s | 89 512 | 0 rnd | 10.1 MB DRAT | drat-trim | VERIFIED (69.8 s) |
| `lrat-head-on` | `pin-sfn2` (HEAD) | ON | 196.5 s | 97 549 | 12 rnd / 9 equiv | 6.11 GB LRAT | cake_lpr | **REJECTED** — `Checking failed at line: 254528. Reason: clause not empty or singleton after reduction` |
| `lrat-head-off` | `pin-sfn2` (HEAD) | OFF | 175.1 s | 89 512 | 0 rnd | 5.61 GB LRAT | cake_lpr | `s VERIFIED UNSAT` (171.9 s) |

Two independent checkers, two proof formats, same separation. **The defect is in the derivation, not in
the DRAT encoding** — cake_lpr consumes explicit antecedents and still cannot reduce the lemma, and it
fails in the same region of the proof (LRAT line 254 528 vs DRAT line 252 026).

**HEAD is also a much cheaper reproduction than the tier window:** 123.6 s and a 12.5 MB DRAT proof
against 552.8 s and 988 MB. (HEAD reaches the same instance in 97 549 conflicts where `60c5671` needed
2 993 227 — that 30× is `SCHED-C7 randec`, landed between the two commits, and is incidental here.)

## 6. Archaeology: S-f exposed this defect, it did not create it

At the pre-S-f build `pin-stagebfull` (git `33940b8`), on this instance:

| leg | conflicts | sweep rounds | proof bytes | checker |
|---|---|---|---|---|
| `pre-on` (default) | 2 731 256 | **0** | 896 502 499 | VERIFIED |
| `pre-off` (`SLOT_DISABLE=Sweep`) | 2 731 256 | **0** | 896 502 499 | VERIFIED |

The two arms are **byte-identical** — normally the signature that an arm was skipped, and here that
identity *is* the finding: **Sweep never fired on `asconhashv12` before S-f.** S-f's contribution was to
give Sweep kissat's 100‰ effort, which is what first reaches this instance (0 → 12–52 rounds). The
emission bug is latent in Sweep; the budget is what made it observable. The archive agrees: every ascon
proof through `33940b8` verified, and the first rejection in the whole Tier C history is at `60c5671`.

## 7. A harness gap this exposed

`tools/bench/run.py:156` keeps only the checker's **last** output line as `proof_msg`. drat-trim always
prints `c verification time: …` last, so `results.json` cannot distinguish

* `s NOT VERIFIED` — a real N2 failure,
* `s MEMORY ALLOCATION ERROR` — checker OOM, the ambiguity `check_proof`'s own docstring warns about,
* `s DERIVATION`.

All three archive as the string `c verification time: 20.521 seconds`. The rejection above was legible
only after re-running the checker and capturing its full output; the initial reading of this record was
that drat-trim had run out of memory on a 988 MB proof, which is exactly the confusion the docstring
predicts. Separately, drat-trim prints `"\rs VERIFIED"` with a leading carriage return, so a
`^`-anchored grep reports every proof as unverified — that cost one collation pass here.

## 8. Consequences

1. **The S-f Tier C evidence against Sweep is withdrawn.** Sweep does not cost a Tier C solve; it costs
   ~1 % of one solve's runtime, and the arms are a wash (+4.90 PAR-2, under one noise floor) once the
   instance is scored on its answer. The Tier B half (ON 92.32 25/28 vs OFF 103.19 24/28, fragility k=2,
   −0.08 ex-fragile) is untouched and remains a wash too.
2. **"Keep Sweep scheduled at 100‰" is now blocked on the defect, not on the PAR-2.** A rule that can
   emit an invalid refutation is an N2 breach whatever its performance. Until it is fixed, the honest
   options are to fix the emission or to back S-f's budget out; the scheduling question cannot be
   settled while the ON arm's certificates are invalid.
3. **Fuzz did not catch it and would not have.** Fuzz was green at HEAD (4000 LRAT / 10000 DRAT, jobs
   `1583885`/`1583886`). The failure needs ≥12 Sweep rounds over a 127 k-variable formula; fuzz cases are
   orders of magnitude too small to schedule Sweep that hard. Same class of gap as the ungoverned-Sweep
   regression: the oracle is right, the *schedule* is never reached. A `stress-policy` leg that forces
   Sweep rounds on a large structured formula is the coverage this needs.
4. **Reproduction for the fix:** HEAD, `benchmarks/cache/tierC/asconhashv12_opt64_H4_M2-LOD9_m0_2_U19.c.cnf.xz`,
   default environment, ~124 s, 12.5 MB DRAT, fails at proof line 252 026. Control:
   `TACTIX_SLOT_DISABLE=Sweep` verifies.
