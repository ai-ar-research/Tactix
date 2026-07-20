# E2 Factor (BVA) — transliteration + design spec

Source: `kissat/src/factor.c` (MIT, Armin Biere). Transliterated with
attribution; no verbatim copy. All line citations are `factor.c` unless a
different file is named. Companion proof machinery: `kissat/src/proof.c`,
`clause.c`, `import.c`, `extend.c`.

Kissat's `factor` is **bounded variable addition (BVA)** restricted (by
default) to a *binary-clause–driven* variant that generalizes SBVA (Haberlandt
& Green, SAT-2023 winner's preprocessor). It is the inverse of variable
elimination: instead of removing a variable and multiplying clauses out, it
*introduces* a fresh variable to *factor out* a repeated sub-structure, netting
fewer/shorter clauses.

---

## 1. What BVA does + the pattern it detects

### 1.1 The algebraic pattern (the "quotient grid")

BVA looks for a **rectangular product** of clauses. Abstractly, there is a set
of **factor literals** `F = {f_0, f_1, …, f_{m-1}}` and a set of **remainders**
`R = {R_0, R_1, …, R_{n-1}}` (each `R_j` is either a single literal, giving a
binary clause, or a set of literals, giving a large clause) such that **every**
combination clause

```
    (f_i ∨ R_j)     for all i ∈ [0,m), j ∈ [0,n)
```

is present in the formula. That is an `m × n` grid of `m·n` clauses that all
"factor" as `F × R`. Introducing a fresh variable `x`:

```
    replace  {f_i ∨ R_j}   by   {x ∨ f_i}  ∪  {¬x ∨ R_j}
```

turns `m·n` clauses into `m + n` clauses. Net clause change =
`(m·n) − (m+n)`, which is positive as soon as `m,n ≥ 2` and one of them `≥ 3`.
Intuition: `x` names the disjunction "one of the `R_j` holds" from one side and
"one of the `f_i` holds" from the other; resolving `(x∨f_i)` against
`(¬x∨R_j)` on `x` reproduces every original `(f_i∨R_j)`, so the rewrite is
equivalence-preserving on the original variables.

### 1.2 How kissat *grows* a quotient (the matching)

Kissat does not search for the whole grid at once; it grows the factor set `F`
one literal at a time, maintaining a **chain of `quotient` records** (struct at
:27–34). Each `quotient` holds one factor literal `q->factor` and the stack
`q->clauses` of clauses (watches) that participate for that factor.

- `first_factor` (:211): seed with a factor literal `f_0` (popped from the
  candidate `schedule` heap). `quotient[0].clauses` = **all** binary+large
  watches on `f_0` (:219–229). Requires size > 1 (:232).
- `next_factor` (:325): given the last quotient's clauses (all containing the
  current factor chain), find the literal `next` that most often co-occurs as
  the *complementary partner* across those clauses. It walks each quotient
  clause, marks its non-factor literals `QUOTIENT` (:372–383), then for the
  min-occurring such literal `min_lit` scans `min_lit`'s watch list for a
  *sibling clause* `d` of equal size (:404) whose literals are exactly the
  `QUOTIENT`-marked set plus one new literal `next` (:407–418). Each such
  `next` gets `count[next]++` (:430–432). The **winner** is the highest count
  (:449–462); ties are broken by `tied_next_factor_score` (:318–323):
  - default (`factorstructural=0`): `watches_score` — raw watch-list size
    (:310–316);
  - structural (`factorstructural=1`): `distinct_paths` — a bounded
    (`factorhops`, default 3) count of distinct implication paths back to the
    seed factor, cached per hop (:256–297).
- `factorize_next` (:500) commits `next` as a new factor: it rebuilds
  `next_quotient.clauses` as the subset of the previous quotient's clauses that
  also pair with `next`, recording for each a back-pointer index in
  `->matches` (:532–533, :586–588) so unmatched clauses can be flushed later.

The chain `quotient[0], quotient[1], …` therefore represents the factor set
`F = {f_0, …, f_k}` incrementally, and `quotient[k].clauses` (after flushing)
is the remainder set `R` common to *all* factors.

### 1.3 The gain function and the bound

`best_quotient` (:607–643) scans the chain. For a chain prefix of length
`factors` with `quotients = |q->clauses|` remainders:

```
    before = quotients * factors          // clauses in the grid   (:614)
    after  = quotients + factors          // clauses after BVA     (:615)
    delta  = before - after               // reduction (only if before>after)
```

It picks the prefix with the **largest `delta`** (:627–630). The chosen `q` is
applied only if `delta > factoring->bound` (:939), where
`bound = solver->bounds.eliminate.additional_clauses` (:72) — the same growth
budget elimination uses (0 initially, grows over time; `kimits.c:126`,
`eliminate.c:365`). So the *smallest applied* factoring needs `delta ≥ 1`
(e.g. `m=2,n=3` or `m=3,n=2`).

Budget: the whole pass is bounded by a **ticks limit** (`factor_ticks`,
accumulated everywhere via `ADD(factor_ticks, …)`); see §5.

---

## 2. The transformation concretely (smallest non-trivial case)

Take `m = 2` factors `F = {a, b}`, `n = 3` binary remainders `R = {p, q, r}`.
Original irredundant clauses (the 2×3 grid):

```
  (a ∨ p)  (a ∨ q)  (a ∨ r)
  (b ∨ p)  (b ∨ q)  (b ∨ r)          — 6 clauses
```

`apply_factoring` (:820) with fresh `x` produces:

**ADD** — dividers, one per factor, `add_factored_divider` (:708), fresh literal
**first**:
```
  (x ∨ a)   (x ∨ b)                  — m = 2 clauses   (kissat_new_binary_clause(fresh, factor))
```
**ADD** — quotient clauses, one per remainder, `add_factored_quotient` (:718),
`¬x` **first** (:737):
```
  (¬x ∨ p)  (¬x ∨ q)  (¬x ∨ r)       — n = 3 clauses
```
**DELETE** — every original grid clause, `delete_unfactored` (:775):
```
  (a∨p) (a∨q) (a∨r) (b∨p) (b∨q) (b∨r) — 6 clauses removed
```

Result: 6 → 5 clauses, `delta = 6 − 5 = 1`. For a **large** remainder the only
change is `add_factored_quotient` builds `(¬x ∨ R_j)` by copying `R_j = c \
{factor}` after the leading `not_fresh` (:737–746), and dividers stay binary.

Order inside `apply_factoring` (:827–838) — **this order is the proof order**:
1. `flush_unmatched_clauses` for each quotient (:827–828) — trims each
   `q->clauses`/`matches` down to the common remainder set (:681–706).
2. `add_factored_divider` for every factor in the chain (:829–830) → the
   `(x ∨ f_i)`.
3. `add_factored_quotient` (:832) → the `(¬x ∨ R_j)`.
4. `delete_unfactored` for every factor (:833–834) → delete the grid.
5. `update_factored` (:835–836) reschedules touched literals; `resize_factoring`
   grows count arrays for the new variable (:838).

---

## 3. The PROOF (highest-value section)

### 3.0 Crucial finding: kissat has **no explicit RAT primitive**

`factor.c` never calls the proof backend directly. Every proof line is a
side-effect of the ordinary clause API:
- `kissat_new_binary_clause` → `ADD_BINARY_TO_PROOF(first, second)`
  (`clause.c:76`) → emits DRAT `a first second 0`.
- `kissat_new_irredundant_clause` → `ADD_CLAUSE_TO_PROOF(c)` (`clause.c:104`).
- `kissat_mark_clause_as_garbage` → `DELETE_CLAUSE_FROM_PROOF(c)`
  (`clause.c:156`); `kissat_delete_binary` → `DELETE_BINARY_FROM_PROOF`
  (`clause.c:184`).

`proof.c` only knows `a <lits> 0` (`print_added_proof_line`, :287) and
`d <lits> 0` (`print_delete_proof_line`, :303). **There is no RAT tag and no
pivot field.** Kissat emits *plain DRAT*; the RAT-ness of the definition
clauses is **inferred by the checker** (`drat-trim`/`cake_lpr`), which for any
added clause first tries RUP and, failing that, tries **RAT on the clause's
first literal**. This is why the literal *order* in §2 is load-bearing: the
fresh literal must be **first** in every clause that is only RAT-justified.

LRAT is not produced on this path inside kissat — kissat writes DRAT and an
external tool (drat-trim `-L`, or `cake_lpr` after backward checking) elaborates
to LRAT. **We are different: we want to emit LRAT directly, so we must
reconstruct the pivot + hint chain ourselves** (§3.3).

### 3.1 Why the dividers are RAT, not RUP

`(x ∨ a)` with `x` brand-new cannot be RUP: no unit propagation from the
current formula (which does not mention `x`) can derive it. It is **RAT on
pivot `x`**: the RAT condition requires, for every clause `D` currently
containing `¬x`, that the resolvent `(x∨a) ⊗_x D = ((x∨a)\{x}) ∪ (D\{¬x})`
is a tautology or RUP. At the moment the dividers are added, **no clause
contains `¬x` yet** (dividers are added before the quotient clauses,
:829→:832), so the RAT condition is *vacuously true*. Both `(x∨a)` and `(x∨b)`
are trivially RAT.

### 3.2 Why the quotient clauses are RAT (and need the originals still present)

`(¬x ∨ p)` is added next. It is **RAT on pivot `¬x`**: for every clause `C`
containing `x` — namely the dividers `(x∨a)`, `(x∨b)` — the resolvent must be
RUP:
```
  (¬x ∨ p) ⊗_x (x ∨ a) = (p ∨ a)   — RUP: it is still an original clause
  (¬x ∨ p) ⊗_x (x ∨ b) = (p ∨ b)   — RUP: still present
```
Both resolvents are original grid clauses that **have not been deleted yet**.
This is exactly why `delete_unfactored` (step 4) must come **after**
`add_factored_quotient` (step 3): the RAT justification of each `(¬x∨R_j)`
consumes the originals `(f_i∨R_j)` as RUP witnesses. Delete them first and the
proof is rejected.

### 3.3 Concrete DRAT line sequence (the `m=2, n=3` case)

Using external names `a,b,p,q,r,x`:

```
    a  x  a 0          ; divider  (x∨a)      RAT on x   (no ¬x clauses yet)
    a  x  b 0          ; divider  (x∨b)      RAT on x
    a -x  p 0          ; quotient (¬x∨p)     RAT on ¬x  via (x∨a),(x∨b) & (a∨p),(b∨p)
    a -x  q 0          ; quotient (¬x∨q)     RAT on ¬x
    a -x  r 0          ; quotient (¬x∨r)     RAT on ¬x
    d  a  p 0          ; delete originals — only now
    d  a  q 0
    d  a  r 0
    d  b  p 0
    d  b  q 0
    d  b  r 0
```

A checker accepts each `a` line by RUP-or-RAT-on-first-literal, then the `d`
lines shrink the DB. Note the fresh variable's DRAT id (its external index) must
be a *new* integer larger than any existing variable — `kissat_fresh_literal`
(`import.c:82`) allocates the next external index and marks it `extension`.

### 3.4 What an `add_rat(clause, pivot, hints)` primitive must carry

We do **not** have `add_rat` yet; Factor is the first RAT-emitting rule.
Requirements per backend:

- **DRAT**: emit `a <clause> 0` with **`pivot` placed first** in the written
  literal list. No pivot/hint metadata is written; correctness relies on
  first-literal RAT. So DRAT `add_rat` = `add` + a contract that the caller
  ordered the pivot first. (In fact for DRAT we could route through the
  existing `add` primitive, but a distinct entry point keeps the pivot-first
  contract explicit and lets the checker-agnostic layer branch.)
- **LRAT**: a RAT line is
  `<id> <clause> 0 <RUP-hints…> [ -<cid> <res-hints…> ]* 0`, where:
  - `clause` is written **pivot first**;
  - `<RUP-hints…>` is the (possibly empty) unit-propagation chain proving the
    clause is not already RUP-blocked — empty for our dividers/quotients;
  - for **each** clause `D` in the DB that contains `¬pivot`, a group
    `-<cid(D)> <hints proving the resolvent is RUP>` where `cid(D)` is `D`'s
    LRAT id (negative sign marks a RAT group).
  So the primitive signature must be
  `add_rat(lits[], pivot, rup_hints[], rat_groups[] = {(neg_cid, res_hints[])})`.
  - Dividers `(x∨f_i)`: pivot `x`, `rup_hints = ∅`, `rat_groups = ∅` (no `¬x`
    clauses exist yet).
  - Quotients `(¬x∨R_j)`: pivot `¬x`, `rup_hints = ∅`, and one RAT group per
    divider `(x∨f_i)` whose resolvent `(R_j∨f_i)` is discharged by the hint =
    **the LRAT id of the still-live original clause `(f_i∨R_j)`**.

**Design consequence for our kernel:** to build LRAT RAT groups we must know, at
apply time, the clause ids of (a) each divider we just added and (b) each
original grid clause `(f_i∨R_j)`. Kissat already threads clause references
through `q->clauses`; we must additionally retain each original's *proof id*.
Our `kernel_push_witness`/id-tracking must expose the id of every clause the
rule touches. This is the single largest new piece (see §6).

### 3.5 Empty-clause / UNSAT path

Nothing special: BVA only rewrites; the eventual refutation of the transformed
formula ends in `add_empty` as usual. Because dividers+quotients imply every
deleted original (§1.1), the transformed formula is equisatisfiable, so a DRAT
refutation of it composes with the RAT-introduction block into a valid
refutation of the original.

---

## 4. Reconstruction (ρ) — projecting out `x`

### 4.1 Kissat pushes **nothing** onto the reconstruction stack

Second crucial finding: `factor.c` never touches `solver->extend` (the witness
stack read by `kissat_extend`, `extend.c:44`). The `extend` stack in kissat is
consulted **only for eliminated variables** (`import->eliminated`, `extend.c:97`;
the loop assigns *unassigned eliminated* literals). The BVA fresh variable is
the opposite of eliminated: it is a live **extension variable**
(`import.extension = true`, `import.c:26–28`, `variables_extension++`) that is
`kissat_activate_literal`'d (`import.c:98`) and **assigned by search** like any
other variable.

Therefore, on SAT:
- The model over internal variables already includes a value for `x`.
- The external model reported to the user ranges over the **original** external
  indices only; `x`'s index is beyond the original max, so it is simply **not
  printed** — `x` is projected out for free.
- Correctness of the projected model: every deleted original `(f_i∨R_j)` equals
  `(x∨f_i) ⊗_x (¬x∨R_j)`, both present and satisfied by the model, so the
  resolvent is satisfied → dropping `x` yields a genuine model of the original
  formula. No witness is needed.

### 4.2 What our port must do

Two viable strategies:

- **(A) Match kissat — no ρ push.** Mark `x` as an *extension / non-original*
  variable and make `kernel_extend_model` (and the s/v-line output) **iterate
  only original variables**, ignoring extension vars. This is the clean choice
  and keeps ρ solely about eliminated variables (V3/E1). Requirement: our
  fresh-variable allocator tags `x` non-original, and the model printer already
  respects that tag.
- **(B) If our ρ must yield a total internal model** (e.g. a downstream rule
  reads `x`'s value from ρ rather than from the trail), push a **defining
  witness** for `x`: the value of `x` is fully determined by the original model
  via either side of the definition — `x` may be set to `false` whenever some
  `f_i` is false is *not* required; the safe reconstruction rule is
  `x := ¬(all R_j)`… simpler: push the quotient clauses `{¬x ∨ R_j}` as a
  witness block **blocked on `x`** (mirroring how weakened clauses carry a
  blocking literal, `extend.c:85,133,168`). Reconstruction then sets `x` true
  iff needed to satisfy those clauses given the `R_j` values, exactly as
  `extend_assign` does for eliminated vars. This reuses the eliminated-var
  machinery but *inverts its role*, so it is more fragile than (A).

**Recommendation:** implement (A). It is what kissat does, it keeps `x` out of
ρ entirely, and it composes cleanly with N2 (proofs) and later BVE of `x`
(BVE then pushes its own witness when it eliminates `x`).

---

## 5. Scheduling / bound / termination

- **Gate** `kissat_factoring` (:1068): only if option `factor` on and
  `log10(active_vars) > eliminations + factordelay` (default `factordelay=4`,
  :56) — i.e. factor is *delayed* until enough elimination rounds have happened,
  and scaled to formula size (:1074–1084).
- **Marked-literal skip** (:1094–1101): skip entirely if no new literals were
  added since last successful pass (`limits.factor.marked >= literals_factor`);
  reset only when a pass *completes* (:1153–1154). Prevents re-running on an
  unchanged formula.
- **Ticks budget** (:1106–1117): first pass gets `factoriniticks` (default 700)
  ×1e6 ticks; later passes use `SET_EFFORT_LIMIT(factor, factor_ticks)` scaled
  by `factoreffort` (default 50‰, :57). Every scan adds to `factor_ticks`; the
  main loop and inner `next_factor` bail when `factor_ticks > limit`
  (:442–443, :914–917), and honor `TERMINATED` (:918).
- **Mode**: runs in **dense mode** — `kissat_enter_dense_mode` connects all
  literals to full (binary+large) watch lists (:1127), `connect_clauses_to_factor`
  (:957) additionally connects only large irredundant clauses of size ≤
  `factorsize` (default 5, :60) that survive `factorcandrounds` (default 2)
  rounds of the "every literal occurs ≥2×" fixpoint filter (:1002–1038); then
  `kissat_resume_sparse_mode` (:1130). Binary-only if `factorsize<3` (:958–961).
- **Avoiding self-refactoring / looping**: (a) candidate literals are consumed
  from a max-heap `schedule` keyed by watch-count and each is cleared from the
  `f->factor` bitmask when processed (:920–924); a fresh var is *not*
  re-scheduled in the same pass beyond normal `update_candidate` touches. (b)
  `next > initial` literals (i.e. freshly created ones, index above the pass's
  starting `LITS`) are **excluded** from being chosen as new factors
  (:352,420–421) — this is the key anti-loop guard: a pass cannot factor over
  variables it just introduced. (c) `resize_factoring` (:645) grows the count
  arrays so new vars have slots but they only become candidates in a *later*
  pass. (d) The whole thing terminates when the schedule empties or the ticks/
  termination limit hits (:908,914,950).
- **Post-pass bookkeeping**: `adjust_scores_and_phases_of_fresh_variables`
  (:842) zeroes the VSIDS score of each fresh var and splices it to the **front**
  of the VMTF queue, so the new definition vars don't distort branching.
- **Interaction with other inprocessing**: factor runs in the eliminate/
  simplification phase (shares `bounds.eliminate.additional_clauses`), before/
  interleaved with BVE. Fresh vars introduced here can later be *eliminated* by
  BVE (which then owns their ρ witness). Because factor only touches irredundant
  clauses and adds irredundant clauses, it does not disturb learned-clause
  bookkeeping.

---

## 6. Executive summary (10 lines)

1. **What it is:** BVA — introduce fresh `x` to factor an `m×n` grid of clauses
   `{f_i ∨ R_j}` into `m` dividers `{x∨f_i}` + `n` quotients `{¬x∨R_j}`, net
   `m·n → m+n`; the binary-driven generalization of SBVA.
2. **Detection:** grow a factor chain (`quotient` records) by repeatedly picking
   the literal co-occurring in the most equal-size sibling clauses; apply the
   prefix with max `delta = m·n − (m+n)` when `delta > eliminate bound`.
3. **Transform (apply order = proof order):** add dividers, add quotients,
   **then** delete originals; reschedule + resize.
4. **Proof is the crux — and RAT is *implicit* in kissat:** it emits plain DRAT
   `a`/`d` lines; the checker infers RAT on the **first literal**, so pivot-first
   ordering is mandatory (dividers `x`-first, quotients `¬x`-first).
5. **Why RAT:** dividers are RAT-on-`x` vacuously (no `¬x` clauses yet);
   quotients are RAT-on-`¬x` with the *still-live* originals as the RUP
   witnesses — hence originals must be deleted **last**.
6. **`add_rat` primitive:** DRAT needs only pivot-first ordering; **LRAT** needs
   `add_rat(lits, pivot, rup_hints, rat_groups)` where each RAT group is
   `(neg clause-id of a `¬pivot` clause, hints)`, and for quotients the hint is
   the **id of the surviving original `(f_i∨R_j)`**.
7. **Reconstruction is trivial:** kissat pushes **nothing** onto ρ; `x` is a
   live extension variable assigned by search and projected out at output. Our
   port should tag `x` non-original and skip it in `kernel_extend_model` (option
   A) rather than push a witness (option B).
8. **Scheduling:** dense-mode, ticks-budgeted (`factoriniticks`/`factoreffort`),
   delayed by formula size, size-capped candidate clauses (`factorsize`);
   anti-loop guard = never factor over `next > initial` (freshly added) literals.
9. **Hardest piece for us:** the LRAT `add_rat` (pivot + RAT-group hint chains,
   requiring us to thread original-clause proof-ids through the rule) — plus the
   non-original-variable model projection. Detection/scoring is
   mechanical by comparison.
10. **Staged order:** (S1) fresh-variable allocation + non-original model
    projection + DRAT-only `add_rat` (pivot-first) → land the transform with
    DRAT, fuzz it; (S2) add the LRAT hint-chain to `add_rat` (thread original
    ids) and proof-check LRAT; (S3) binary-only detector + gain/bound + apply;
    (S4) large-clause remainders + `factorsize` filter; (S5) scoring
    (watches, then optional structural hops), scheduling gate, ticks budget,
    stress-policy fuzz for N1 schedules.
