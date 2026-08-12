# Vivify cross-candidate trail reuse (SCHED-G3), the leftover bit (SCHED-F5), tier3

Design pass, 2026-08-05. Frozen on landing. Scope: `src/rules/vivify/`,
one `Clause::flags` bit, the Vivify share in `DefaultPolicy`.

Track SCHED items addressed: **G3** (trail reuse), **F5** (per-clause leftover
prioritization — routed here by Stage F because the touched-epoch substrate
stamps *variables* and cannot express a per-clause priority), **H1's Vivify
half** (termination polling), and **C10 hand-off H3** (tier3 is never
vivified).

---

## 1. What the two references actually do

Read at the use site, not from the option names.

**kissat `vivify.c`.** A round does *not* return to the root between
candidates. `vivify_clause` walks the candidate's `sorted` literal list with a
position counter `level`; at position `l`:

```c
if (level++ < solver->level) {
  frame *frame = &FRAME (level);
  if (frame->decision == NOT (lit)) { INC (vivify_reused); continue; }   /* reuse */
  kissat_backtrack_without_updating_phases (solver, level - 1);          /* mismatch */
}
const value value = VALUE (lit);
assert (!value || LEVEL (lit) <= level);
```

(`vivify.c:1033-1046`.) Three properties matter and all three are load-bearing
for us:

1. The reuse test is on the **frame decision literal**, i.e. on the assumption
   itself, not on the value. A literal that happens to be false for another
   reason is not a reuse.
2. The value is **never read on a reuse branch**. The `assert` on the fall-
   through path pins why: values may only be interpreted when
   `solver->level == level - 1`, i.e. when exactly the assumptions this
   candidate has itself made are in force. On the reuse branch the trail may
   be *deeper* than the current position (leftovers from a longer previous
   candidate), so a value read there could be justified by an assumption that
   is not one of this candidate's literals.
3. Before the walk, kissat checks whether the **candidate is itself the reason**
   of a trail literal and, if so, backtracks below it
   (`vivify.c:995-1005`).

**cadical `vivify.cpp:925-990`** does the identical three things
(`stats.vivifyreused`, the `forced` reason check at `:940-965`), and states the
payoff in its own comment: *"In preliminary experiments this saved between
30%-50% decisions (and thus propagations)."*

**Both references also say what makes reuse possible at all**, and it is not
the reuse loop — it is the *order*. cadical `vivify.cpp:35-42`:

> There is another very similar approach called 'distillation' … which
> reorganizes the CNF in a trie data structure to reuse decisions and
> propagations along the trie. We used that as an inspiration but instead of
> building a trie we simply sort clauses and literals in such a way that we get
> the same effect. … Reusing can be improved by picking a global literal order
> and sorting the literals in all clauses with respect to that order. We favor
> literals with more occurrences first. Then we sort clauses lexicographically
> with respect to that literal order.

kissat implements the same thing (`vivify_sort_clause_by_counts` +
`worse_candidate`, `vivify.c:307-346`).

**This is the finding that shapes the item.** Our probe order is *clause
literal order* (`vivify.hpp` deviation 3) and our candidate order is
`(irredundant, size, ref)`. Under those two orders, adjacent candidates share a
leading literal only by accident, so a reuse loop bolted onto today's
scheduling would measure a reuse rate near zero and the mechanism would be
inert — an [[ab-arm-identical-artefacts]] result waiting to happen. **The
literal order and the candidate order are not an optional refinement of G3;
they are its prerequisite, and they land with it.**

One deliberate divergence from kissat here: kissat *permutes the clause's
literals in the arena* (`sort_scheduled_candidate_literals`) and therefore has
to bracket the whole round with `kissat_flush_large_watches` /
`kissat_watch_large_clauses`. We keep the global literal order in a
round-scoped side vector and sort only our local probe list, so the arena and
the watch arena are untouched. Same effect, no watch rebuild.

---

## 2. What changes

### 2.1 A round-scoped global literal order

One arena pass per round accumulates `counts[lit]` over every non-garbage
clause (kissat counts over the whole database, not just the scope — `vivify.c:
276`, the `count_clause` call precedes the tier filter). Global order `≺`:

    a ≺ b   iff   counts[a] > counts[b],  ties broken by  a < b

`counts` is a persistent `thread_local` buffer restored in O(touched) at round
exit, not an O(nvars) fill per round ([[per-call-linear-scratch-hazard]]).

### 2.2 Probe order and candidate order

* **Probe list** of a candidate = its non-root-fixed literals sorted by `≺`.
* **Candidate order** = `(priority bit desc, probe list lexicographic under ≺,
  shorter first, ref)`.

Candidates that agree on their leading literals become adjacent, which is
exactly the trie-without-a-trie that both references describe.

### 2.3 The `Clause::VIVIFY` flag bit (SCHED-F5)

Current `flags` layout: `garbage(0) | redundant(1) | reason(2) | used(3..7) |
glue(8..31)`, so `MAX_GLUE = 2^24 - 1`.

**The bit is the right answer to F5, and stealing it from `glue` is free.**
`set_glue` already saturates, and glue is bounded by clause size, which is
bounded by the variable count; a formula with more than 2^23 = 8.4 M distinct
variables in one clause is far outside anything the arena addresses. New
layout: `… | used(3..7) | vivify(8) | glue(9..31)`, `MAX_GLUE = 2^23 - 1`.

The alternative F5 listed — a side table keyed on something GC-stable — was
correctly rejected there: the arena `Ref` is not GC-stable, and the only
GC-stable name a clause has is its proof id, which would mean a hash lookup on
a hot path to store one bit. The header bit is 24 bits cheaper and needs no
lifecycle of its own: `kernel_add_large_clause` zeroes `flags`, so a new clause
starts unprioritized, and GC relocation carries the header verbatim.

Semantics, transliterated from `vivify.c:265-302, 1311`:

* scheduling partitions the scope into prioritized (`vivify()` set) and not;
* **if the prioritized set is empty, every scheduled candidate is prioritized**
  — the bit is a PRIORITY mechanism, never a suppressor, which is the property
  F5 flagged as the one the variable-stamp substrate destroys;
* the bit is cleared on a candidate the moment it is *tried*, so what survives
  a budget cut is exactly the untried tail, and that tail heads the next round.

This is also the first resumption Vivify has ever had: it is a BATCH rule, so
Stage E's `SlotEntry::cursor` does not apply to it, and every round to date
restarted from the same head — the same defect E1 found in Probe.

### 2.4 Trail reuse across candidates

`vivify_apply` owns one probing scope per **run of non-mutating candidates**
and hands the trail from candidate to candidate. Per candidate:

```
1. root-value candidacy re-check              (§3.1)
2. if the candidate is a trail reason, backtrack below it   (§3.3)
3. if k.level > |probes|, backtrack to |probes|             (§3.4)
4. reuse walk: position i (1-based), literal p_i
     if i <= k.level:
        if frame_decision(i) == ¬p_i  -> reuse, continue   (never read a value)
        else                          -> backtrack(i-1)
     assert k.level == i - 1
     ... today's value tests / assume / propagate, unchanged ...
5. on an actionable outcome: reset_to_root() then rewrite   (§3.2)
```

`reset_to_root()` is today's `unwind()`: clear the conflict, `kernel_backtrack
(k, 0, false)`, `kernel_probing_end(k)`. On a NoOp candidate it is **not**
called, and that is the whole change.

### 2.5 Tier3 (C10 hand-off H3) — LANDS

`k.vivify_scope` grows from `{0 any, 1 redundant, 2 irredundant}` to name the
band:

| scope | band | share of the Vivify dose |
|---|---|---|
| 0 | any (the N1 / unscoped path) | — |
| 1 | redundant, `glue <= k.tier1` (tier1) | 3/10 |
| 2 | irredundant | 3/10 |
| 3 | redundant, `k.tier1 < glue <= k.tier2` (tier2) | 3/10 |
| 4 | redundant, `glue > k.tier2` (tier3) | 1/10 |

kissat's shares are `vivifyirr 3 / vivifytier1 3 / vivifytier2 3 / vivifytier3
1` (`vivify.c:1390-1400`), which is the "3+3+1 = 7" `default_policy.cpp` was
already *claiming* while spending the whole 7 on a single `glue <= tier2` pass.
Legacy scope 1 (all redundant within tier2) is kept as the meaning of "the
redundant band" for external policies; the policy now issues 1/3/4 instead.

Tier3 is pure candidate-set scheduling: `is_candidate` is a *filter*, not the
guard, so the calculus rule is untouched and there is no new proof delta.

### 2.6 Termination polling (H1, Vivify half)

kissat polls **every** candidate with no cadence (`vivify.c:1291`,
`TERMINATED (vivify_terminated_1)`). We do the same — `kernel_terminating(k)`
is one predicate call per candidate, unmeasurable against a candidate that
propagates. **There is deliberately no `!(++tick & 255)` window here**: the
trap that made task #10's abort paths vacuous through 4 × 10,000 fuzz cases was
exactly a poll window an instance never reached, and a poll with no window
cannot have one. `k.stats.vivify_polls` counts at the source (inside the
branch, one per evaluation) and `k.stats.vivify_terminated` counts fires, so
"the site is reached" and "the site fires" are two separate numbers and neither
can be inferred from the other.

The abort path is the ordinary round exit: `reset_to_root()` then return
whatever the round produced. Dropping the remaining candidates is sound —
Vivify's contribution is a set of independent add/delete pairs, each already
emitted with its own certificate.

---

## 3. Soundness: what makes the reused prefix valid for the next candidate

> **Invariant VP (prefix validity).** At the moment candidate *c* begins its
> reuse walk, the retained state satisfies:
>
> **(VP1)** the arena, the watch arena, the proof stream and the root
> assignment are byte-for-byte what they were when the retained trail was
> built;
>
> **(VP2)** every retained trail literal is a logical consequence of
> `F ∪ R ∪ {d₁ … d_i}`, where `d₁ … d_i` are the decision literals of levels
> `1 … i`, and its recorded reason is a clause that is live in the arena and
> undeleted in the proof;
>
> **(VP3)** after processing position *i* of *c*'s probe list, `k.level ≥ i`
> and `d_j = ¬p_j` for every `j ≤ i`; and a value is read, or an assumption
> made, only where `k.level = i − 1` exactly.

**VP1 is maintained by the reset rule, and it is the whole safety argument.**
Every operation that can invalidate a retained trail — a clause rewrite, a
clause deletion, a root unit, a garbage collection — is preceded by
`reset_to_root()`, which discards the entire trail and closes the probing
scope. So a trail is never carried *across* a database change; it is carried
only across candidates that changed nothing. There is no generation counter to
get wrong, because there is no state that outlives the thing it describes.

This also discharges, for free, three obligations that would otherwise each
need their own argument:

* `kernel_probing_end`'s contract — no clause may be added, deleted, or arena-
  relocated inside a scope — is satisfied because the scope closes before any
  mutation. Propagation's legal watch relocation and `searched` pose persist,
  matching Kissat's probing engine.
* `reset_to_root` restores the trail / `propagate_head` / level values before a
  formula mutation; no watch rollback is performed.
* kissat's `reestablish_watch_invariant_for_candidate` has no counterpart here
  and needs none: it exists because kissat shrinks a clause in place while
  still at a non-zero level, and we never mutate above the root.

**VP2** holds because propagation only ever derives implied literals, and the
per-candidate `ignore` argument only *removes* propagations — it can never add
a literal that is not implied. Liveness of the reasons is VP1.

**VP3** is an induction over the walk. At position *i* we either (a) reuse,
which requires `d_i = ¬p_i` by the frame test, or (b) backtrack to `i − 1` and
assume `¬p_i`. Both re-establish the hypothesis. The consequence that matters:
**a value is read only in case (b)**, where all levels above `i − 1` have just
been discarded, so the only assumptions in force are `¬p₁ … ¬p_{i-1}` — every
one of them a literal of *this* candidate. That is precisely kissat's
`assert (!value || LEVEL (lit) <= level)`, and it is why the reuse branch must
`continue` without inspecting anything.

### 3.1 Root values vs. current values

Today `is_candidate` and the probe-list construction read `k.value(l)` and are
correct only because the level is always 0 on entry. With a retained trail they
would read *speculative* assignments — and the root-satisfied **deletion** path
is reached through the same test, so a clause satisfied only under an
assumption could be deleted outright. That is the one way this change could
have produced a wrong answer rather than a slow one.

Both are therefore moved onto an explicit root value,
`root_value(k,l) = (k.value(l) != 0 && k.var_level(var_of(l)) == 0) ? k.value(l) : 0`,
which is kissat's `kissat_fixed` and is what `simplify_vivification_candidate`
and the `sorted` construction use (`vivify.c:60,940`). The deletion path in
addition calls `reset_to_root()` before touching the database, per §2.4 step 5.

### 3.2 Why a rewrite may still cite the candidate — and why it does not here

The retained levels were propagated with `ignore` set to the *previous*
candidate, so a literal on the retained trail may have the *current* candidate
as its reason. Two consequences, one benign and one not:

* Benign, on the proof: `rewrite()` emits the add before
  `kernel_replace_clause` emits the delete, so a chain citing the candidate's
  own id is a legal LRAT step over a still-live clause, and the derived clause
  is implied by `F ∪ R` either way.
* **Not benign, on the checker:** in the implied-negation case the candidate's
  id is passed as the closing hint (`last = cid`). If the reason walk also
  reached it, the id would appear *twice* in one chain, and a hint that is
  already satisfied when the checker replays it is a rejection, not a warning.

Both references guard the same way and so do we (§3.3): the candidate is
removed from the trail as a reason *before* the walk, and `ignore = cref` stops
it becoming one again. A clause is the reason of at most one literal at a time,
so one check settles it.

### 3.3 The candidate-is-a-reason check

O(|c|), exactly cadical's `forced` search (`vivify.cpp:940-965`) and kissat's
`vivify.c:995-1005`:

```
for (Lit l : *c)
  if (k.value(l) > 0 && k.var_level(var_of(l)) > 0) {
    const Assigned &a = k.assigned[var_of(l)];
    if (!a.binary && a.reason == cref) { backtrack(k, k.var_level(...) - 1, false); break; }
  }
```

cadical notes why the check cannot be skipped in its own words: *"Otherwise
this clause might be incorrectly considered to be redundant, or … redundancy by
other clauses using this forced literal becomes impossible."*

### 3.4 Leftover levels

If every position of *c*'s probe list reuses, the retained trail may be deeper
than `|probes|` — assumptions from a longer previous candidate that are **not**
literals of *c*. VP3 already makes that harmless (no value is read on a reuse
branch, so no inference is drawn), but the state is untidy and one careless
later edit away from being wrong. Step 3 therefore trims `k.level` down to
`|probes|` up front, so the trail can never hold an assumption outside the
current candidate. The all-reused case then yields nothing — correctly: all
`|probes|` assumptions are in force and produced no conflict — and is counted
as `vivify_reuse_full` rather than silently absorbed.

### 3.5 What is *not* claimed

Reuse is an incompleteness, not just a speed-up: the retained levels were
propagated with a different `ignore`, so they lack the propagations that would
have gone *through* the previous candidate and carry propagations through the
current one. Both directions only change which strengthenings are *found*.
Every strengthening that is found is still checked by the same chain
construction as before.

---

## 4. Reachability evidence (pre-registered)

A reuse rate of zero is a reportable result, and these counters are what make
that statement falsifiable rather than an assumption. All are unconditional.

| counter | what it proves |
|---|---|
| `vivify_reused` | assumption levels taken from the retained trail |
| `vivify_probes` | assumptions actually made (`reused / (reused + probes)` = the reuse rate; cadical claims 30–50 %) |
| `vivify_reuse_full` | candidates entirely covered by the retained prefix (§3.4) |
| `vivify_tier3_checked` | tier3 clauses reaching the probe loop — **zero means §2.5 is inert** |
| `vivify_prioritized` | candidates carrying the F5 bit at scheduling; **and** `vivify_reprioritized`, the rounds where the leftover set was empty and everything was re-armed. One count alone cannot distinguish "the bit works" from "the bit is always all-ones" ([[ab-arm-identical-artefacts]]) |
| `vivify_polls` / `vivify_terminated` | the H1 site is reached / the H1 site fires |

Printed as a `c vivify2` line beside the existing `c vivify`.

---

## 5. A/B specification

Not run here (the 4-core allocation is oversubscribed; no tier campaign).

* **Arms, same window, same binary, node-pinned**, via the hatch
  `TACTIX_VIVIFY_REUSE=0` (reverts §2.1–§2.4 to today's order and per-candidate
  root return) and `TACTIX_VIVIFY_TIER3=0` (drops §2.5's fourth pass, redundant
  shares back to 7/10 on one tier2-bounded scope). Both ship **ON**.
* **Read the counters first.** If `vivify_reused` is 0 on the tier, abandon the
  window and report the mechanism inert — the same closure `SCHED-E3` and
  `SCHED-C10` took. If `vivify_tier3_checked` is 0, the tier3 half is inert
  independently of the reuse half.
* Tier C, `--baseline kissat --baseline cadical`, paired draws before any
  single-instance flip is treated as a verdict ([[chaotic-instance-veto]]).
* Two pin dirs if a Tier B arm goes out in the same window.
