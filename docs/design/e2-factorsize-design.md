# E2 Factor beyond binary clauses (`factorsize`) — design pass

**Status:** design, pre-implementation (2026-07-30). Freeze on landing.
**Motivating measurement:** `benchmarks/reports/2026-07-30-tierB-timeouts-diagnosis.md`
§5a′/R5. **Owner item:** PROGRESS task "Defect B".

## 1. Why

Our E2 Factor is the **binary-clause-driven** variant of BVA: the grid is a set of
binary clauses `(f ∨ r)` with `f ∈ F`, `r ∈ R`, both sets of *literals*. Kissat's
`factorsize` defaults to **5**, i.e. its grid ranges over clauses up to size 5.

The reference-side test is already done and it confirmed, decisively:
`kissat --factorsize=2` — kissat restricted to *our* variant — loses
`or_randxor` entirely (TO @300 s, 10.02 M conflicts, against **12.27 s** at its
default) and costs `unsat-set-b-fclqcolor` **12.6×** (203.94 s). CaDiCaL with
`--no-factor` also loses rxor. So the gap on those two instances is **Factor's
clause-size scope**, not our selection, dosing, or search.

`or_randxor` is the only true wall in the new Tier B: unsolved at 3,600 s after
**42.5 M conflicts = 69× kissat's 617 k**, at engine parity (props/conflict
0.99×). No scheduling change touches it — every dosing lever tried in the last
two days (BCE round/occurrence caps, the uniform slot rate, V4's budget) has
failed, three of them with an explicit *partial-is-worse-than-either-extreme*
signature. This is the one remaining lead that is a **scope** fix rather than a
dosing fix, and it is worth ≈ **40 Tier B(28) PAR-2** — the largest single item
in the diagnosis.

## 2. The generalization, precisely

Today `R` is a set of literals and the grid is `{ (f ∨ r) : f ∈ F, r ∈ R }`.

Generalized, `R` is a set of **remainders** — sub-clauses — and the grid is
`{ ({f} ∪ r) : f ∈ F, r ∈ R }`, where every `r` satisfies `|r| ≤ factorsize − 1`
and `f ∉ r`, `¬f ∉ r` (otherwise the grid clause is shorter than expected or a
tautology). Setting `factorsize = 2` forces `|r| = 1` and recovers exactly
today's algorithm — which is the shipping plan (§5).

The three primitives that change:

| today | generalized |
|---|---|
| `binary_partners(l)` → literals `p` with live binary `(l ∨ p)` | `remainders(l)` → the sub-clause `C ∖ {l}` for each live irredundant `C ∋ l` with `|C| ≤ factorsize` |
| `inR[r]` — a literal-indexed marker | membership over remainders (needs identity — §3) |
| `newR = R ∩ partners(g)` | `newR = R ∩ remainders(g)` — intersection over remainders |

The next-factor candidate `g` remains a **literal**, so `cofactor[h]` stays
literal-indexed and the greedy growth loop keeps its shape. Only the *R* side
becomes structured.

## 3. The one real data-structure problem: remainder identity

Set operations on `R` (`inR`, intersection, dedup) currently work because a
remainder is a literal and can index an array. A sub-clause cannot. Plan:
**intern remainders into dense ids** for the duration of one `factor_apply`.

- Canonical form: the remainder's literals **sorted ascending** (they come from a
  clause, so they are already distinct).
- Intern map: hash of the sorted literal sequence → `RemId` (a dense `uint32_t`),
  plus a flat `vector<Lit>` pool with `(offset,size)` per id. One arena, cleared
  per apply — no per-remainder allocation, following the codebase's
  scratch discipline ([[per-call-linear-scratch-hazard]]: no `O(nvars)`
  fill-init per call; the intern table is sized by *material seen*, and cleared
  by walking the touched ids).
- `inR` becomes a `vector<uint8_t>` indexed by `RemId`, restored by walking the
  ids actually set (never a full clear).

`|r| = 1` collapses this to today's behaviour with one extra indirection, which
is why the `factorsize = 2` default must be verified **bit-identical**, not
merely "close" (§5).

## 4. Proof obligation: UNCHANGED — this is the important part

The five-primitive vocabulary already covers it; no new primitive, no kernel
accessor, no RFC.

- **Dividers `(x ∨ f)`, `f ∈ F` — add-RAT on `x`.** `x` is fresh, so at the
  moment they are added *no clause mentions `¬x`* and the RAT side-condition is
  vacuous. Identical to today; `f` is still a literal.
- **Quotients `(¬x ∨ r)`, `r ∈ R` — add-RAT on `¬x`.** The clauses containing
  `x` are exactly the dividers, and resolving `(¬x ∨ r)` with `(x ∨ f)` gives
  `({f} ∪ r)` — which is in the grid and therefore still in `F` at that point.
  **This argument does not care whether `r` is one literal or five**, which is
  precisely why the generalization is proof-free. Emission order (dividers
  before quotients, grid deleted last) is load-bearing exactly as today —
  `docs/calculus.md` §6b, deviation D-FACTOR-LRAT.
- **Grid deletion** — plain `delete` of `({f} ∪ r)` for all `m × n` pairs.
- **`ρ` witness for `x`** — unchanged in shape; E2 is a `ρ`-free *new-variable*
  rule, and the reconstruction entry is the same one the binary variant pushes.

**Consequence:** `docs/calculus.md`'s E2 entry needs no semantic change. Only
`implementation-notes.md` gains a line recording the scope widening (which is a
within-rule *scope* choice in the established sense, not a guard).

## 5. Shipping plan — default-identical, arm-gated

`Kernel::factor_size` (config, policy-owned, in the `bve_bound` pattern),
**default 2 = today's binary-only behaviour**, `TACTIX_FACTOR_SIZE` as the arm.
This is not timidity: every dosing/scope change measured in the last two days
that *looked* safe on single instances moved a tier the other way, so the
default must not move until the tier pair says so.

**Acceptance gate (pre-registered):**
- **Bit-identity at `factor_size = 2`** — conflicts AND `c factor` counts
  identical to `b8ece6d` on mod4block, mp1-blockpuzzle, x9, FmlaEquiv. A
  refactor of this size that is not bit-identical at the identity setting has a
  bug, full stop.
- **CONFIRMS at 5:** `or_randxor` solves (target 15–60 s), `fclqcolor`
  369 → 30–120 s.
- **KILLS:** `x9` or `mp1-blockpuzzle` regressing — today's Factor
  beneficiaries and the regression watch the diagnosis names.
- **Cost watch:** Factor's own `c timing` share and `factor_ticks`; the grid
  search is now over occurrence lists rather than binary watches, so the tick
  model in `charge_factor_ticks` must be re-derived, not reused blindly.

**Definition of done** (rule-dev skill): unit tests over hand-built grids
including `|r| > 1`; a micro-formula proof test through real drat-trim AND
lrat-check (the quotient RAT chain is the part that changes shape); quick fuzz
green on both proof formats **including `--random-policy`**; model
reconstruction tested explicitly (E2 is a witness rule — a wrong `ρ` entry is a
wrong SAT answer, not a proof failure); Tier B(28) + Tier C same-window pair.

## 6. Deliberate non-goals

- **`factorcandrounds`** (kissat's iterative candidate refinement, factor.c:1001)
  — a cost optimization on candidate selection. Not needed to test the
  hypothesis; revisit only if the grid search proves too slow.
- **Redundant clauses in the grid.** Kissat connects irredundant only; we keep
  that. A redundant clause can be deleted by S2 Reduce at any time, and a grid
  whose members can vanish underneath it is a correctness hazard, not a
  performance question.
- **Changing the gain function.** Clause-count gain `m·n − (m + n)` is kissat's
  and stays. Note it is now *not* the literal-count gain (grid clauses have size
  `1 + |r|`, dividers are binary), so if the measurement disappoints, the gain
  function is the first thing to re-examine — but it is not a hypothesis this
  pass tests.
- **The density short-circuit** (`factor.cpp:206`) stays as-is. It was vindicated
  in the 2026-07-30 R2 work (forcing Factor on where the gate suppresses it made
  mod4block 3.5× *worse*), so widening scope and loosening admission are
  separate experiments and must not be conflated.
