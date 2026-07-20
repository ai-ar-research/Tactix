# V6 Congruence — Tier A/B evaluation

- **Hardware:** local workstation (darwin), jobs=4.
- **Git:** `worktree-congruence` @ 45c7149 (V6 AND+XOR+ITE congruence closure).
- **Tiers:** A.list (200), B.list (14). Timeouts: A 30 s, B 300 s.
- **Baseline:** the pre-congruence revamp (report `2026-07-19-inprocessing-revamp.md`,
  Tier B PAR-2 **111.6**, solved **12/14**) and kissat 4.0.4.

## Tier A (correctness gate)

| config | PAR-2 | solved | proofs |
|---|---|---|---|
| tactix (congruence ON) | 0.50 | **200/200** (SAT 82, UNSAT 118) | 118/118 checked |

No correctness regression; every UNSAT proof drat-trim VERIFIED.

## Tier B

| config | PAR-2 | solved | notes |
|---|---|---|---|
| revamp (no congruence) | 111.6 | 12/14 | prior baseline |
| **tactix (congruence ON)** | **77.75** | **13/14** | **+1: hwmcc-miters cracked** |
| kissat 4.0.4 | 6.96 | 14/14 | reference |

**Congruence is net-positive: PAR-2 111.6 → 77.75 (−30 %), +1 solved.**

### Per-instance highlights (tactix congruence vs kissat, seconds)

| instance | tactix | kissat | note |
|---|---|---|---|
| hwmcc17miters-…-oski15a08b00s | **105.9 UNSAT ✓** | 2.6 | was a permanent TO; **congruence-essential** (kissat can't solve it congruence-OFF); 92 MB proof VERIFIED |
| clique_n2_k10 | **300 TO ✗** | 5.5 | the one unsolved — **search-bound** (kissat solves it congruence-OFF in 44 s); no ternary clauses, congruence n/a |
| FmlaEquivChain_4_6_6 | 41.1 | 8.9 | equivalence-chain miter |
| mp1-blockpuzzle_5x10 | 189.3 | 8.6 | NOT congruence: it extracts 447→1760 gates but adds **0 equivalences**; the 965 MB proof + runtime are pre-existing (BVE/Substitute/search) |
| 9vliw_bp_mc / x9-08014 / square.2.0 | 13.7 / 35.1 / 5.7 | 7.4 / 8.5 / 2.9 | |

## Interpretation

- Congruence delivers exactly the intended capability: it **cracks hwmcc-miters**,
  the congruence-essential instance that no amount of search quality reaches (kissat
  itself times out on it with `--congruence=false`). Proof VERIFIED (N2 intact).
- Where gate-congruence does not apply, congruence is a **cheap no-op** (extracts
  gates, adds 0 equivalences, e.g. mp1-blockpuzzle) — it does not regress instances.
- **clique** remains the one TO: it is *search*-bound, not congruence-bound (kissat
  solves it congruence-OFF in 44 s; it has no ternary clauses for gate extraction).
  That is a separate search-quality gap (the general ~2.2× CaDiCaL gap on Tier C),
  not addressed here.
- The ~11× aggregate gap to kissat (77.75 vs 6.96) is the pre-existing search-quality
  gap; congruence *narrowed* it (from the 111.6 baseline).

## Caveats / deferred

- **Proof size**: congruence emits large DRAT proofs on gate-rich miters (hwmcc
  92 MB, FmlaEquivChain 184 MB). They verify, but proof-size/productivity tuning is
  a follow-up.
- **Performance**: ~50× kissat on hwmcc (101 s vs 2.1 s). The closure over ~2 M gates
  + Substitute + proof I/O dominate; the shared clause index was a minor win.
- **LRAT**: congruence is a sound **no-op under `--lrat`** today (bridged/XOR/ITE
  matches need native-LRAT chains; tracked). DRAT gets the full closure.
