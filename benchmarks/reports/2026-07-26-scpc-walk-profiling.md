# SCPC-1000-18: the Tier C timeout is a MISSING RULE — local search (S6 `Walk`)

Date: 2026-07-26 · skill: debug-performance · instance `tierC/SCPC-1000-18.cnf.xz`
Baselines pinned: kissat 4.0.4, cadical 3.0.0 (`tools/solvers/bin/`)
Probes: 1563267–1563284 (single instance), 1563345 (Tier C walk sweep),
1563405 (Tier B walk sweep). Allocation shape: `cpu-dense-hwulab-q`,
1 cpu/task, 8 GB/cpu, one task per arm.
Outcome: plan rev 71 (S6 `Walk` phase 4 → 3); implementation Track W.

## 0. Quantify

| solver | wall | conflicts |
|---|---|---|
| tactix `a20135a` | **TO (1200 s)** | 30.7 M (stable) / 40.3 M (focused) |
| kissat 4.0.4 | 166.4 s | 5,648,737 |
| cadical 3.0.0 | 24.7 s | 703,079 |

In the clean T3 window (`20260725-193123-C-t3clean`) this is one of two unsolved
Tier C instances, and the other (`128_125`) defeats all three solvers — so it is
**the only actionable Tier C timeout**, worth 80 PAR-2 by itself.

It is not a "3× too slow" instance. Given a 3600 s budget (probe 1563267,
3× the tier cap) the pre-Walk solver burned **109,626,468 conflicts /
24.0 G propagations / 1,258,332 restarts and still returned UNKNOWN** — 156×
cadical's conflict count without reaching an answer.

## 1. The instance: an independent-set/covering formula (a WalkSAT landscape)

Measured by scanning the CNF (no solver run):

- `p cnf 1000 51003`; **50,108 binaries, all purely negative** (`-u -v`) — a
  conflict graph on 1000 vertices, edge density ≈ 0.10 (mean degree ≈ 100),
  statistically indistinguishable from G(1000, 0.1); **895 long clauses, all
  purely positive**, lengths 27–77 (mean 54.4).
- "SCPC" decodes exactly: a **set-covering problem with pairwise conflicts** —
  1000 columns, 895 rows, and a conflict graph. A model is an independent set of
  ~53 columns that hits all 895 rows.
- Two consequences. (a) There is **no inprocessing surface**: the binary
  implication graph is bipartite of depth 1, so Substitute / TransitiveReduce /
  Probe / Vivify / Congruence / BCE find nothing structural. (b) The
  all-true initial phase both solvers (and we) start from falsifies essentially
  every one of the 50,108 binaries, while the all-false assignment breaks
  exactly the 895 positive clauses and each true-flip's break count is exactly
  its number of already-true neighbours — **a textbook ProbSAT landscape**.

## 2. Decompose (ladder steps 1–2)

At 1200 s, per-conflict throughput is at parity across all arms — ours 25.6 k
conflicts/s (stable), cadical-minus-walk 24.9 k/s, kissat-minus-walk 31.4 k/s.
The gap is therefore **not** engine speed (consistent with TB5's reframe), and
not restart cadence. It is which assignment the search is trying.

## 3. Ablate the baselines (step 3): a monotone dose–response on walk effort

cadical, one option at a time (default 24.7 s / 703 k conflicts):

| arm | wall | conflicts |
|---|---|---|
| `--walkeffort=800` (10× dose) | **3.0 s** | 45,892 |
| default (`walkeffort=80`) | 24.7 s | 703,079 |
| `--walkeffort=8` (⅒ dose) | 452.7 s | 12,246,349 |
| `--walk=false` | **TO** | 29,701,240 |
| `--walk=false --sweep=false` | **TO** | 30,047,483 |
| `--sweep=false` | 16.5 s | 465,226 |
| `--vivify=false` | 24.3 s | 703,072 |
| `--chrono=0` | 81.1 s | 2,278,126 |
| `--elim=false` | TO | 21,422,099 |
| `--rephase=0` (kills walk's only caller) | 1059.1 s | 27,482,692 |

kissat (default 166.4 s / 5.65 M conflicts):

| arm | wall | conflicts |
|---|---|---|
| `--walkinitially --walkeffort=500` | ~46.7 s | 1,477,082 |
| default (`walkeffort=50`) | 166.4 s | 5,648,737 |
| `--walkeffort=0` | **TO** | 37,539,600 |
| `--sweep=false` | 159.0 s | 5,648,737 (trajectory identical) |
| `--rephase=false` | TO | 36,183,224 |

Four points spanning a 100× effort range and a 150× wall-time range, on two
independent solvers, all monotone: this is causal evidence well past what a
single ablation gives. Sweep and vivification are noise here; chronological
backtracking is worth ~3×; elimination matters (both solvers eliminate ~80 % of
their variables) but we already have it.

## 4. Our search vs the walk-less baselines (step 4)

| arm | 1200 s | conflicts |
|---|---|---|
| tactix (stable-forced) | TO | 30,707,430 |
| tactix (focused-forced) | TO | 40,261,615 |
| cadical `--walk=false` | TO | 29,701,240 |
| kissat `--walkeffort=0` | TO | 37,539,600 |

Our core search is **at parity with both baselines once their local search is
removed** — the same conflict budget, the same failure. The entire gap is the
missing rule. (This is the mp1/Factor lesson of 2026-07-20 repeating: a large
conflict gap that looked like search quality was a missing simplification.)

## 5. Generalization (step 5): what local search is worth on our tiers

kissat, default vs `--walkeffort=0`, whole tiers, same shape:

| tier | walk | no walk | solved |
|---|---|---|---|
| B (300 s cap) | PAR-2 **8.62** | PAR-2 **29.22** | 14/14 both |
| C (1200 s cap) | PAR-2 **221.19** (29/30) | PAR-2 **282.48** (28/30) | SCPC is the flip |

Tier B per instance (`nowalk/walk`): battleship **19.4×**, mod4block **8.4×**,
crypto1 **4.5×**, clique 2.1×, FmlaEquivChain 1.5× — our own historical pain
set. For calibration, our shipped Tier B is 9.93, i.e. **we already beat
walk-less kissat (29.22) by 3×**; essentially the whole remaining 1.20× gap to
kissat is this one mechanism.

Tier C shows the other half of the truth: walk is **not free**. Several
instances are faster without it (jgiraldezlevy 0.08×, frb45 0.15×, EDP3 0.18×,
mod2c 0.40×, j3037 0.49×) — partly SAT-side trajectory luck, partly real
overhead on instances where CDCL alone lands a model quickly. Net it is +22 %
PAR-2 on C and +239 % on B, and **when to walk is a dosing decision** — another
§5.6 exhibit, alongside elimination dosing (rev 68).

## 6. The code-level gap

- `src/policies/default_policy.cpp` rotates `B,B,I,B,B,O`; kissat rotates
  `B,W,I,B,W,O` (`kissat/src/rephase.c:86-92`), cadical reaches walk at 1/3 of
  its rephase slots (`cadical/src/rephase.cpp`). `default_policy.hpp:244` stated
  the substitution outright: "kissat's B,W,I,B,W,O with the Walking slots
  deferred to phase 4".
- The substitution was not neutral in a second way: kissat resets
  `best_assigned` only on type `B`, so its two `W` slots let `best` accumulate
  across two rephase intervals; ours reset it at 4 of 6 slots — we also
  discarded more best-phase evidence than kissat.
- Nothing resembling local search existed in `src/` (`walk|localsearch|probsat`
  matched only `kernel_reason_walk`), and no warmup, random-decision bursts,
  reorder, or sweep.

## 7. Diagnosis and routing

**Cause class 1 — missing proof rule** (here: a missing `H`-only policy
operation), not search quality, not engineering. Routed to:

- **revise-plan** → plan rev 71: S6 `Walk` moved from phase 4 into phase 3.
- **rule-dev** → Track W (`src/policies/ops/walk/`), increments W1–W4; design
  `docs/design/s6-walk-design.md`.
- Deliberately **not** routed: sweep (V5) and vivification stay phase 4 — this
  instance disconfirms them as levers; chronological backtracking is a real but
  3× secondary effect already tracked as TB4b.
