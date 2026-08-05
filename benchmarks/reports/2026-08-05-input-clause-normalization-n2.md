# The frontend's input-clause normalization broke LRAT ids — the whole surface, enumerated

**Date:** 2026-08-05 · **Frozen on writing** (doc discipline: reports are never edited).
**Follows:** `2026-08-05-sweep-se-polarity-fix.md` §7, which found this while proving the Sweep fix and
handed it over rather than expanding scope. **Branch:** `frontend-dedup-n2`.

**Verdict:** the handover was right — there is a real, live N2 breach, and it is **wider than the
duplicate-literal case that was found**: the same id-vs-form mismatch also let a refutation discovered
*during input* consume an id the checker had already given to a later input clause. Both are fixed. Six
further things the input path does were enumerated and are **not** breaches, and one more (a missing
final `0`) is a real divergence I deliberately did not fix; the evidence for each is below, because "we
did not fix it" is only a defensible answer with one.

---

## 1. The rule that makes all of this a rule

The checker reads the **original file** and numbers its clauses **1..N by position**. That is not a
convention we may reinterpret:

- `lrat-check` (`drat-trim/lrat-check.c:426-431`) parses the header only to size its tables and then
  does `while (1) { size = parseLine(cnf, CNF, index); addClause(index++, ...) }` — position, nothing
  else.
- `cake_lpr` goes further and **enforces** the declared count: on the same two clauses with a header
  saying 3 it refuses the file outright (`c DIMACS parse failed at line: 0. Reason: incorrect number of
  clauses`). Position numbering is the only reading consistent with that.

So a chain citing id `i` asks the checker to propagate **the file's clause at position `i`** — repeats
and all. Every transformation `ingest_clause` applies to an input clause is therefore a candidate
divergence, and the two ways to break the correspondence are (a) storing a *different clause* under an
input id, and (b) consuming an input id for something that is *not* that input clause.

A second fact does the actual damage. **Checkers hold a clause as a literal LIST, not a set.**
`lrat-check`'s propagation loop walks the entries and fails on the second unassigned one
(`FAILED: multiple literals unassigned in hint 1: -2 -2`); cake_lpr says `clause not empty or
singleton after reduction`. `-1 -1` is simply not a unit clause for them.

## 2. The enumeration, with a verdict for each

| # | What `ingest_clause` does | Can a chain cite an id whose clause the checker has in another form? | Verdict |
|---|---|---|---|
| 1 | **Repeated literal** removed (`sort` + `unique`) | **YES** — the stored clause is narrower than the file's under the same id | **BREACH — fixed** |
| 2 | **Tautology** dropped | No — the id is consumed but recorded *nowhere* (not `unit_ids`, not `binary_ids`, no clause header), so nothing can cite it | not a breach |
| 3 | **Literal order** (we sort) | No — checkers are order-insensitive within a clause, and our order is never communicated | not a breach |
| 4 | **Duplicate unit** dropped (second copy) | No — dropped, id recorded nowhere; the *first* copy's id still names a clause the checker holds verbatim | not a breach |
| 5 | **Duplicate binary** dropped (`binary_ids` hit) | No — same as 4. (A repeated **large** clause is not deduped at all: both copies are stored under their own ids.) | not a breach |
| 6 | **Empty input clause** → refute now, emit `add_empty(++next_clause_id, …)` | **YES, differently** — the id burned for the derived empty clause **collides with a later input clause's id** | **BREACH (latent) — fixed** |
| 7 | **Contradicting input units** → same emission | **YES, same collision** | **BREACH (latent) — fixed** |
| 8 | **Clauses after a refutation** dropped | No — ids still consumed in step, nothing recorded | not a breach |
| 9 | **A lying header** (our parser treats the count as advisory) | No — neither we nor either LRAT checker numbers by the header. See §6 for the separate cake_lpr consequence | not a breach |
| 10 | **A missing final `0`** (our parser ingests the trailing partial clause) | **YES** — but the divergence is that the checker does not have the clause AT ALL. Malformed input; see §2.1 | real, **NOT fixed** (out of scope, sized + flagged) |

Rows 2, 4, 5 rest on one property worth stating plainly: **an id is only citable if it was recorded**,
and `ingest_clause` records an id in exactly three places — `unit_ids[var]`, `binary_ids[key]`, and a
large clause's header word. Everything it drops writes to none of them.

Row 1 was measured, not assumed. Rows 6/7 were measured too, and the measurement is the reason they are
labelled *latent*: all three checkers **accept** them today, because the derived empty clause is always
the proof's last line, so the colliding id is defined and never used. `lrat-check`'s `setClause` would
silently overwrite the input clause it collides with; nothing later reads it. That is a correctness
argument that depends on nothing else ever being emitted after a parse-time refutation — a property no
one was maintaining deliberately. The fix removes the collision instead of documenting the coincidence.

### 2.1 Row 10 — a real divergence I am NOT fixing, and why

`parse_dimacs` is deliberately "tolerant of … missing final newline/zero" and ends with
`if (!scratch.empty()) ingest_clause(k, scratch)`. A file whose last clause has literals but **no
terminating `0`** therefore gives us N clauses and the checker N−1: `lrat-check`'s `parseLine` hits EOF
before the 0 and returns 0, which breaks its read loop and drops the clause. Measured on
`p cnf 2 4 / 1 2 0 / -1 2 0 / 1 -2 0 / -1 -2` (no trailing zero):

```
tactix    s UNSATISFIABLE, chain cites id 4
lrat-check c ERROR: using DELETED hint clause 4   → c NOT VERIFIED
cake_lpr   c DIMACS parse failed at line: 3. Reason: failed to parse line
```

This is a genuine unverifiable-certificate path, so it is recorded, not dismissed. It is **not** in the
normalization class this change fixes, and I did not touch it, for one reason that decides it: **the
input is malformed and neither LRAT checker will accept the FILE**, so no certificate we could emit
would verify — there is nothing to repair on the emission side. The available repair is on the *input
validation* side (reject the file, answer `s UNKNOWN`), which trades a correct SAT answer on malformed
input for a refusal and belongs to the `compete` surface, not to this fix. Note the distinction from a
missing final *newline*, which is valid DIMACS, is handled by both sides, and must stay tolerated
(probe `l-missing-final-zero`, verified by all three checkers).

Row 10 stands for its whole class: **wherever our parser is more lenient than the checker's, we can
answer on a file no checker will process.** The other leniencies I checked are safe because they fail
closed rather than open — a `%` terminator (SATLIB), a `+` sign, or any stray character makes
`parse_dimacs` return an error and we print `s UNKNOWN`, which emits no certificate at all. Only the
missing-`0` tolerance produces an *answer plus an uncheckable proof*, which is why it is the one listed.

## 3. Row 1, measured: the two shapes, and why only some of them ever failed

**Unit shape** (the handover's reproducer, `p cnf 1 2 / -1 -1 0 / 1 1 0`): we store the unit `¬x1` under
id 1; the chain `4 0 3 2 0` cites it; the checker's clause 1 has two entries. Rejected.

**General shape** — a repeated literal in a clause of width ≥ 3, stored as a binary or a large clause.
Reachable, and found by a targeted hunt over random CNFs with one injected repeated-literal clause:
188 UNSAT cases, 2 rejected; shrunk with `tools/fuzz/ddmin` to corpus entry **014**, where the file's
`-4 3 3 0` is stored as the binary `(3 ∨ ¬4)` and the chain uses it as a **propagating** antecedent
(`FAILED: multiple literals unassigned in hint 4: 3 3`).

The general shape matters for a reason worth recording, because it is what makes "just fix the unit
case" wrong. A duplicate is only *visible* to the checker when the duplicated literal is the one left
**unassigned** at check time:

- if the chain uses the clause as the **conflict**, every entry is falsified → both copies are
  falsified → fine;
- if the duplicated literal is the **lemma's own** literal, it is masked as falsified → fine;
- if the clause **propagates** the duplicated literal (the ordinary case for an antecedent whose
  propagated literal was *resolved away* and so is absent from the learned clause) → **rejected**.

Three of my hand-written probes (`b-dup-large`, `k-dup-binary-propagates`, `m-dup-resolved-away`)
verified pre-fix for exactly the first two reasons, which is the trap: the bug is not rare, its
*symptom* is conditional. Any fix that chases symptoms rather than the id-form correspondence would
leave most of the class live.

## 4. Why it is not a one-liner, and what the references do

The repair shape is not in question: emit the deduped clause as its own lemma with the original as its
single antecedent. That is a valid one-antecedent RUP step — negating the deduped clause falsifies every
literal of the file form (same literal *set*), so the original alone is conflicting.

The problem is the **id**. The lemma needs an id above *every* input id, and N is not known until the
input ends; ids cannot be taken mid-parse without stealing one the checker has already assigned by
position.

**CaDiCaL has faced exactly this and solves it by trusting the header.** `Internal::reserve_ids(nCls)`
sets `clause_id = reserved_ids = number` up front, `add_original_lit` then hands out
`original_id < reserved_ids ? ++original_id : ++clause_id` (`src/internal.cpp`), and
`add_new_original_clause` (`src/clause.cpp`) — on `original.size() > size`, i.e. duplicates or falsified
literals removed — takes `new_id = ++clause_id` from the derived range, emits
`proof->add_derived_clause(new_id, false, clause, lrat_chain)` with the original id pushed onto the
chain, and deletes the original. Kissat does not face it in the same way: its parser has no native LRAT
path to break.

**We take CaDiCaL's lemma shape and not its id scheme.** Our parser treats the declared clause count as
advisory (deliberately — `parse.cpp` "grows past a lying header"), so reserving from it would trade one
id hazard for another. Deferring to end-of-input needs no header at all, and it fixes rows 6/7 for free.

## 5. The fix

`frontend/parse.hpp` gains `finish_input(Kernel &)`, called at the end of `parse_dimacs` and, for the C
API, from every entry point that begins reasoning (`tactix_solve`, `tactix_apply`,
`tactix_apply_instance`, `tactix_rules_applicable`, `tactix_rule_instances`, `tactix_decide`,
`tactix_status` — the API has no end-of-file, so "the input is complete" is the first call that uses it).

- `ingest_clause` records a **restatement** (original id, deduped literals, and the place to re-point:
  `unit_ids[var]`, `binary_ids[key]`, or a large clause's `Ref`) whenever the clause it actually
  **stores** is narrower than the file's. Dropped clauses record nothing, which is exactly right — rows
  2/4/5 need no lemma.
- A refutation found during input sets `inconsistent` immediately (unchanged) but only *records* the
  chain; nothing is emitted.
- `finish_input` then runs with `next_clause_id == N` exactly — one id per input clause, no derived id
  consumed — emits each restatement as `add_rup(nid, lits, {orig_id})`, re-points the stored id, and
  finally emits the deferred empty clause. The contradicting-unit chain reads `unit_ids[var]` *after*
  re-pointing, so it picks up the restated id automatically.
- The refuting clause itself is never restated, and does not need to be: a clause consisting of copies
  of one literal `u`, cited when `¬u` is already assigned, is fully falsified — a *conflicting*
  antecedent, where duplicates are invisible (§3).
- The original input clause is **not** deleted. A checker holding a superset only ever finds RUP easier,
  and a `d` line would be one more thing to get wrong.
- Emission is format-uniform: `add_rup` writes the LRAT line with its chain and the DRAT line without,
  so both fuzz legs exercise the same code. As a side effect DRAT deletion matching improves — a later
  `d 1 2` used to match nothing when the file said `1 1 2`.

`c input <n> clauses restated` is printed by the frontend and counted by `fuzz.py`, so the path's
reachability is measurable rather than assumed (§7).

## 6. A checker bug found on the way, and one compatibility note

**`lrat-check` corrupts its heap on tiny inputs, independently of us.** It sizes `table` at
`tableAlloc = nCls * 2` and grows it by 1.5×, but `addClause` writes `size + 1` words where `size`
already includes the clause's terminating 0 — so on `p cnf 1 2` with two 2-literal clauses it overruns
**while parsing the CNF**, before any proof line. Evidence that this is not our proof:

- an unrelated hand-written CNF+proof pair with a 2-clause header aborts identically
  (`realloc(): invalid next size`);
- the *same two clauses* with a 3-clause header verify our fixed proof (`c VERIFIED`);
- pre-fix the same file reached a clean `NOT VERIFIED` only because the run made one fewer `realloc`
  after the corruption.

Consequence for the handover's gate: on the exact `p cnf 1 2` reproducer, **cake_lpr VERIFIES the fixed
proof and REJECTED the pre-fix one**, drat-trim verifies both, and lrat-check verifies the fixed proof
as soon as its table is not degenerate. The regression corpus entries are deliberately sized past this
(4 and 6 clauses) so all three checkers give a clean verdict on the same file.

**Compatibility note (not fixed, not a breach):** cake_lpr rejects any DIMACS file whose clause count
disagrees with the header, while we accept it. Being more tolerant than the checker cannot make a valid
proof invalid — but it does mean we would answer on a file cake_lpr will not check. Flagged for the
`compete` surface; no change made here.

## 7. Gates

### 7.1 The reproducer, pre-fix vs post-fix

`p cnf 1 2 / -1 -1 0 / 1 1 0`

| arm | LRAT emitted | lrat-check | cake_lpr | drat-trim |
|---|---|---|---|---|
| pre-fix | `3 0 1 2 0` | **REJECT** `multiple literals unassigned in hint 1: -2 -2` | **REJECT** `clause not empty or singleton after reduction` | VERIFIED |
| post-fix | `3 -1 0 1 0` / `4 0 3 2 0` | (own allocator bug at `nCls=2`, §6) | **VERIFIED UNSAT** | VERIFIED |
| post-fix, 3-clause header | same | **VERIFIED** | (refuses the header) | VERIFIED |

### 7.2 Regression corpus — fails on the pre-fix build

Two entries, both `solver_args: ["--lrat"]` (honoured by `cmd_corpus` since today):

- **013-input-dup-literal-unit-lrat-id** — 4 clauses, the unit shape.
- **014-input-dup-literal-general-lrat-id** — 6 clauses, the general shape, ddmin-shrunk from a
  30-clause random case.

```
corpus, PRE-fix build:   CORPUS FAIL 013 [('proof', 'lrat-check rejected the native LRAT (exit 1)')]
                         CORPUS FAIL 014 [('proof', 'lrat-check rejected the native LRAT (exit 1)')]
                         corpus: 14 entries, 2 failures
corpus, POST-fix build:  corpus: 14 entries, 0 failures
```

### 7.3 Fuzz legs, with source-counted reachability

Paired seed 20260805, `--jobs 4`, local (no SLURM — the queue belongs to another agent). The
`reached:` figure is the solver's own `c input` counter aggregated by `fuzz.py`, so it counts the path
at its source, not the CNF text; it reads 0 on every pre-fix arm because that build has no such counter,
which is itself a check that the number is real and not an artifact of the harness.

| leg | build | cases | failures | verdicts | **reached** (input-restated) |
|---|---|---|---|---|---|
| native LRAT | **pre-fix** | 2000 | **25** (all `proof`) | S 1137 / U 826 / TO 37 | n/a — the counter is new |
| native LRAT | post-fix | 2000 | **0** | S 1138 / U 828 / TO 34 | **547 cases, 440 UNSAT** |
| `--random-policy` + native LRAT | **pre-fix** | 2000 | **26** (all `proof`) | S 1005 / U 668 / TO 327 | n/a |
| `--random-policy` + native LRAT | post-fix | 2000 | **0** | S 1006 / U 671 / TO 323 | **555 cases, 448 UNSAT** |
| native DRAT | post-fix | 2000 | **0** | S 1137 / U 823 / TO 40 | 547 cases, 440 UNSAT |
| `--random-policy` + native DRAT | post-fix | 2000 | **0** | S 1005 / U 668 / TO 327 | 555 cases, 448 UNSAT |
| `--random-policy` + ASCII DRAT | post-fix | 1000 | **0** | S 510 / U 327 / TO 163 | 270 cases, 214 UNSAT |

Three things this table is meant to settle:

1. **The legs are not vacuous.** On the decisive native-LRAT leg the input-normalization path is reached
   by **547 of 2000 cases and 440 of the 828 UNSAT answers** — 53 % of the UNSAT answers, counted from
   the solver's own counter, not inferred. This is the check the Sweep post-mortem said to run first.
2. **The symptom is much rarer than the path**, exactly as §3 predicts: 440 UNSAT answers reached it and
   only **25 rejected pre-fix (5.7 %)**. A fix aimed at the failing cases rather than the id-form
   correspondence would have looked green and left most of the class live.
3. **Every pre-fix failure is this bug and nothing else.** All 25 bundles on the DefaultPolicy leg and
   all 26 on the random-policy leg carry a repeated input literal, and every one is a `proof` failure —
   0 other signatures, 0 model/differential/crash failures, on either arm.

Rates, for the record: **25/826 = 3.0 %** of UNSAT answers under DefaultPolicy and **26/668 = 3.9 %**
under `--random-policy`, which brackets the handover's 1.5–4 % estimate. Note the random-policy legs
carry ~325 TIMEOUTs at `--max-vars 100`; a timeout reaches no semantic oracle, so their denominators are
the UNSAT counts, not the case counts.

DRAT is unaffected by construction (it cites no ids), so those three legs are a **no-regression** check
on a change that does alter the DRAT stream by one lemma per degenerate clause — not evidence for the
fix. Said explicitly because a green DRAT leg here would otherwise look like confirmation.

### 7.4 ctest

**43/43**, with `micro_proofs` and `sweep_proofs` actually *running* rather than skipped (they exit 77
without `tools/checkers/bin`, which a fresh worktree lacks — worth knowing, since a skipped proof test
looks like a passing one in the summary line).

`micro_proofs` gained the four cases this change is about — the three stored widths a repeated literal
can collapse to (unit / binary / large) and a mid-input refutation with input clauses still to come —
so the breach now has a **sub-second ctest guard** as well as a corpus entry. It **fails on the pre-fix
binary** (`FAILED: multiple literals unassigned in hint 4: 3 3`).

While there: `micro_proofs` judged lrat-check with `grep -qi 'verif'`, the very substring that let
`fuzz.py` accept `c NOT VERIFIED`. Here it was masked — the preceding `|| { FAIL }` on lrat-check's exit
code catches every rejection, so no test was ever wrong — but it is the identical landmine one edit
away from mattering, and it is now `grep -q 'c VERIFIED'`.

## 8. What did NOT move

No proof rule's guard, precondition, effect or proof delta changed — this is the input path, which is
not a rule. `docs/calculus.md` is therefore untouched by design. `docs/implementation-notes.md` gains
the design-decision entry (the id-space contract and why deferral rather than CaDiCaL's reservation),
since that is where standalone engineering decisions live.
