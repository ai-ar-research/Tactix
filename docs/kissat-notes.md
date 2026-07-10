# Kissat 4.0.4 study notes — kernel layout, propagation, mode switching

Transliteration notes for phase 1 (plan §5, §7). Source: `kissat/` reference
clone (MIT, Biere et al.) — cite file names in code comments when adopting a
technique; never copy verbatim. File references are to `kissat/src/`.

## 1. Kernel state layout (`internal.h`)

One flat `struct kissat` owns everything; no pointers between substructures,
almost everything is an index. The parts that matter for us:

- **Literal encoding:** variables are `unsigned` indices; literal =
  `2*idx + sign` (`literal.h`: `LIT(idx)`, `NOT(lit)` = `lit ^ 1`,
  `IDX(lit)` = `lit >> 1`). All arrays are literal- or variable-indexed flat
  arrays (`values`, `assigned`, `flags`, `marks`, `watches`, `links`).
- **Clause arena** (`arena.h`, `clause.h`): one `STACK(ward)` holding all
  clauses contiguously; a clause is addressed by a 32-bit `reference`
  (word offset into the arena). Header is bit-packed: `glue:19`, 8 boolean
  flags (`garbage redundant reason shrunken subsume swept vivify quotient`),
  `used:5`, then `searched` (last search position for the replacement scan,
  persisted across propagations), `size`, and the literals inlined
  (`lits[3]` grows past the struct). `kissat_next_clause` walks the arena
  linearly (shrunken clauses pad with `INVALID_LIT` up to original size).
  Binary clauses do NOT live in the arena at all — they exist only inside
  watch lists.
- **Assignment metadata** (`assign.h`): per-variable `struct assigned
  { level, trail, 5 analysis flag bits, reason }`. Reason is a clause
  reference, or the other literal for binary reasons (flag `binary`), or
  sentinels `DECISION_REASON`/`UNIT_REASON` (= UINT_MAX, UINT_MAX-1).
  `values` is a separate signed-char array (fast path reads only it).
- **Trail:** flat `unsigned_array trail` + `propagate` pointer (the
  propagation head is a raw pointer into the trail, not an index).
  `frames` records the trail height per decision level.
- **Heuristic state:** VMTF `links` (doubly linked list with `stamp`) +
  `queue{first,last,stamp,search{idx,stamp}}`; EVSIDS-style `heap scores`
  with `scinc`; `phases` (saved/target/best per var). All of this is
  H-state in our formal model — proof-irrelevant.

## 2. Watches and propagation (`watch.h`, `proplit.h`, `propsearch.c`)

The single most performance-critical design; adopt it as-is.

- A `watch` is one 32-bit union: 31-bit payload + 1-bit `binary` tag.
  Watch lists are flat vectors of these. **Binary clause:** one slot — the
  other literal, tagged binary (the clause is *only* this). **Large
  clause:** two consecutive slots — `blocking` literal (untagged), then the
  arena `reference`. So the hot loop reads a homogeneous `unsigned` stream.
- Propagation of `lit` scans `watches[NOT(lit)]` with two pointers `p`
  (read) and `q` (write), compacting in place:
  1. Read head; if `blocking` literal is true → keep, continue (one memory
     touch, no clause access — this is why blocking lits pay).
  2. Binary: other value false → conflict (a fake 2-lit `clause conflict`
     in the solver struct is used as the conflict object); unassigned →
     assign with binary reason. Never touches the arena.
  3. Large: dereference clause; `other = lits[0]^lits[1]^not_lit` (XOR
     trick instead of a branch); if other true → overwrite blocking lit in
     the watch and keep. Otherwise scan for a replacement starting at
     `c->searched` (resumes where the last scan stopped, wraps to lits+2);
     found → swap into lits[0..1], re-watch under the replacement literal —
     but *delayed*: pushed to `solver->delayed` and flushed after the scan
     (`kissat_watch_large_delayed`), because pushing to another literal's
     vector can reallocate while we iterate. No replacement: other false →
     conflict, else assign `other` with reason `ref`.
  4. Conflict breaks the loop; the tail is copied through and the vector
     end reset (`SET_END_OF_WATCHES`).
- **Ticks accounting:** `ticks` estimates cache lines touched
  (`1 + cache_lines(size_watches)` per literal, +1 per clause access /
  assign). All inprocessing budgets are denominated in these ticks —
  effort limits scale with actual search cost, not wall clock. Adopt this
  unit for our per-rule budgets from day one.
- Root-level conflict ⇒ `inconsistent = true` + empty clause to proof
  (`kissat_update_conflicts_and_trail`) — exactly our Propagate/Analyze
  terminal case.
- Assignment level (`fastassign.h`/`inlineassign.h`) is computed from the
  reason (support for chronological backtracking); binary reasons can be
  "jumped" (use the reason of the other literal) when the instance is
  classified big/big.

## 3. Decide (`decide.c`) — maps to our M2 rule

- Variable selection: in **focused** mode, `last_enqueued_unassigned` walks
  the VMTF queue from the cached `queue.search` position backwards over
  assigned vars (amortized O(1), cache updated); in **stable** mode,
  max-score heap with lazy deletion (`pop` while top is assigned).
  Optional short **random decision sequences** (`randec`, length
  `randeclength * log(count)`) fire at level ≤ 1 on a conflict-count limit.
- Phase selection (`kissat_decide_phase`), in priority order: forced →
  target phase (stable mode, or `target>1`) → saved phase → `INITIAL_PHASE`;
  plus a rotation in focused mode keyed on `(switched>>1)&7` that
  periodically uses initial/inverted initial phases. Target/best phases are
  maintained by autarky-style bookkeeping elsewhere (`phases.c`).
- **For N1:** `kissat_decide` = choose idx (heuristic) + choose phase +
  `push_frame` + `assign_decision`. Our M2 `apply(σ, lit)` is exactly the
  last two steps; `instances(σ)` ranked by the active heuristic replaces
  the first two. Kissat's structure confirms the split is clean — decision
  bookkeeping (level++, frame push) is independent of *who* picked the
  literal (`kissat_internal_assume` is precisely that entry point).

## 4. The search loop (`search.c`) — our DefaultPolicy, literally

```
while (!res):
  conflict = search_propagate()            // M1
  if conflict:            res = analyze()  // M3 (+ M4 inside)
  elif iterating:         iterate()        // report only
  elif !unassigned:       res = 10         // SAT terminal
  elif terminated:        break
  elif reducing():        res = reduce()   // S2 guard → apply
  elif switching_mode():  switch_mode()    // S4
  elif restarting():      restart()        // S1
  elif reordering():      reorder()        // S7
  elif rephasing():       rephase()        // S3
  elif probing():         res = probe()    // V1/V2 umbrella
  elif eliminating():     res = eliminate()// E1 umbrella
  elif limits hit:        break
  else:                   decide()         // M2
```

This is a static priority list of `guard() → apply()` pairs — i.e. Kissat's
scheduler is already the degenerate case of our rule registry with a fixed
order. DefaultPolicy phase 1–2 = this exact chain; the registry’s job is to
make the order data instead of code without adding cost to the hot three
(propagate/analyze/decide). Before the loop: `lucky` probes (S5),
`preprocess`, `classify`.

## 5. Mode switching (`mode.c`) — S4

- Only if `stable == 1` option (0 = always focused, 2 = always stable).
- First switch after `modeinit` **conflicts**; afterwards the limit
  alternates unit: focused→stable happens on a *conflicts* limit, the
  stable phase then lasts a *ticks* budget (`delta_ticks` of the preceding
  focused phase — keeps wall-time share balanced), focused intervals scale
  as `modeint * count·log⁴(count)` (`kissat_nlogpown`), i.e. stable mode
  gets relatively more time as the run ages.
- On switch: swap restart regime (reluctant/Luby init for stable;
  `update_focused_restart_limit` for focused), `update_scores` when
  entering stable, `reset_search_of_queue` when entering focused, reset
  EMAs (`init_averages`), start a random decision sequence.
- Averages/EMAs are kept per mode (`averages[2]`), and learned-clause tier
  thresholds per mode (`tier1[2]/tier2[2]`, currently pinned to shared).

## 6. Misc adoption notes

- `searched` position in the clause header is an easy, measurable win —
  remember it.
- Garbage clauses are skipped (and their watches dropped) lazily during
  propagation; actual collection is separate (`collect.c`).
- The `clause conflict` embedded in the solver struct serves as the
  conflict object for binary clauses — avoids materializing binaries.
- `kissat_update_queue`/stamps: VMTF stamps are plain increasing counters;
  `reassign_queue_stamps` handles wraparound rarely.
- Statistics are macro-based (`INC/ADD/GET`) and compiled out in
  competition builds except a core set; per-rule stats in our design
  should follow the same zero-cost-when-off discipline (§5.3 per-rule
  everything).

## 7. To study when the matching phase starts (not yet read in depth)

- `analyze.c`/`shrink.c`/`minimize.c` (M3, phase 1— read before
  implementing Analyze; includes OTFS and antecedent recording hooks
  relevant to native LRAT).
- `restart.c`/`reluctant.c`/`averages.c` (S1, phase 2).
- `reduce.c`/`tiers.c` (S2, phase 2), `rephase.c` (S3).
- `eliminate.c`/`fastel.c`/kitten (E1, phase 3; O11).
- `sweep.c`, `congruence.c`, `factor.c` (V5/V6/E2, phase 4).
