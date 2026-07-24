# S2b — within-pass composite grids (lifting the extension-var factor boundary)

> **Design pass (2026-07-24). Frozen on landing** — after S2b lands, do not
> edit (living truth: `docs/implementation-notes.md`, PROGRESS). Companions:
> `docs/design/s2a-factor-matcher-design.md` (the selection/mechanism split),
> `docs/design/e2-factor-research.md` (kissat `factor.c` transliteration),
> `benchmarks/reports/2026-07-24-battleship-divergence.md` (why Factor matters
> on the tail).

## 1. The evidence (2026-07-24, all cluster, all same-window)

1. **S2a Increment 3 disconfirmed tie-break steering**: mp1's grid cap is
   **27×27 (Δ675) under three different tie orders**; kissat reaches
   22×64-class grids (Δ1322) on the same instance — higher-gain grids exist
   that our chain can never reach.
2. **kissat's mechanism is composite** (`factor.c` read): `apply_factoring`
   calls `resize_factoring(…, not_fresh)` — the pass's own count/schedule
   arrays grow to include the fresh literal — and the new divider/quotient
   binaries land in live watch lists that later chains in the *same pass*
   walk (`next_factor` counting). Fresh vars also get `mark_added_literal`
   at activation, re-arming their `flags.factor` bits for the *next* pass
   (`schedule_factorization` iterates ALL active vars, no
   original-vs-extension distinction).
3. **The composite structure is real on our own output**: kissat factors
   **1,303 further variables (36 %, 35 factorization rounds)** on our
   already-factored mp1 dump (probe 1560298).
4. **…and post-hoc composition cannot rescue a bad first pass**: after all
   that further factoring, kissat still needs **1.37 M** downstream conflicts
   on our dump vs **305,691** native. Grid application is destructive (the
   originals are deleted), so the first pass's square grids consume the graph
   irreversibly. Round 1 on the raw formula contains no extension vars, so
   kissat's wide grids MUST be **within-pass composites** — chains built on
   fresh vars from earlier grids of the same pass, before the graph is spent.

**Conclusion:** the fix is within-pass composition — fresh literals become
partners/seeds immediately — not an extra post-pass generation sweep.

## 2. Why the `orig_lits` boundary exists, and what is actually load-bearing

`factor_apply` restricts factors/remainders to `p < orig_lits`
(= 2·`num_output_vars`), with two recorded justifications:

- **The anti-loop invariant** ("factor over its own extension vars →
  unbounded self-factoring"). **Re-examined, this is NOT load-bearing:**
  every accepted factoring has `delta = m·n − (m+n) ≥ FACTOR_MIN_GAIN = 1`,
  i.e. it strictly decreases the live binary count; the count is finite, so
  ANY schedule of factorings terminates. The degenerate "factor a grid's own
  dividers" case is unreachable: the dividers (x∨fᵢ) pairwise share only
  `x`, so a grid among them has m=1 and delta ≤ −1 — never accepted.
  (kissat bounds the same way: gain > bound, plus a ticks budget.)
- **The engineering reason (the real one):** the pass's scratch arrays
  (`inR`, `seen`, `cofactor`, `tried`, sized `orig_lits` once at entry) and
  the seed heap are built before any growth. Mid-pass fresh literals would
  index out of bounds. This is exactly what kissat's `resize_factoring`
  solves.

One boundary consequence MUST survive: **BVE never schedules extension
variables** (implementation-notes §5) — otherwise BVE resolves a fresh var
away and Factor re-creates it, thrashing every round. That exclusion is
BVE-side and untouched by S2b.

## 3. Mechanism (increment 1)

Inside `factor_apply`, replace the fixed `orig_lits` limit with a growing
`live_lits` frontier:

- `live_lits` = `2·k.nvars` at entry — NOT `orig_lits`: pre-existing
  extension vars from EARLIER passes (previous inprocessing rounds'
  factorings, V3/E1 aux) are exactly the composite structure kissat exploits
  across rounds (probe: +1,303 on our dump) — growing as this pass adds
  vars. The anti-loop term is delta-monotonicity (§2), not a var-class
  exclusion.
- Scratch arrays (`inR`, `seen`, `cofactor`, `tried`) resize with
  `live_lits` on each applied factoring (amortized doubling; kissat's
  `resize_factoring` transliterated to our vectors).
- `binary_partners`' `limit` parameter becomes `live_lits` — fresh literals
  are countable as partners / next-factors from the moment their binaries
  exist (within-pass composition, matching kissat).
- The post-apply re-push (`update_factored` analog) additionally pushes the
  fresh literal's BOTH polarities onto the seed heap (kissat seeds fresh
  vars next pass via flags; seeding them this pass is the same composition
  one step sooner — and `tried` still caps every literal at one seed
  attempt per pass, so termination is unchanged).
- `kernel_num_output_vars` / DIMACS output / witness handling: untouched —
  extension vars were already first-class there (E1/V3/S2a precedent);
  `apply_factoring` is literally unchanged (grid-agnostic, RAT emission
  identical — the S2a proof-delta invariance argument applies verbatim,
  §5 of the S2a design).
- The **density gate** keeps using `num_output_vars` (original count) — the
  gate is an instance-class selector, not a pass-progress meter.

**Knob:** `TACTIX_FACTOR_COMPOSITE=1` enables; unset/0 restores the
`orig_lits` limit exactly. (Default OFF per the §7 verdict.) N1: this is
rule-internal mechanism scope, like `FACTOR_DIM_LIMIT` — reachability of
factorings over the API is only ENLARGED when on; the guard stays pure.

## 4. Termination & proof arguments

- **Termination:** each accepted factoring strictly reduces live binary
  count (§2); `FACTOR_MAX_PER_APPLY` (2²⁰) is the unchanged hard backstop;
  each literal (original or fresh) seeds at most once per pass (`tried`).
- **Proof delta:** unchanged — `apply_factoring` emits the same
  divider/quotient RAT pattern for any `(F,R)`; hints reference live binary
  ids, which extension-var binaries have like any other. Every UNSAT still
  proof-checks; SAT models reconstruct through ρ in reverse application
  order (composite chains unwind naturally: outer aux first, inner next).
- **Reconstruction depth:** composite witnesses nest (y defined over x); ρ
  is an ordered stack replayed in reverse — already the invariant V3/E1
  established. No change.

## 5. Gates (increment 1 = mechanism; each subsequent step its own commit)

1. **mp1 proxy**: grid trace must break the 27×27 cap; downstream-conflict
   proxy 1.37 M → target direction 305 k (design §4 of S2a: ship only what
   moves the proxy without Tier B loss).
2. **Fuzz**: quick 10 k + random-policy+LRAT 4 k + `factor_stress_policy`
   legs (the adversarial arms now explore composite chains — the new N1
   surface this opens; TACTIX_FACTOR_FORCE keeps it reachable on small
   CNFs). Both formats.
3. **Tier B array** (14/14 must hold; battleship + mp1 watched) + **Tier A
   array** (199/200, no new misses).
4. `tests/micro_proofs.sh` + ctest both presets.

## 6. Dead ends already paid for (do not re-litigate)

- Tie-break steering (S2a Inc 3): 27×27 under three orders; `paths` shelved.
- Post-hoc composition (§1.4): factoring MORE after a bad first pass does
  not recover quality — composition must be available while the graph is
  still rich.
- Accept tightening / large-clause scope / dedup (S2a design §7).

## 7. Increment-1 VERDICT (2026-07-24, probe 1560299 — lands with this doc)

**Mechanism works; quality hypothesis DISCONFIRMED; ships default-OFF.**
Composite ON on mp1: 1,834 grids (940 base + 894 composites — the mechanism
fires), disabled path byte-faithful (940 / 27×27 / 1,367,464 reproduced) —
but the grid cap does NOT move (27×27 under composition = the 4th mechanism
that fails to break it) and the proxy REGRESSES 1.37 M → 2.18 M: in a single
preprocessing blast, composition over-applies (the L4 U-shape, Factor
edition). Two consequences:

1. **The 22×64 width target is now judged an instrumentation artifact** —
   §1's own caveat matured: the hacked kissat counter measured the un-truncated
   chain (`m = q->id+1` post-flush), not the applied best-delta prefix; our
   chains also grow long before truncation. No clean-surface evidence of a
   width gap survives.
2. **mp1 is routed to S4.** kissat native: 855 factored vars over 16 small
   tick-dosed rounds interleaved with elimination and search — vs our
   940-grid blast. With tie-breaks, composition, accept, scope, and dedup all
   measured dead, the ARRANGEMENT is the only live mp1 lever. The composite
   mechanism stays behind `TACTIX_FACTOR_COMPOSITE=1` because S4's small
   doses are exactly where kissat's own cross-round composition operates.
