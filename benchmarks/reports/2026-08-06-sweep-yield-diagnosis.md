# Why V5 Sweep detected almost nothing: 97 % of its dose went to a linear array scan

**Date** 2026-08-06 · **branch** `sweep-yield` · **base** `73af059` · **counters only, no
wall-clock claim anywhere in this report.**
**Jobs** `1587817` (before, shape) · `1587823` (kissat-metrics reference) · `1587828`
(before, effort split) · `1587834` (after) · `1587852` (per-path emission) ·
`1587856`–`1587862` (fuzz).
**Binaries** before `build/pin-sweepdiag` / `…-sweepdiag2`; after `build/pin-sweepstrip`
sha `96b774836c3c9fca`; shipped `build/pin-sweepship`. The closure instrument of §2.3 is an
extra pass over each environment, so it is **off by default** in the shipped rule
(`TACTIX_SWEEP_CLOSURE=1`); every "after" number below was measured with it ON, which makes
those numbers conservative.
**Reference** kissat 4.0.4 `-DMETRICS` variant at `tools/solvers/variants/kissat-metrics/`.
Probe scripts `benchmarks/probes/sweep-yield.{sh,mf,sbatch}`, `sweep-kmetrics.*`,
`sweep-emit-paths.*`; raw output under `benchmarks/probes/scratch/sweep-yield/`
(regenerable).

## 0. The headline

Sweep's yield problem was **not** in the rule's reasoning, its environment semantics, its
sub-solver, or its dosing. **It was in a `for` loop that answered "have I already added this
clause?" by scanning every clause already added.** That scan is charged to Sweep's own effort
currency, and on `b22_1` it consumed **109.2 M of 112.4 M sweep ticks — 97.1 %** — leaving
**1.5 %** for the sub-solver that actually finds equivalences. The dose therefore bought
**174 environments where kissat builds 11,416**, and a rule that never gets to sweep the
formula finds nothing in it.

Replacing the scan with an open-addressed key set, and ranking seeds the way kissat ranks
them, at **exactly the same effort share** on `b22_1`:

| | before | after | kissat |
|---|---|---|---|
| environments swept | **174** | **25 162** | 11 416 |
| equivalences proved | **0** | **239** | 545 |
| equivalence binaries emitted with a proof | **0** | **478** | (1 090 — two per equivalence, inferred, kissat has no counter for it) |
| sweep ticks spent | 112 425 691 | 110 811 311 | (62.9 M kitten ticks) |
| share of those ticks in the dedup scan | **97.1 %** | 12.4 % | — |
| share in the sub-solver | 1.5 % | **44.3 %** | — |

## 1. The hypothesis the brief led with is REFUTED, and cheaply

`S-c`'s deviation (1) keeps root-false literals in environment clauses where kissat strips
them. A kept root-false literal is a free variable in the sub-solver, which would make every
environment strictly weaker. The counter `sweep_env_false` says how often that actually
happens:

| instance / arm | environment literal occurrences | of them root-FALSE | of those, with no unit id (genuinely FREE) |
|---|---|---|---|
| `b22_1` shipped | 113 354 | **0** | 0 |
| `b22_1` sweepfree | 25 259 901 | 2 206 (0.009 %) | 420 |
| `mp1-klieber` shipped | 595 806 | 4 | 1 |
| `mod4block` shipped | 3 668 602 | 2 198 (0.06 %) | **0** |

**Zero root-false literals in the entire shipped `b22_1` environment population.** The
mechanism is real in principle and is now fixed anyway (§4.1), but it was **inert**: it cannot
have caused a single missed equivalence on the exhibit the brief names. Recorded here so the
next session does not build it a second time.

The reason it is inert is worth one line, because the ledger got it backwards: the sweeper
runs at a *drained root*, where a clause with a false literal is a clause the search has
already touched and simplification has usually already rewritten. The deviation's stated cost
("a weaker environment") is real per occurrence and there are almost no occurrences.

## 2. What the counters actually said

New counters (`c sweepdiag`, `c sweepcost`; `Stats` in `kernel/kernel.hpp`). Six before-arms,
900 s cap, one node.

### 2.1 Outcome of every candidate direction — the sub-solver is not declining

| instance / arm | candidate directions solved | MODELLED (`Sat`) | REFUTED (`Unsat`) | `Budget` | equivalences |
|---|---|---|---|---|---|
| `b22_1` shipped | 4 026 | 3 810 | 216 (5.4 %) | **0** | 0 |
| `b22_1` sweepfree | 1 219 099 | 1 216 924 | 2 175 (0.18 %) | **0** | 1 |
| `mp1-klieber` shipped | 28 481 | 27 906 | 575 (2.0 %) | **0** | 2 |
| `mp1-klieber` sweepfree | 377 779 | 356 399 | 21 380 (5.7 %) | **0** | 3 |
| `mod4block` shipped | 23 227 | 23 226 | **1** | **0** | 0 |
| `mod4block` sweepfree | 104 839 | 104 460 | 379 | **0** | 0 |

Three things fall out immediately.

1. **`Budget` is 0 everywhere.** The brief's third candidate — "the sub-solver returns `Budget`
   where kitten refutes, so we silently decline refutations" — is **refuted**. Not one solve in
   1.7 M was abandoned on the tick budget.
2. **The failures are MODELS, not timeouts.** The environment admits an assignment that
   separates the pair. That is the signature of an environment too small to constrain, or of a
   pair that was never a plausible candidate — not of a weak sub-solver.
3. **Bigger environments made it worse.** `sweepfree` (kissat's *maximum* caps, 8 192 vars /
   32 768 clauses / depth 3, ×1000 effort, per-environment bounds lifted) drops `b22_1`'s
   refutation rate from 5.4 % to **0.18 %**. The N4 audit read that arm as "the zero survives
   1000× the dose"; the sharper reading is that **the dose was never the lever and a bigger
   environment is actively counter-productive**, because it floods the candidate list with
   literals from the outer shell.

### 2.2 Where the effort went — the actual defect

`sweep_ticks` is one number covering three activities. Split:

| instance / arm | total sweep ticks | dedup scan | sub-solver | BFS scan (remainder) |
|---|---|---|---|---|
| `b22_1` shipped | 112 425 691 | **109 214 883 (97.1 %)** | 1 630 363 (1.5 %) | 1.4 % |
| `b22_1` sweepfree | 111 815 463 693 | 93 345 201 428 (83.5 %) | 18 406 991 153 (16.5 %) | ~0 |
| `mp1-klieber` shipped | 1 368 792 240 | **1 342 550 870 (98.1 %)** | 5 814 644 (0.42 %) | 1.5 % |
| `mp1-klieber` sweepfree | 1 103 661 477 484 | 1 092 343 769 962 (99.0 %) | 11 041 800 829 (1.0 %) | ~0 |
| `mod4block` shipped | 1 025 555 469 | 491 363 500 (47.9 %) | 27 513 683 (2.7 %) | 49.4 % |
| `mod4block` sweepfree | 1 019 997 040 867 | 1 017 420 836 583 (99.7 %) | 1 636 140 791 (0.16 %) | ~0 |

`env_add` began:

```
  *ticks += e.keys.size();
  for (const uint64_t x : e.keys)
    if (x == key)
      return false;   // already encoded (a binary reached from its other end)
```

`e.keys` holds one entry per clause already encoded, and this ran for **every clause
considered**, including the great majority that the variable cap then declined. Cost is
`O(clauses considered × clauses encoded)`. With a 1 024-clause cap and an occurrence walk that
considers tens of thousands, that is the entire dose. Both references do this in O(1) —
kissat marks the clause (`c->swept`, `sweep.c:290-311`) and the variable
(`depths[idx]`, `:227-241`), cleared per environment.

The consequence is a **coverage collapse**, and coverage is where kissat's yield comes from:

| | kissat `b22_1` | tactix before | ratio |
|---|---|---|---|
| environments (seeds swept) | 11 416 | 174 | **66×** |
| effort per environment | 5 514 kitten ticks | 646 124 sweep ticks | **117×** |
| equivalence solves per environment | 4.55 | 23.1 | 5× |
| refutation rate per solve | 16.5 % | 5.4 % | 3× |
| equivalences per environment | 0.048 | 0 | — |

kissat sweeps **many cheap environments shallowly**; we swept **a few expensive ones hard**.
Both halves of that were wrong.

### 2.3 Environment CLOSURE — why the remaining candidates are bad

`sweep_occ_env / sweep_occ_f` is, over all environment variables, the fraction of a variable's
large-clause neighbourhood that the environment actually holds.

| instance / arm | occurrences held | variables holding their WHOLE neighbourhood |
|---|---|---|
| `b22_1` shipped | 16 392 / 212 054 = **7.7 %** | 2 772 / 44 533 = **6.2 %** |
| `mp1-klieber` shipped | 38 376 / 2 485 587 = **1.5 %** | 3 666 / 271 360 = 1.4 % |
| `mod4block` shipped | 3 654 174 / 63 948 159 = 5.7 % | 7 457 / 112 624 = 6.6 % |

An environment is a ball, so 93 % of its variables arrive with the one clause that introduced
them and nothing can be proved about them. **We queried them first**: the candidate list was
seeded in BFS discovery order and the loop pops from the **back**, i.e. the outermost shell,
and `max_candidates = 32` cut the loop off there. The shipped arm therefore spent its entire
per-environment query budget on the variables least able to pay.

## 3. The reference column, in full

kissat 4.0.4 `-DMETRICS`, same instances, 900 s (job `1587823`).

| | `b22_1` | `mp1-klieber` | `mod4block` |
|---|---|---|---|
| `sweep` calls | 24 | 8 | 8 |
| `sweep_variables` (environments) | 11 416 | 17 699 | 978 |
| env vars per environment | 157.8 | 254.4 | 239.9 |
| env clauses per environment | 241.2 | 264.2 | 2 114.7 |
| realized depth | 2.01 | 2.05 | 1.98 |
| `sweep_solved_equivalences` | 51 949 | 65 537 | 2 044 |
| of those, refuted | ~16.5 % | 17 % | 2.6 % |
| `sweep_equivalences` | **545** | **741** | **2** |
| `sweep_units` | 3 | 0 | 5 |
| `kitten_ticks` | 62 946 445 | — | — |

**Two corrections to the brief this forces.**

1. **`mod4block` is not an exhibit of a yield gap.** kissat finds **2** equivalences and 5
   units on it, from 978 environments. "234 rounds, 3 293 environments, 4.72e9 ticks → 0"
   describes an instance where the reference gets essentially nothing either. It should be
   dropped from the exhibit set; keeping it invites tuning against noise.
2. kissat's environments are the **initial** cap size (158/241 vars/clauses against options
   `sweepvars` 256 / `sweepclauses` 1024) — it never reaches the maximum caps on these
   instances, because `sweep_completed` is 1 or 2. Our `sweepfree` arm was therefore not "the
   generous version of kissat"; it was a configuration kissat never runs.

## 4. The fix

Three changes to `src/rules/sweep/`, no kernel change, no policy change, no proof-delta change
beyond §4.1's citation.

### 4.1 Root-false literals are stripped, and the strip is CITED (`SP-SWEEP-8`)

Kept for correctness parity with the references even though §1 measures it inert, because it
also frees environment-variable slots. `C \ {l}` for a root-false `l` follows from `C` and the
root unit `{¬l}`. The soundness argument moves from "`env ⊆ F`" to "`env` is **entailed by**
`F`", and the emission side pays for the difference:

- **LRAT.** Each environment clause carries the proof ids of the units that justify its
  removals (`SweepEnv::strip_ids`, a CSR parallel to `clause_ids`, captured into
  `SweepDirProof` with `orig_ids` because the environment does not outlive its seed). A step's
  strip units are emitted **at the front** of its hint chain, deduplicated. A unit clause is
  unit under every assignment, so it is valid at any position in an LRAT hint list; the front
  is the only position that does not depend on which antecedent needs which unit.
- **DRAT / no proof.** Nothing to cite: the checker re-derives every root unit by propagation
  (every root unit is either an input clause, a `PendingUnits::record` unit that was emitted,
  or unit-propagated from clauses the checker holds), so the stripped clause propagates exactly
  as the original does and every lemma stays RUP.
- **The one case that keeps a literal** is an LRAT run with `unit_ids[v] == 0`. That is
  possible only where `propagate_service::emit_root_units` has not run, and it is a weakening,
  never an unsoundness. Measured: **0 of 4 193 root-false occurrences on `b22_1` after**.
- Off arm: `TACTIX_SWEEP_STRIP=0`.

`docs/calculus.md` V5 and `docs/implementation-notes.md` are updated: the precondition wording
("subset of F" → "entailed by F") and the proof delta (the chain must cite the strip units)
both moved.

### 4.2 O(1) clause dedup (`SP-SWEEP-4`)

`SweepEnv::key_slots`: an open-addressed set of the clause keys already encoded, sized so it
never passes half full (at most `max_env_clauses` insertions), `UINT64_MAX` as the empty slot.
Probe and record are separate steps, because a clause that passes dedup can still be declined
by the variable cap and must stay re-addable — `fresh` shrinks as its other variables enter the
environment.

### 4.3 Seed order and candidate order (`SP-SWEEP-5`, `SP-SWEEP-6`)

- `sweep_seeds` now **ranks** by ascending occurrence degree with single-polarity variables
  last (kissat `scheduable_variable` + its radix sort, `sweep.c:1478-1528`). **Ranked, not
  filtered**: kissat *drops* both classes, but `instances()` is the policy's action space and
  must stay total (N1), so they sort to the back instead. `std::sort` on `(rank, var)` — a
  total, deterministic order without `stable_sort`'s per-round `n/2` temporary.
- The candidate list is reversed before the query loop, so the loop's `pop_back` takes the
  **nearest** candidate first instead of the outermost.

## 5. Before / after on the three exhibits, against kissat

Same 900 s cap, same node type, same effort share, counters only.

Shipped-dose arms:

| | `b22_1` | | | `mp1-klieber` | | | `mod4block` | | |
|---|---|---|---|---|---|---|---|---|---|
| | before | after | kissat | before | after | kissat | before | after | kissat |
| environments | 174 | **25 162** | 11 416 | 1 060 | **43 450** | 17 699 | 726 | 137 | 978 |
| candidate directions | 4 026 | 182 291 | 51 949 | 28 481 | 769 144 | 65 537 | 23 227 | 2 667 | 2 044 |
| refutation rate | 5.4 % | **12.5 %** | 16.5 % | 2.0 % | **7.9 %** | 17 % | 0.004 % | 2.7 % | 2.6 % |
| **equivalences** | **0** | **239** | **545** | **2** | **45** | **741** | **0** | **1** | **2** |
| **binaries emitted** | **0** | **478** | — | 4 | **90** | — | 0 | **2** | — |
| dedup share of ticks | 97.1 % | 12.4 % | — | 98.1 % | 16.5 % | — | 47.9 % | 0.14 % | — |
| sub-solver share | 1.5 % | 44.3 % | — | 0.42 % | 13.0 % | — | 2.7 % | 3.3 % | — |

And the `sweepfree` arm on `b22_1`, which is where the emission paths get their volume:

| `b22_1` sweepfree | before | after |
|---|---|---|
| environments | 1 204 | **337 286** |
| **equivalences** | **1** | **7 125** |
| **binaries emitted** | **2** | **10 278** |
| of those, via the GENERAL core→chain path | 0 pairs | **837 pairs** (2 823 declined by RUP) |
| dedup share of ticks | 83.5 % | **3.2 %** |
| sub-solver share | 16.5 % | **85.5 %** |

`sweepfree` is a **diagnostic arm and nothing else** — it gives Sweep a ×1000 effort share, so
after the fix it eats the solve (`mod4block`'s sweepfree arm now returns UNKNOWN where it
previously answered SAT, and reports the first `budget 348` events in this whole pass). It is
in the table because it is where the emission paths get their volume and because it is the arm
the N4 audit read "the zero survives 1000× the dose" off. Nothing here is a dosing
recommendation.

`b22_1` is the clean read, because it is UNSAT on both arms with the same verdict and the same
51 rounds: **0 → 239 equivalences and 0 → 478 emitted binaries for 1.4 % FEWER ticks.** We now
sweep 2.2× kissat's environment count and reach 44 % of its equivalence yield; the remaining
gap is `SP-SWEEP-7` (§7). `mp1-klieber` moves 2 → 45 equivalences (kissat 741) at an unchanged
dose, so the same mechanism, same direction, smaller fraction — its closure is the worst of the
three (1.5 %), which is what `SP-SWEEP-7` addresses.

`mod4block` reaches parity with kissat (1 against 2) and is, per §3, not a gap instance. Its
environment count falls because it is SAT and the trajectory diverges — do not read that row as
a regression, and do not read any of these rows as timing.

## 6. N2 evidence

- **`ctest` 54/54**, including `micro_proofs` and `sweep_proofs` — the latter runs S-e's real
  emitted proof through drat-trim **and** lrat-check, and is the test that catches a translation
  defect of the kind the 2026-08-05 polarity breach was.
- **Fuzz: seven legs on the final binary (`build/debug`, sanitizers + `TACTIX_CHECK`),
  0 failures.** The last two are the only ones that route *every* proved equivalence through the
  general core→chain path, i.e. the path §4.1's citation change lives in.

  | job | leg | cases | failures | `Sweep` reached |
  |---|---|---|---|---|
  | `1587856` | `--random-policy`, native DRAT | 4 000 | **0** | 2 284 / 3 325 |
  | `1587857` | `--random-policy --lrat`, native LRAT | 4 000 | **0** | 2 286 / 3 327 |
  | `1587858` | `--random-policy --no-binary`, ASCII DRAT | 3 000 | **0** | 1 718 / 2 506 |
  | `1587859` | `--sweep-stress-policy --lrat` | 3 000 | **0** | 1 872 / 2 658 |
  | `1587860` | `--sweep-stress-policy`, DRAT | 3 000 | **0** | 1 872 / 2 658 |
  | `1587861` | `--sweep-stress-policy --lrat` × `SWEEP_FORCE_GENERAL` | 3 000 | **0** | 1 873 / 2 659 |
  | `1587862` | `--sweep-stress-policy`, DRAT × `SWEEP_FORCE_GENERAL` | 3 000 | **0** | 1 876 / 2 662 |

  The reachability column is what keeps these from being vacuous; `tools/fuzz/failures/` holds
  nothing but the standing `nonterminating` population.
- **Per-path emission counts, non-zero on BOTH paths and BOTH formats, with every UNSAT proof
  checked** (job `1587852`, 60 Tier-A instances per format, `--sweep-stress-policy`, half of them
  under `TACTIX_SWEEP_FORCE_GENERAL=1`):

  | format | `sweepemit` binaries | not-RUP (declined by S-d, handed to S-e) | general pairs (S-e) | UNSAT proofs checked | **REJECTED** |
  |---|---|---|---|---|---|
  | native DRAT (drat-trim) | 1 218 | 5 376 | 97 | 52 | **0** |
  | native LRAT (lrat-check) | 1 218 | 5 376 | 97 | 52 | **0** |
- Note the standing caveat that a `--random-policy` leg's N is N minus its non-terminating
  population (16.9 % here, printed by the harness).

## 7. What is NOT fixed, and where it goes

The remaining `b22_1` gap is 239 against 545 at 2.2× the environment count, i.e. our
**yield per environment is still ~5× worse**. Two named mechanisms, both rule work, both now in
the ledger as `SP-SWEEP-7`:

1. **Seed-anchored candidates** (deviation 5). One environment can only prove equivalences
   **to its seed**; kissat keeps a partition over *all* environment literals
   (`init_backbone_and_partition` / `sweep_refine_partition`, `sweep.c:474-587`) so one
   environment can prove a pair anywhere inside it.
2. **No `kitten_flip_literal` pre-filter** (deviation 3). kissat refines the partition by
   flipping a literal in the current model, at zero solve cost, before spending a query
   (`sweep.c:1152-1185`). Our `refine` reaches the same partition but pays a full solve per
   step. This needs a "can this literal be flipped" query the sub-solver does not expose.

Also unaddressed and separately ledgered: the **backbone half** (deviation 4 — kissat extracts
root units from the same environments and gets 59 % of its backbone refinement free from
flips), the productivity **delay ladder** (`SP-SWEEP-2`), and the resumable schedule tail
(`SP-SWEEP-3`, whose *rank* half landed here as `SP-SWEEP-5`).

**This is emphatically not a "structurally weaker than kitten" finding.** Our environment
construction is at kissat's shape and, after §4.1, at its semantics; the sub-solver was never
the bottleneck (0 `Budget` in 1.7 M solves). What was structurally wrong was an O(n²) inner
loop and an arbitrary seed order, and both are gone.

## 8. The ready-to-run A/B spec

Not run here — this pass took no timing measurement, and the brief forbids a full campaign.

- **Arms, both from ONE commit, both in ONE submission window, node-pinned, one tier per node:**
  `strip-on` (shipped default) and `strip-off` (`TACTIX_SWEEP_STRIP=0`) is **not** the
  interesting pair — §1 measures it inert. The pair to run is
  **`fix` (shipped default) vs `base` (the `73af059` binary)**, because the three changes are
  not separately hatched and their point is joint.
- **Two pin dirs** (`build/pin-sweepyield-b`, `…-c`), one per array, built from the same commit
  with matching binary shas — the same-window Tier B + Tier C rule.
- `python3 tools/bench/run.py --tier B --slurm --solver build/pin-sweepyield-b/tactix
  --baseline <base-binary> --name sweepyield` and the Tier C twin, submitted together.
- **Primary metric** PAR-2 with the fragility line read first; **secondary** `#solved`, read
  before the aggregate.
- **The one cost this pass could not price is the seed RANK.** `sweep_seeds` now sorts the whole
  active variable set once per round, and it is charged to nothing — as the un-ranked walk before
  it was not. On `T105.2.0` (~3.1 M variables × ~33 Sweep occasions) that is the worst case, and
  a two-arm same-window counter probe on it is §10. If the A/B shows a Tier C loss
  concentrated on the largest formulas, this is the first suspect, and the fix is a radix rank
  (kissat's `RADIX_STACK`) plus charging the walk to `sweep_ticks`.
- **Pre-registered per-instance guard:** two byte-identical arms in one window differ by up to
  **41 s on a single instance**, so no single-instance claim below that is admissible. `b22_1`
  is the instance where the mechanism is largest; it is a *reachability* exhibit here, not a
  timing one.
- **Expected direction, stated in advance so the run can falsify it:** Sweep now spends 44 %
  of its (unchanged) dose in the sub-solver instead of 1.5 % in an array scan, and emits
  hundreds of binaries that V3 Substitute then consumes. The plausible loss is trajectory
  churn on instances where those binaries are worthless. A neutral PAR-2 with the yield
  restored is a pass; the yield is what the next Sweep item builds on.

## 9. Reproducing this

```
sbatch --array=1-6 --output=<scratch>/sweep-yield/%A_%a.out \
       --export=ALL,SY_TACTIX=<binary> benchmarks/probes/sweep-yield.sbatch
sbatch --array=1-3 --output=<scratch>/sweep-yield/km/%A_%a.out \
       benchmarks/probes/sweep-kmetrics.sbatch
sbatch --array=1-2 --output=<scratch>/sweep-yield/emit_%A_%a.out \
       benchmarks/probes/sweep-emit-paths.sbatch
```

`sweep-kmetrics.sh` needs `tools/solvers/variants/kissat-metrics/kissat`; all four resolve
the gitignored instance cache through `git worktree list`, so they work from a linked worktree.

## 10. The seed-rank cost check on the largest formula

`sweep_seeds` sorts the whole active variable set once per round, and `T105.2.0` has ~3.1 M
variables. Both arms in ONE submission on ONE node (`node018`), 900 s, counters — job
`1587875`, `benchmarks/probes/sweep-rankcost.sbatch`. RESULT BELOW; if the after arm's
conflict count is not within noise of the before arm's, the radix rank in §8 is owed before
the A/B is worth running.
