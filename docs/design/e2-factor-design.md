# E2 Factor (bounded variable addition / SBVA) — design

Companion research: `docs/design/e2-factor-research.md` (kissat `factor.c`
transliteration). This doc is the **our-architecture design**: the abstract rule,
the proof + reconstruction plan against our kernel, the fits-§5.3 argument, and a
value-front-loaded staged implementation plan. Motivation: the 2026-07-20
profiling (`benchmarks/reports/…`, PROGRESS Log) showed Factor is the **dominant
missing lever** on structured UNSAT — it *is* the `mp1-blockpuzzle` (13.7×) and
`x9` (2.3×) gaps; kissat without `--factor` takes *more* conflicts than we do.

## 1. The abstract rule (catalog E2)

- **Precondition (abstract):** a fresh variable `x` and an `m×n` **clause grid**
  — factor literals `F={f_0..f_{m-1}}`, remainders `R={R_0..R_{n-1}}` (each `R_j`
  a literal or literal-set) — such that **every** `(f_i ∨ R_j)` is a present
  irredundant clause, with net gain `delta = m·n − (m+n) > bound`.
- **Effect:** add `m` **dividers** `(x∨f_i)` and `n` **quotients** `(¬x∨R_j)`;
  delete every grid clause `(f_i∨R_j)`. `x` is a **new** variable (not in `F₀`).
  `m·n → m+n` clauses. `R,ρ` otherwise unchanged; `H` gets `x` (score 0, front of
  the decision queue).
- **Proof δ:** **add-RAT** (dividers RAT-on-`x`, quotients RAT-on-`¬x`) + **delete**.
  No `ρ` push (see §3) — this *refines* the §5.2 catalog line, which currently
  says "+ push-witness"; Factor needs none.
- **Soundness (equisatisfiability, NOT modulo ρ):** resolving `(x∨f_i) ⊗_x
  (¬x∨R_j)` reproduces the deleted `(f_i∨R_j)`, so dividers+quotients **entail**
  every deleted clause → the transform preserves satisfiability, and a model of
  the transformed formula **restricted to the original variables** satisfies the
  original (both sides of the definition are satisfied, so each resolvent is).
  `x` is *added*, not eliminated — the inverse of BVE.

## 2. Proof plan against our backend

**DRAT needs no new primitive.** A RAT clause in a DRAT file is byte-identical to
a RUP clause; drat-trim tries RUP then **RAT on the first literal**. So:
- emit each divider `(x∨f_i)` and quotient `(¬x∨R_j)` via the **existing
  `add_rup`** (id 0 / DRAT), with the **pivot written FIRST** (`x` first in
  dividers, `¬x` first in quotients);
- **order is load-bearing** (= kissat's apply order): dividers, then quotients,
  then delete originals **last** — the quotient `(¬x∨R_j)`'s RAT step resolves
  against the dividers and discharges the resolvent `(f_i∨R_j)` as RUP using the
  *still-live original*. Delete originals first → proof rejected.
- Dividers are RAT-on-`x` **vacuously** (no `¬x` clause exists yet).

**LRAT needs a real `add_rat` — deferred (like congruence's XOR/ITE).** A native
LRAT RAT line is `id  <pivot-first lits> 0  <rup-hints> ( -cid <res-hints> )* 0`.
New primitive:
```
add_rat(id, lits[], size, pivot, rup_hints[], nrup, rat_groups[], ngroups)
   rat_group = { neg_cid /* an existing clause D ∋ ¬pivot */, res_hints[] }
```
- dividers: `rup_hints=∅, rat_groups=∅`.
- quotients `(¬x∨R_j)`: one group per divider `(x∨f_i)`, whose single hint is the
  **proof id of the surviving original `(f_i∨R_j)`**.
So the rule must **retain each grid clause's proof id** at apply time (we already
have `c->id()`); DRAT ignores it, LRAT consumes it. Under LRAT we gate Factor off
until this lands (interim deviation, mirrors D-CONG-LRAT), keeping the LRAT path
sound.

## 3. Reconstruction — projection, no ρ

`x` is a **live extension variable** (assigned by search), not eliminated, so it
needs **no ρ witness**. On SAT, project `x` out: the reported model ranges over
original variables only; `x`'s index is beyond the original max and is simply not
printed. Correctness: every deleted original is entailed by two satisfied clauses,
so the projected model satisfies `F₀`. Keeps `ρ` exclusively about V3/E1
*eliminated* vars. (If BVE later eliminates `x`, BVE pushes its own witness then.)

**Kernel requirement:** a **non-original / extension tag** on variables added
after parse, respected by `kernel_extend_model` and the `v`-line printer (iterate
original vars only).

## 4. Fits-our-architecture (§5.3) + the one real risk

New rule = one directory (`src/rules/factor/`) + one registry line. **Reuses:**
the dense occurrence index (`Kernel::dense`, from the inprocessing revamp — kissat
runs factor in dense mode); the arena/watch add+delete primitives; a
schedule/candidate heap like BVE's.

**Fresh-variable allocation mid-solve — RESOLVED, mostly already supported.**
`kernel_reserve_vars(k, k.nvars + N)` is **additive and growable**
(`if (nvars <= k.nvars) return;` then resizes *every* per-variable structure —
`values`, `assigned`, `saved` phases, `watches` (`resize_lits`), `links` (VMTF
queue), `scores`, `target`, `best`, `unit_ids`, `seen`, `marked_levels` — bumps
`unassigned`, and splices the new vars onto the decision queue). So the "add a
variable" primitive **already exists** and initializes all state consistently.
What remains for S0 is therefore small:
1. a **non-original / extension tag** (a `first_extension_var` index suffices —
   `var ≥ boundary` ⇒ extension) that `kernel_extend_model` and the `v`-line
   printer skip;
2. call `kernel_reserve_vars` at a **root fixpoint** (level 0, quiescent — where
   inprocessing already runs, so no trail/level invariant is at risk);
3. (heuristic, optional, matching kissat) splice fresh vars to the **front** of
   the VMTF queue with score 0 so the definition vars don't distort branching —
   `kernel_reserve_vars` currently appends them at the back; adjust or leave as a
   tuning item.

That is the extent of the "enabling risk"; everything else (detection, scoring,
the transform, the DRAT proof) is mechanical by comparison.

## 5. Staged implementation (value-front-loaded)

Ordered so a **measurable** DRAT Factor on `mp1`/`x9` lands before the LRAT and
tuning work:

- **S0 — kernel: fresh-variable allocation + non-original tag.** Grow the
  per-variable arrays at a root fixpoint; tag added vars non-original; make
  `kernel_extend_model` + `v`-line skip them. Unit-test: add a var, solve, model
  omits it. *(The gating piece — do first.)*
- **S1 — the transform + DRAT proof, fixed detector.** Given a detected grid,
  `apply`: add dividers, add quotients (pivot-first via `add_rup`), delete
  originals last; reschedule. Start with a trivial/hardcoded grid to exercise the
  proof ordering. **Fuzz: DRAT + SAT-model reconstruction green.**
- **S2 — binary-driven detection + gain/bound.** Grow the factor chain
  (quotient records) over binary watches, pick the max-co-occurring partner,
  `best_quotient` gain `delta`, apply when `delta > bound`. Rule-local dense
  occurrence use. **This is the algorithmic core.**
- **S3 — scheduling + activation + MEASURE.** Dense-mode entry/exit, ticks
  budget (`factoriniticks`/effort), size-delay gate, the anti-loop guard (never
  factor over freshly-added literals). Activate in DefaultPolicy; **measure Tier B
  (`mp1`,`x9`,`fmla`) + Tier C** — the payoff checkpoint (does Factor close the
  measured gaps?).
- **S4 — large-clause remainders + `factorsize` filter** (extend beyond binary
  remainders); re-measure.
- **S5 — native LRAT `add_rat` + proof-check; stress-policy fuzz** (N1 schedules);
  then ungate Factor under `--lrat`.

## 6. Open questions to resolve during S0–S2

1. ~~Can the kernel grow `nvars` mid-solve?~~ **RESOLVED:** `kernel_reserve_vars`
   is additive/growable and initializes all per-var state (§4). Remaining S0 check:
   confirm no *rule-local* scratch caches an `nvars`-sized buffer across the
   fixpoint where Factor runs (each apply re-reads `k.nvars`, so unlikely).
2. Does `add_rup`'s DRAT path write literals in the given order (so pivot-first is
   honoured)? (Spec says yes — verify.)
3. Bound source: reuse BVE's `eliminate.additional_clauses` growth budget, or a
   Factor-specific budget? (Start by reusing, per kissat.)
4. Should Factor sit in the same inprocessing phase as BVE (it shares the budget
   and dense mode)? Likely yes — schedule adjacent, Factor before BVE so BVE can
   later eliminate unproductive fresh vars.
