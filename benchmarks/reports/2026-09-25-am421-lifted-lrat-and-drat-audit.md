# AM421 lifted LRAT replay and AM400 DRAT-audit disposition

Date: 2026-09-25. This is a bounded proof-correctness checkpoint, **not** N1--N4 or Tier D acceptance.

## General Sweep repair

Commits `f02e6ef7` and `d7043567` normalize each captured Sweep subsolver LRAT chain against the *complete negation of the emitted lifted lemma*, including its target literal(s). Already-satisfied hints are omitted; each remaining hint must be unit or conflicting, and a non-closing chain declines before kernel mutation. The emitted strip-ledger prefix also omits root-unit hints that the lemma cube already assumes. This is a general proof rule condition, not a case/seed exception. `docs/calculus.md` and `docs/implementation-notes.md` specify the proof obligation. The maintained white-box regression exercises a valid `[1,2]` capture becoming `[2]`, and an invalid remaining hint that must decline.

The first proposed BVE definition-unit replay (`41dd6870`) did **not** repair the archived `c88-R2` proof; that case's actual producer was Sweep's general binary ladder. It was reverted in `a5b78bbc` before the Sweep change. No speculative BVE definition-unit change remains.

The earlier AM417 report owns the separate Instantiate, Strengthen, and BVE root-hint repairs (`9f1c9d6e`, `9433f500`). The final combined source is `d7043567`. A pinned Debug public-API run, SLURM job `1722204` on `cpu-dense-hwulab-q` (4 CPUs, 32 GiB), used driver SHA-256 `315ea0d75ef983c28a5cb18721ee537b63a71d23b9cfa5bee2982075a3df85e2`. Its exact commands, proofs, checker logs and full strict-audit output are in `benchmarks/runs/20260925-am421-final-hint-check/`. On the four unchanged original CNFs `c9291-R3`, `c287-R2`, `c88-R2`, and `h2-sweep`, the archived strict LRAT audit reports **zero failures** and pinned CakeLPR reports `s VERIFIED UNSAT` for each. `c287-R2` includes 41 audited RAT lines. The pre-fix H2 job `1722157` had two `satisfied-by-cube` failures; the pre-Sweep c88 job `1722120` retained its one such failure. Proof-byte changes remove nonpropagating hints rather than adding assumptions to the checker.

Focused Debug `sweep`, `sweep_proofs`, and `micro_proofs` pass 3/3. Fresh Release `sweep`, `bve`, `instantiate`, `strengthen`, `online_api`, `sweep_proofs`, and `micro_proofs` pass 7/7 in `build/am421-hint-release`. Fresh ASan+UBSan Debug `sweep`, `bve`, `instantiate`, and `strengthen` pass 4/4 in `build/am421-hint-asan`. These are focused checks, not full CTest or Claude-owned affected-scope fuzz.

## Independent AM400 DRAT F1 classification

The terminal AM400 owner report identified three parent-identical strict-audit-only flags. Direct replay of its frozen `harness/audit.py` and proofs gives this common sequence:

| Seed / candidate leg | First root conflict | Conflict lost after deletion | Flagged empty addition |
| --- | ---: | ---: | ---: |
| `9439112` / `cdbg` | step 302 | step 320 | step 333 |
| `9500113` / `cdbg` | step 2995 | step 3066 | step 3067 |
| `9911826` / `crel` | step 39 | step 52 | step 53 |

Every addition **before the first root contradiction** is RUP or RAT under the audit's own clause population; the only non-implied addition in each complete stream is the later empty clause. For `9911826`, removing the root-reason clause `(12 v 2 v 6)` at step 52 makes the audit's then-current population satisfiable (explicit 12-variable model found), so its step-53 complaint is internally consistent but omits the earlier, already-sound contradiction. The same pattern holds for the other two frozen proofs. DRAT refutation is established by that valid prefix; later deletions cannot erase the fact that the original formula was already proved UNSAT. The owner's drat-trim backward/forward and CakeLPR results independently verify all three. Thus F1 is a **harness audit-contract false positive after terminal root conflict**, not evidence of a new solver inference or a permission to waive pre-conflict failures. A general future audit fix is to latch the first valid root contradiction, retain grading of every addition up to it, and treat later proof operations as irrelevant to soundness; add positive and negative controls before changing any shared harness. The historical campaign files are left frozen. AM400's F2 out-of-range control is separately a harness expectation mismatch after Probe already refuted the formula, per its terminal handoff.

## Remaining gate

The canonical no-option DSR-to-LSR-to-Trestle H2 rejection is separate and still open; no checker option or route was changed. Current-source affected-scope fuzz, full N1/N2 coverage, N3/N4 performance/parity gates, and the requested post-goal 5,000-second Tier D run remain due. No online-learning readiness claim follows from these proof repairs.
