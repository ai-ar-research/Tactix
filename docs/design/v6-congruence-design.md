# V6 Congruence — gate congruence closure (design, in progress)

> **Frozen snapshot — do not edit.** A design/reference record from when this
> work was done; it is *not* kept in sync. Living truth: `docs/calculus.md` (what
> the rules are), `DEVELOPMENT_PLAN.md` (scope/roadmap), `PROGRESS.md` (status).

**Rule:** V6 `Congruence` (plan §5.2 catalog, line ~497) — proof-delta **add-RUP +
delete**, `RuleTier::Inprocess`. Planned **phase 4**; **pulled forward** to
phase 3 (user-directed 2026-07-19) to attack the T3 gap. New in Kissat 4.

## Motivation

The inprocessing revamp (`SLOT_IN_DEFAULT=true`) is net-positive but T3 is not met
(~2.2× clean CaDiCaL, `benchmarks/reports/2026-07-19-inprocessing-revamp.md`). The
dominant gap is a handful of structured UNSAT instances our current rules **cannot
crack at any budget** — the equivalence-checking **miters** `hwmcc-miters` and
`clique` (kissat 2.3/4.5 s; ours TO). Measured dead ends (2026-07-19, commit
`0f9acf1`): the probing V-rules (Probe/Vivify/Backbone) — net-negative + don't
crack them; `Substitute` (binary-graph SCC equivalence) — doesn't crack them
either. Miters are (circuit₁ XOR circuit₂); their equivalences live in the **gate
structure**, not the binary implication graph — so **gate congruence closure** is
the technique that matches: extract the gate definitions, find gates with equal
inputs, merge their equivalent outputs. This is exactly how kissat/CaDiCaL solve
these fast.

## What it does

1. **Extract gates** — recognize AND/OR, XOR, ITE (and definition) gate
   definitions `out = op(inputs)` from the clause database.
2. **Structurally hash** gates by `(type, inputs)`; two gates with the same
   `(type, inputs)` have **equivalent outputs** → derive `out₁ == out₂`.
3. **Substitute** the derived equivalences (rewrite one output to the other),
   which collapses the two circuits of a miter and refutes it fast.

Proof: **RUP-only** (plan) — the derived equivalence binaries `(¬a∨b),(a∨¬b)` are
RUP from the two gates' clauses; rewritten clauses are RUP + `delete`, exactly the
V3 Substitute discipline. (The XOR case is the proof worry — research agent C is
verifying it stays RUP or needs RUP-lemmas.)

## Reuse (the reason this is feasible, not a 4636-line rewrite)

- **Gate detection** — E1c `bve_detect_gate` (`src/rules/bve/bve.cpp`) already
  finds equivalence + AND/OR gates from a variable's clauses; extend to a GLOBAL
  gate table + XOR/ITE + hashing.
- **Equivalence substitution + reconstruction + proof** — V3 `Substitute`
  (`src/rules/substitute/substitute.cpp`): `do_substitution` rewrites
  equivalent literals to representatives with LRAT chains + ρ witnesses. Feed
  congruence's `a==b` equivalences into it (or a shared helper).
- **Occurrence access** — the dense `OccIndex` (`Kernel::dense`, the revamp) for
  scanning clauses by literal during gate extraction.
- **ρ reconstruction** — `kernel_push_witness`/`kernel_extend_model` (merged
  outputs are eliminated vars, like V3/BVE).

So Congruence ≈ **gate extraction (extend E1c) + structural hashing (new) + V3
substitution (reuse) + RUP proof (reuse the add-RUP+delete discipline)**.

## Non-negotiables

- **N1**: `Congruence` registers as one rule with a pure guard (quiescent root,
  like the other inprocessing rules) + apply + the canonical instance;
  independently applicable via the public API. The dense index stays an optional
  acceleration (arena-scan fallback).
- **N2**: RUP + delete proof on every path (the load-bearing correctness item —
  agent C verifies the XOR case).
- **N3**: it EXISTS to move N3/T3 forward; adopt only if measured net-positive
  (it must crack the miters without disrupting SAT — unlike the V-rules, gate
  substitution changes the formula deterministically toward a smaller equivalent
  one, so the SAT-disruption risk is lower, but MEASURE).

## Architecture (from the 3 research agents; transliterate KISSAT — RUP-native, simpler)

**The key simplification (kissat's actual design): Congruence does NOT substitute.**
It extracts gates, hashes them by `(type, sorted inputs)`, and when two gates share
inputs it **adds the two equivalence binaries `(¬a∨b),(a∨¬b)`** to the formula
(making `a==b` explicit in CNF) — then STOPS. The whole-formula collapse is done by
**V3 `Substitute`**, run next in the same phase: its SCC now sees `a↔b` in the
binary graph (Congruence just put it there — the exact reason Substitute alone
couldn't crack the miters), and its `big_path_chain` LRAT works because the
equivalence binaries are physically present. So **no change to `do_substitution`
and no reconstruction in Congruence** (equivalent-literal substitution needs no
witness — the merged var takes its representative's value; Substitute's existing ρ
handles it). Congruence's ONLY new proof obligation is emitting the equivalence
binaries.

### Congruence rule = extract → hash → add-equivalence-binaries
1. **Gate struct** `{GateType type; Lit output; std::vector<Lit> inputs (sorted)}`
   + a global extraction pass. Lift the AND/OR + EQUIV recognizers from
   `bve_detect_gate` (`bve.cpp:149-215`) but emit a `Gate` instead of clause masks.
   Scan via the dense `OccIndex` (arena fallback). AND gate `a=AND(x…)` = long base
   `(a∨¬x1∨…)` + input binaries `(¬a∨xi)` (kissat `2526`, `new_and_gate 1005`).
2. **Structural hash** `(type, sorted inputs) → gate`. On a collision with a
   different output, the two outputs are equivalent → record the pair `(a,b)`
   (kissat `find_gate 486`, `merge_literals 877`).
3. **Add the equivalence binaries** `(¬a∨b),(a∨¬b)` via `kernel_add_binary_clause`
   + proof. **AND/OR: directly single-step RUP** — `(¬a∨b)`: assume `a,¬b`;
   `a`'s binaries `(¬a∨xi)` force all `xi`; `b`'s long clause `(b∨¬x1∨…)` conflicts
   (agent C, kissat `626-644`; merges with NO chain, `1020-1024`). DRAT: emit the
   binary (checker finds RUP). LRAT: chain = `[a's input-binary ids] + [b's
   long-clause id]` — constructible from the gate's own clauses (the ids are in
   hand at extraction), so **N2 holds on both formats from increment 1**.
4. Bounds: arity cap, occurrence limit, a tick budget; `TERMINATED`-style checks.

### What stays reused (agent B's map, unchanged)
Quiescent-root guard, `inprocess_reach_root`, dense index, binary enumeration via
watches, `kernel_add_binary_clause`/`kernel_delete_binary` + proof — all as-is.
V3 `Substitute` does the collapse + ρ + subsumption. **No `do_substitution`
change** (kissat design avoids it). Register `TACTIX_REGISTER_RULE(Congruence,
RuleTier::Inprocess, …)`; guard = `!inconsistent && level==0`.

### XOR/ITE (later increment) — bounded RUP chains, not single-step
XOR/ITE equivalences are NOT single-step RUP (the residual is binary clauses with
no unit to seed UP). kissat adds a bounded **intermediate-lemma chain**
(`add_xor_matching_proof_chain 1084`, `add_ite_matching_proof_chain 1224`):
emit `(¬a∨b∨cube)`/`(a∨¬b∨cube)` over the `2^(k-1)` sign patterns of the shared
inputs (each RUP), resolve inputs away, then delete the chain — still RUP + delete,
no RAT (arity capped ≤4). Deferred to increment 3.

## Increment plan (revamp/rule-dev discipline; miter as the canary)

1. **AND/OR-gate congruence** — the new `Congruence` rule (extract AND/OR gates →
   hash → add equivalence binaries, DRAT **and** native-LRAT chain) + enable
   `Substitute` in the phase composition. Micro proof tests (a hand-built
   congruent-AND pair → equivalence binary, drat-trim + lrat-check). **Canary:
   does it crack `hwmcc-miters`/`clique`?** (they're mostly AND/OR circuit gates).
   Gate on soundness (DefaultPolicy DRAT + native-LRAT fuzz).
2. **Fuzz-harden + Tier B/C** — measure net effect. Congruence changes the formula
   *deterministically toward a smaller equivalent one* (unlike the SAT-disruptive
   probing V-rules), so the SAT-disruption risk is lower — but MEASURE (battleship
   canary). Adopt only if net-positive (N3).
3. **XOR/ITE gates + their proof chains** — if the miters (or Tier C) need parity
   gates. Bigger proof work (the bounded resolution chains).
4. **Tune + finalize** — composition/effort, Tier C (T3 direction), plan re-pin,
   merge.

Increment 1 is the decisive test: if AND/OR-gate congruence + Substitute cracks the
miters, the whole T3 line reopens; if not, the gap is elsewhere and we learn that
cheaply before the XOR/ITE work.
