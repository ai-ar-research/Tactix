// Sweep's sub-solver engine and its refutation derivation. See subsolver.hpp
// for the contract and docs/design/v5-sweep-design.md §3/§4 for why it exists.
// Ordinary CDCL, written small on purpose.
#include "engine/subsolver.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace tactix {

// Where this deviates from the kernel, and why:
//  - Watch lists are vectors, not a flat arena: tiny environments do not repay
//    the kernel's delayed-relocation discipline (`watch.hpp`).
//  - Binaries are not inlined into them: every clause needs an id to appear in
//    a proof chain, and uniformity beats the binary fast path at this scale.
//  - BVE's fallback decisions run in fixed variable order. Sweep selects the
//    full Kitten-shaped imported-variable VMTF queue and deterministic
//    randomized phase stream; both modes share proof analysis and budgets.
//  - Assumptions are MiniSat-shaped: each owns a level (possibly empty), and a
//    falsified assumption ends the solve as `Unsat`.

namespace {
constexpr SubLit NO_LIT = UINT32_MAX;
constexpr SubVar NO_VAR = UINT32_MAX;
constexpr uint32_t KITTEN_CACHE_LINE_SHIFT = 7;
constexpr uint64_t KITTEN_RANDOM_MULTIPLIER = 6364136223846793005ull;
constexpr uint64_t KITTEN_RANDOM_INCREMENT = 1442695040888963407ull;
} // namespace

void SubSolver::start_kitten_session(bool clear_before_first_environment) {
  // Sweep uses a freshly constructed embedded Kitten directly: initialize
  // consumes seed 0 and that first environment keeps generator 0. Eliminate
  // also constructs Kitten, but every definition starts with `kitten_clear`,
  // so its first environment consumes seed 1. Reset models the environment
  // boundary; seed its initialized counter for the owning caller's lifecycle.
  kitten_initialized_ = clear_before_first_environment ? 1 : 0;
  kitten_capacity_vars_ = 0;
}

uint64_t SubSolver::next_kitten_random() {
  kitten_generator_ *= KITTEN_RANDOM_MULTIPLIER;
  kitten_generator_ += KITTEN_RANDOM_INCREMENT;
  return kitten_generator_;
}

uint32_t SubSolver::pick_kitten_random(uint32_t begin, uint32_t end) {
  assert(begin <= end);
  if (begin == end)
    return begin;
  const uint32_t delta = end - begin;
  const uint32_t random = static_cast<uint32_t>(next_kitten_random() >> 32);
  const double fraction = random / 4294967296.0;
  const uint32_t scaled = static_cast<uint32_t>(delta * fraction);
  assert(scaled < delta);
  return begin + scaled;
}

void SubSolver::reset(uint32_t nvars, SubSearchMode mode) {
  reset_impl(nvars, mode, false);
}

void SubSolver::reset_compact_kitten(uint32_t nvars) {
  reset_impl(nvars, SubSearchMode::Kitten, true);
}

void SubSolver::reset_impl(uint32_t nvars, SubSearchMode mode,
                           bool compact_identity) {
  assert(!compact_identity || mode == SubSearchMode::Kitten);
  // `kitten_clear` clears watch stacks only through the previous environment's
  // active literal count (`kitten->lits`), while retaining the outer array and
  // every stack capacity up to its historical allocation size.  Remember that
  // active prefix before resetting the import count below.
  const size_t previous_active_lits =
      2 * static_cast<size_t>(imported_vars_);
  assert(previous_active_lits <= watches_.size());
  nvars_ = nvars;
  compact_identity_ = compact_identity;
  imported_vars_ =
      mode == SubSearchMode::Fixed || compact_identity ? nvars : 0;
  phase_capacity_vars_ =
      mode == SubSearchMode::Fixed ? nvars : kitten_capacity_vars_;
  search_mode_ = mode;
  if (mode == SubSearchMode::Kitten)
    kitten_generator_ = kitten_initialized_++;
  else
    kitten_generator_ = 0;
  inconsistent_ = false;
  solved_ = false;
  model_valid_ = false;
  arena_.clear();
  // `kitten_clear` empties every per-literal stack but retains its allocation.
  // Definition extraction can reset this engine millions of times, so
  // destroying the nested vectors here turns identical logical work into
  // allocator traffic.  Keep the high-water outer image and clear only its
  // contents; all consumers index through the current `nvars_` bound.
  for (size_t lit = 0; lit < previous_active_lits; ++lit)
    watches_[lit].clear();
  // The occurrence index is a safety net for Fixed-mode model flips.  Kitten
  // uses only its watched clauses for that operation (as does upstream
  // `flip_literal`) and never reads `occurrences_`; clearing and rebuilding a
  // second per-literal index for every BVE definition was pure representation
  // cost.  If this object returns to Fixed mode, clear the retained high-water
  // image before repopulating it below.
  if (mode == SubSearchMode::Fixed)
    for (std::vector<SubRef> &occs : occurrences_)
      occs.clear();
  const size_t nlits = 2 * static_cast<size_t>(nvars);
  if (watches_.size() < nlits)
    watches_.resize(nlits);
  if (mode == SubSearchMode::Fixed && occurrences_.size() < nlits)
    occurrences_.resize(nlits);
  imported_clause_.clear();
  for (const SubVar v : clause_touched_)
    clause_marks_[v] = 0;
  clause_touched_.clear();
  if (clause_marks_.size() < nvars)
    clause_marks_.resize(nvars, 0);
  values_.assign(2 * static_cast<size_t>(nvars), 0);
  model_.assign(nvars, 1);
  phase_.assign(std::max(nvars, phase_capacity_vars_), 0);
  if (compact_identity)
    import_.clear();
  else
    import_.assign(nvars, 0);
  export_.clear();
  if (!compact_identity)
    export_.reserve(nvars);
  units_.clear();
  queue_prev_.assign(nvars, NO_VAR);
  queue_next_.assign(nvars, NO_VAR);
  queue_stamp_.assign(nvars, 0);
  queue_first_ = queue_last_ = queue_search_ = NO_VAR;
  queue_next_stamp_ = 0;
  if (mode == SubSearchMode::Fixed)
    for (SubVar v = 0; v < nvars; v++) {
      import_[v] = v + 1;
      export_.push_back(v);
    }
  else if (compact_identity) {
    // This is exactly what first-occurrence `import_literal` would construct
    // for dense ids 0..nvars-1, without writing or consulting an identity map
    // for every clause literal.
    for (SubVar v = 0; v < nvars; ++v)
      queue_enqueue(v);
    queue_update_search(queue_last_);
    if (nvars && !phase_capacity_vars_)
      phase_capacity_vars_ = 2;
    while (phase_capacity_vars_ < nvars)
      phase_capacity_vars_ *= 2;
    kitten_capacity_vars_ = phase_capacity_vars_;
    if (phase_.size() < phase_capacity_vars_)
      phase_.resize(phase_capacity_vars_, 0);
  }
  trail_.clear();
  frames_.assign(1, 0);
  level_.assign(nvars, 0);
  trail_pos_.assign(nvars, 0);
  reason_.assign(nvars, NO_REF);
  propagate_head_ = 0;
  conflict_ref_ = NO_REF;
  pending_refutation_ = NO_REF;
  seen_.assign(nvars, 0);
  analyzed_.clear();
  learned_.clear();
  cone_seen_.assign(nvars, 0);
  cone_touched_.clear();
  cone_stack_.clear();
  chain_entries_.clear();
  derived_.clear();
  target_ = {};
  have_derivation_ = false;
  target_is_derived_ = false;
  next_id_ = 1;
  original_clauses_ = 0;
  decision_cursor_ = 0;
  ticks_ = 0;
  kitten_ticks_ = 0;
  budget_ = 0;
  tick_mode_ = SubTickMode::Entries;
  conflicts_ = 0;
}

void SubSolver::queue_update_search(SubVar v) {
  if (search_mode_ == SubSearchMode::Kitten)
    queue_search_ = v;
}

void SubSolver::queue_enqueue(SubVar v) {
  assert(v < imported_vars_);
  queue_prev_[v] = queue_last_;
  queue_next_[v] = NO_VAR;
  if (queue_last_ == NO_VAR)
    queue_first_ = v;
  else
    queue_next_[queue_last_] = v;
  queue_last_ = v;
  queue_stamp_[v] = queue_next_stamp_++;
}

void SubSolver::queue_dequeue(SubVar v) {
  const SubVar prev = queue_prev_[v], next = queue_next_[v];
  if (prev == NO_VAR)
    queue_first_ = next;
  else
    queue_next_[prev] = next;
  if (next == NO_VAR)
    queue_last_ = prev;
  else
    queue_prev_[next] = prev;
}

void SubSolver::queue_move_to_front(SubVar v) {
  if (search_mode_ != SubSearchMode::Kitten || v == queue_last_)
    return;
  queue_dequeue(v);
  queue_enqueue(v);
}

void SubSolver::queue_bump_analyzed() {
  if (search_mode_ != SubSearchMode::Kitten)
    return;
  for (const SubVar v : analyzed_)
    queue_move_to_front(v);
}

SubLit SubSolver::import_literal(SubLit external) {
  const SubVar ev = sub_var_of(external);
  assert(ev < nvars_);
  if (search_mode_ == SubSearchMode::Fixed || compact_identity_)
    return external;
  uint32_t mapped = import_[ev];
  if (!mapped) {
    const SubVar iv = imported_vars_++;
    import_[ev] = iv + 1;
    export_.push_back(ev);
    mapped = iv + 1;
    queue_enqueue(iv);
    queue_update_search(queue_last_);
    if (!phase_capacity_vars_)
      phase_capacity_vars_ = 2;
    while (phase_capacity_vars_ < imported_vars_)
      phase_capacity_vars_ *= 2;
    kitten_capacity_vars_ = phase_capacity_vars_;
    if (phase_.size() < phase_capacity_vars_)
      phase_.resize(phase_capacity_vars_, 0);
  }
  return sub_lit(mapped - 1, (external & 1u) != 0);
}

SubLit SubSolver::export_literal(SubLit internal) const {
  const SubVar iv = sub_var_of(internal);
  if (compact_identity_) {
    assert(iv < nvars_);
    return internal;
  }
  assert(iv < export_.size());
  return sub_lit(export_[iv], (internal & 1u) != 0);
}

std::vector<SubLit>
SubSolver::export_literals(const std::vector<SubLit> &internal) const {
  std::vector<SubLit> result;
  result.reserve(internal.size());
  for (const SubLit l : internal)
    result.push_back(export_literal(l));
  return result;
}

void SubSolver::randomize_phases() {
  assert(search_mode_ == SubSearchMode::Kitten);
  uint64_t random = next_kitten_random();
  uint32_t i = 0;
  const uint32_t blocks = phase_capacity_vars_ & ~63u;
  while (i != blocks) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    // Match Kitten's eight word stores.  Every byte in word 'bit' receives
    // that bit from the corresponding byte of the random word.  memcpy keeps
    // the stores alignment-safe for vector<uint8_t> while giving the compiler
    // the same fixed-width expansion opportunity as upstream Kitten.
    constexpr uint64_t BYTE_LOW_BITS = 0x0101010101010101ull;
    uint64_t expanded[8];
    for (uint32_t bit = 0; bit < 8; bit++)
      expanded[bit] = (random >> bit) & BYTE_LOW_BITS;
    std::memcpy(phase_.data() + i, expanded, sizeof expanded);
#else
    // Preserve the architecture-independent scalar mapping where native word
    // byte order does not match the representation used by Kitten's fast path.
    for (uint32_t bit = 0; bit < 8; bit++)
      for (uint32_t byte = 0; byte < 8; byte++)
        phase_[i + 8 * bit + byte] =
            static_cast<uint8_t>((random >> (bit + 8 * byte)) & 1u);
#endif
    random = next_kitten_random();
    i += 64;
  }
  uint32_t shift = 0;
  while (i != phase_capacity_vars_)
    phase_[i++] = static_cast<uint8_t>((random >> shift++) & 1u);
}

void SubSolver::attach(SubRef ref) {
  const SubLit *c = clause_lits(ref);
  watches_[c[0]].push_back({c[1], ref});
  watches_[c[1]].push_back({c[0], ref});
}

void SubSolver::assign(SubLit l, SubRef reason) {
  assert(values_[l] == 0);
  if (search_mode_ == SubSearchMode::Kitten &&
      frames_.size() == 1 && reason != NO_REF &&
      clause_size(reason) > 1) {
    // Kitten materializes every non-unit root implication as a learned unit.
    // Besides making later incremental solves re-propagate the same root
    // state, this records the antecedent cone needed by Sweep's proof replay.
    chain_clear();
    const SubLit *c = clause_lits(reason);
    const uint32_t size = clause_size(reason);
    for (uint32_t i = 0; i < size; i++)
      if (c[i] != l)
        record_cone(sub_var_of(c[i]), nullptr);
    const std::vector<SubLit> unit{l};
    reason = add_derived(unit, chain_build(reason));
    chain_clear();
  }
  values_[l] = 1;
  values_[sub_negate(l)] = -1;
  const SubVar v = sub_var_of(l);
  phase_[v] = static_cast<uint8_t>(l & 1u);
  level_[v] = static_cast<uint32_t>(frames_.size()) - 1;
  trail_pos_[v] = static_cast<uint32_t>(trail_.size());
  reason_[v] = reason;
  trail_.push_back(l);
}

uint32_t SubSolver::add_clause(const std::vector<SubLit> &lits) {
  assert(!solved_ && "original clauses must all precede the first solve");
  const uint32_t id = next_id_++;
  kitten_ticks_++; // Kitten `new_reference`
  if (inconsistent_)
    return id;
  // Dedup and drop tautologies. Nothing is shrunk against the root assignment:
  // the clause DB must hold the clauses the caller named, since the returned
  // core indexes them.  Kitten does this with one signed mark per variable
  // (`kitten_clause_with_id_and_exception`); a nested scan of `c` is quadratic
  // on long definition clauses and used to dominate BVE's real wall time.
  // Kitten reuses `eclause` across imports.  This scratch is likewise owned by
  // the sub-solver session: the arena receives its own copy below, so clearing
  // it for the next clause cannot alter clause identity or literal order.
  std::vector<SubLit> &c = imported_clause_;
  c.clear();
  c.reserve(lits.size());
  assert(clause_touched_.empty());
  const auto clear_clause_marks = [&] {
    for (const SubVar v : clause_touched_)
      clause_marks_[v] = 0;
    clause_touched_.clear();
  };
  for (const SubLit external : lits) {
    assert(sub_var_of(external) < nvars_);
    const SubLit l = import_literal(external);
    const SubVar v = sub_var_of(l);
    const int8_t sign = (l & 1u) ? -1 : 1;
    const int8_t mark = clause_marks_[v];
    if (mark == sign)
      continue; // same-sign duplicate: keep its first position
    if (mark == -sign) {
      clear_clause_marks();
      return id; // tautology: keeps its id, never attached, never cited
    }
    assert(mark == 0);
    clause_marks_[v] = sign;
    clause_touched_.push_back(v);
    c.push_back(l);
  }
  clear_clause_marks();
  arena_.push_back(id);
  arena_.push_back(static_cast<SubLit>(c.size()));
  const SubRef ref = static_cast<SubRef>(arena_.size());
  arena_.insert(arena_.end(), c.begin(), c.end());
  if (search_mode_ == SubSearchMode::Fixed)
    for (const SubLit l : c)
      occurrences_[l].push_back(ref);
  if (c.empty()) { // the empty clause, stated outright
    inconsistent_ = true;
    pending_refutation_ = ref; // the target is built at the first solve, once
    return id;                 // the original-id watershed is fixed
  }
  if (c.size() == 1) {
    if (search_mode_ == SubSearchMode::Kitten) {
      units_.push_back(ref);
      return id;
    }
    // a unit is never watched (it can never propagate again); it is the reason
    // for its own literal, which is what puts its id into later chains
    if (values_[c[0]] > 0)
      return id;
    if (values_[c[0]] < 0) {
      inconsistent_ = true;
      pending_refutation_ = ref;
      return id;
    }
    assign(c[0], ref);
    return id;
  }
  attach(ref);
  return id;
}

SubSolver::SubRef SubSolver::add_derived(const std::vector<SubLit> &lits,
                                         const std::vector<uint32_t> &chain) {
  const uint32_t id = next_id_++;
  kitten_ticks_++; // learned clauses use the same `new_reference`
  derived_.push_back({id, export_literals(lits), chain});
  arena_.push_back(id);
  arena_.push_back(static_cast<SubLit>(lits.size()));
  const SubRef ref = static_cast<SubRef>(arena_.size());
  arena_.insert(arena_.end(), lits.begin(), lits.end());
  if (search_mode_ == SubSearchMode::Fixed)
    for (const SubLit l : lits)
      occurrences_[l].push_back(ref);
  if (lits.size() >= 2)
    attach(ref);
  else if (search_mode_ == SubSearchMode::Kitten)
    units_.push_back(ref);
  return ref;
}

bool SubSolver::propagate() {
  while (propagate_head_ < trail_.size()) {
    const SubLit lit = trail_[propagate_head_++];
    const SubLit falsified = sub_negate(lit);
    std::vector<SubWatch> &ws = watches_[falsified];
    ticks_ += ws.size(); // effort currency: watch entries inspected
    kitten_ticks_ +=
        1 + (ws.size() * sizeof(SubWatch) >> KITTEN_CACHE_LINE_SHIFT);
    size_t i = 0, j = 0;
    const size_t n = ws.size();
    for (; i < n;) {
      const SubWatch w = ws[i++];
      if (values_[w.blocking] > 0) { // blocking literal already satisfies it
        ws[j++] = w;
        continue;
      }
      SubLit *c = clause_lits(w.ref);
      const uint32_t size = clause_size(w.ref);
      kitten_ticks_ += size > 2;
      SubLit other;
      if (search_mode_ == SubSearchMode::Kitten) {
        // Kitten identifies the other watch without orienting the pair.  A
        // mere satisfied/unit/conflicting visit must preserve clause order:
        // that order is the next conflict analysis' visitation order and thus
        // part of the VMTF trajectory.  Only an actual watch move rewrites it.
        other = c[0] ^ c[1] ^ falsified;
      } else {
        if (c[0] == falsified)
          std::swap(c[0], c[1]);
        assert(c[1] == falsified);
        other = c[0];
      }
      if (values_[other] > 0) {
        ws[j++] = {other, w.ref};
        continue;
      }
      uint32_t r = 2;
      for (; r < size; r++)
        if (values_[c[r]] >= 0)
          break;
      ticks_ += size;
      if (r < size) { // re-watch: this entry leaves the list
        if (search_mode_ == SubSearchMode::Kitten) {
          c[0] = other;
          c[1] = c[r];
          c[r] = falsified;
        } else {
          std::swap(c[1], c[r]);
        }
        assert(c[1] != falsified);
        watches_[c[1]].push_back({other, w.ref});
        continue;
      }
      // Kitten refreshes the blocking literal only on the satisfied fast path.
      // A unit or conflict keeps the existing watch record (whose blocker can
      // be stale after the opposite watch moved); preserving that cache state
      // is necessary for the same later watch-list traversal.
      ws[j++] = search_mode_ == SubSearchMode::Kitten
                    ? w
                    : SubWatch{other, w.ref};
      if (values_[other] < 0) {
        conflict_ref_ = w.ref;
        while (i < n)
          ws[j++] = ws[i++];
        ws.resize(j);
        return false;
      }
      assign(other, w.ref);
    }
    ws.resize(j);
  }
  return true;
}

void SubSolver::backtrack(uint32_t level) {
  // `frames_[i]` is the trail index where level `i` starts, so keeping levels
  // 0..level means truncating to where level+1 began (the kernel's convention;
  // frames_.size() == current level + 1).
  const uint32_t current = static_cast<uint32_t>(frames_.size()) - 1;
  if (level >= current)
    return;
  const uint32_t keep = frames_[level + 1];
  while (trail_.size() > keep) {
    const SubLit l = trail_.back();
    trail_.pop_back();
    const SubVar v = sub_var_of(l);
    values_[l] = 0;
    values_[sub_negate(l)] = 0;
    reason_[v] = NO_REF;
    if (search_mode_ == SubSearchMode::Kitten &&
        (queue_search_ == NO_VAR ||
         queue_stamp_[v] > queue_stamp_[queue_search_]))
      queue_update_search(v);
  }
  frames_.resize(level + 1);
  propagate_head_ = keep;
  decision_cursor_ = 0; // fixed order: rescan from the start
}

void SubSolver::clear_assignments() {
  for (const SubLit l : trail_) {
    const SubVar v = sub_var_of(l);
    values_[l] = values_[sub_negate(l)] = 0;
    level_[v] = 0;
    reason_[v] = NO_REF;
    if (queue_search_ == NO_VAR ||
        queue_stamp_[v] > queue_stamp_[queue_search_])
      queue_update_search(v);
  }
  trail_.clear();
  frames_.assign(1, 0);
  propagate_head_ = 0;
  decision_cursor_ = 0;
}

bool SubSolver::propagate_units() {
  for (const SubRef ref : units_) {
    const SubLit unit = clause_lits(ref)[0];
    if (values_[unit] > 0)
      continue;
    if (values_[unit] < 0) {
      conflict_ref_ = ref;
      return false;
    }
    assign(unit, ref);
  }
  return propagate();
}

// --- proof plumbing ---------------------------------------------------------
// The kernel's construction, transplanted: antecedents are collected as (trail
// position, clause id) pairs and emitted ascending with the conflicting clause
// last, the order a checker's own propagation visits them
// (`kernel_lrat_chain`). Every lemma follows from the environment alone —
// assumptions enter as decisions and leave as literals of the learned clause,
// never as premises — so lemmas stay valid across assumption sets and restarts.

void SubSolver::chain_clear() {
  chain_entries_.clear();
  for (const SubVar v : cone_touched_)
    cone_seen_[v] = 0;
  cone_touched_.clear();
}

void SubSolver::record_cone(SubVar v, std::vector<SubLit> *decisions) {
  cone_stack_.clear();
  cone_stack_.push_back(v);
  while (!cone_stack_.empty()) {
    const SubVar w = cone_stack_.back();
    cone_stack_.pop_back();
    if (cone_seen_[w])
      continue;
    cone_seen_[w] = 1;
    cone_touched_.push_back(w);
    const SubRef reason = reason_[w];
    if (reason == NO_REF) { // a decision: an assumption the target negates
      if (decisions)
        decisions->push_back(trail_[trail_pos_[w]]);
      continue;
    }
    chain_entries_.emplace_back(trail_pos_[w], clause_id(reason));
    const uint32_t size = clause_size(reason);
    const SubLit *c = clause_lits(reason);
    for (uint32_t i = 0; i < size; i++)
      if (sub_var_of(c[i]) != w)
        cone_stack_.push_back(sub_var_of(c[i]));
  }
}

std::vector<uint32_t> SubSolver::chain_build(SubRef closing) {
  std::sort(chain_entries_.begin(), chain_entries_.end());
  std::vector<uint32_t> chain;
  chain.reserve(chain_entries_.size() + 1);
  for (const auto &e : chain_entries_)
    chain.push_back(e.second);
  if (closing != NO_REF)
    chain.push_back(clause_id(closing));
  return chain;
}

void SubSolver::refute_empty(SubRef conflict) {
  // Every literal of `conflict` is false at level 0, so the empty clause
  // follows from their reason cones plus the conflicting clause itself.
  inconsistent_ = true;
  chain_clear();
  const uint32_t size = clause_size(conflict);
  const SubLit *c = clause_lits(conflict);
  for (uint32_t i = 0; i < size; i++)
    record_cone(sub_var_of(c[i]), nullptr);
  // Kitten materializes a root conflict as a learned empty clause, and
  // `new_reference` charges one tick.  A stated original empty clause is
  // already `inconsistent` and does not allocate that lemma.
  if (search_mode_ == SubSearchMode::Kitten && clause_size(conflict))
    kitten_ticks_++;
  target_ = {next_id_++, {}, chain_build(conflict)};
  have_derivation_ = true;
  target_is_derived_ = false;
}

bool SubSolver::analyze() {
  const uint32_t current = static_cast<uint32_t>(frames_.size()) - 1;
  assert(current > 0);
  learned_.clear();
  learned_.push_back(0); // placeholder for the asserting literal
  for (const SubVar v : analyzed_)
    seen_[v] = 0;
  analyzed_.clear();
  chain_clear();

  uint32_t count = 0; // seen, unresolved, at the current level
  uint32_t bj = 0;
  const auto see = [&](SubLit l) {
    const SubVar v = sub_var_of(l);
    if (seen_[v])
      return;
    if (level_[v] == 0 && search_mode_ != SubSearchMode::Kitten) {
      // Dropped from the clause, so the checker must re-derive it and its whole
      // reason cone joins the chain. The kernel emits a unit lemma per root
      // literal instead; at this scale the cone is cheaper.
      record_cone(v, nullptr);
      return;
    }
    // Kitten deliberately retains root-false literals here. If the learned
    // clause backjumps to zero, `assign` first stores that clause and then
    // materializes its asserting consequence as a separate learned unit.
    seen_[v] = 1;
    analyzed_.push_back(v);
    if (level_[v] == current)
      count++;
    else if (search_mode_ == SubSearchMode::Kitten) {
      // Kitten promotes each newly deepest lower-level literal into watch 1
      // immediately. Replacing that with one final swap preserves the jump
      // level but permutes the clause tail after multiple promotions, which
      // later changes propagation and the VMTF analysis order.
      const uint32_t candidate = level_[v];
      if (candidate > bj) {
        bj = candidate;
        if (learned_.size() > 1)
          std::swap(l, learned_[1]);
      }
      learned_.push_back(l);
    } else
      learned_.push_back(l);
  };
  {
    const uint32_t size = clause_size(conflict_ref_);
    const SubLit *c = clause_lits(conflict_ref_);
    for (uint32_t i = 0; i < size; i++)
      see(c[i]);
  }
  // Unreachable with correct two-watch propagation: a conflict whose literals
  // all sit below the current level would have fired at that lower level. If it
  // ever happens the caller must draw no conclusion rather than report Unsat —
  // a refutation from an unanalyzed conflict is the one way this engine could
  // lie to its caller.
  assert(count > 0);
  if (count == 0)
    return false;

  size_t idx = trail_.size();
  SubLit uip = 0;
  for (;;) {
    SubVar v;
    do {
      assert(idx > 0);
      v = sub_var_of(trail_[--idx]);
    } while (!seen_[v]);
    count--;
    if (count == 0) {
      uip = trail_[idx];
      break;
    }
    // Resolve on v: every other literal of its reason joins the clause, and the
    // reason itself joins the chain at v's trail position.
    const SubLit resolved = trail_[idx];
    assert(reason_[v] != NO_REF); // a decision is never resolved on
    chain_entries_.emplace_back(trail_pos_[v], clause_id(reason_[v]));
    const uint32_t size = clause_size(reason_[v]);
    const SubLit *c = clause_lits(reason_[v]);
    for (uint32_t i = 0; i < size; i++)
      if (c[i] != resolved)
        see(c[i]);
  }
  learned_[0] = sub_negate(uip);
  const std::vector<uint32_t> chain = chain_build(conflict_ref_);
  queue_bump_analyzed();
  for (const SubVar v : analyzed_)
    seen_[v] = 0;
  analyzed_.clear();

  // second watch = the deepest of the remaining literals (the backjump level)
  if (search_mode_ != SubSearchMode::Kitten && learned_.size() > 1) {
    size_t best = 1;
    for (size_t i = 2; i < learned_.size(); i++)
      if (level_[sub_var_of(learned_[i])] > level_[sub_var_of(learned_[best])])
        best = i;
    std::swap(learned_[1], learned_[best]);
    bj = level_[sub_var_of(learned_[1])];
  }
  backtrack(bj);
  const SubRef ref = add_derived(learned_, chain);
  assign(learned_[0], ref);
  return true;
}

void SubSolver::fail_assumptions(
    const std::vector<SubLit> &assumptions) {
  assert(search_mode_ == SubSearchMode::Kitten);
  assert(!assumptions.empty());
  assert(analyzed_.empty());

  // Kitten does not necessarily analyze the assumption at the current pseudo
  // decision level.  It scans the complete registered set, preferring a
  // root-falsified assumption, then one clashing directly with another
  // assumption, and only then the first propagated failure.  This precedence
  // is observable: the resulting core clause remains in the incremental
  // subsolver and changes later Sweep queries.
  SubLit first_failed = NO_LIT;
  SubLit failed_clashing = NO_LIT;
  SubLit failed_unit = NO_LIT;
  for (const SubLit lit : assumptions) {
    if (values_[lit] >= 0)
      continue;
    if (first_failed == NO_LIT)
      first_failed = lit;
    const SubVar v = sub_var_of(lit);
    if (level_[v] == 0) {
      failed_unit = lit;
      break;
    }
    if (failed_clashing == NO_LIT && reason_[v] == NO_REF)
      failed_clashing = lit;
  }
  const SubLit failed = failed_unit != NO_LIT      ? failed_unit
                        : failed_clashing != NO_LIT ? failed_clashing
                                                   : first_failed;
  assert(failed != NO_LIT);
  const SubVar failed_var = sub_var_of(failed);

  have_derivation_ = false;
  target_is_derived_ = false;
  if (failed_unit != NO_LIT) {
    // The existing learned/original unit is already the complete failing core.
    const SubRef reason = reason_[failed_var];
    assert(reason != NO_REF && clause_size(reason) == 1);
    target_ = {next_id_, {export_literal(sub_negate(failed))},
               {clause_id(reason)}};
    have_derivation_ = true;
    return;
  }
  if (failed_clashing != NO_LIT)
    return; // mutually contradictory assumptions entail only a tautology

  std::vector<SubLit> clause{sub_negate(failed)};
  std::vector<SubRef> resolved;
  std::vector<SubVar> root_work;
  seen_[failed_var] = 1;
  analyzed_.push_back(failed_var);

  // This is Kitten's reverse-trail DFS, including its separate FIFO pass over
  // root variables.  A breadth-first reason-cone walk produces a valid-looking
  // core but a different clause/order and is explicitly rejected by Kitten for
  // LRAT.  Keep the traversal as one semantic unit with failing-core reuse.
  size_t pos = trail_.size();
  uint32_t open = 1;
  while (open) {
    open--;
    SubLit uip;
    SubVar v;
    do {
      assert(pos > 0);
      uip = trail_[--pos];
      v = sub_var_of(uip);
    } while (!seen_[v]);
    const SubRef reason = reason_[v];
    if (reason == NO_REF) {
      clause.push_back(sub_negate(uip));
      continue;
    }
    resolved.push_back(reason);
    const SubLit *c = clause_lits(reason);
    const uint32_t size = clause_size(reason);
    for (uint32_t i = 0; i < size; i++) {
      const SubVar other = sub_var_of(c[i]);
      if (seen_[other])
        continue;
      seen_[other] = 1;
      analyzed_.push_back(other);
      if (level_[other])
        open++;
      else
        root_work.push_back(other);
    }
  }
  for (const SubVar v : root_work) {
    const SubRef reason = reason_[v];
    assert(reason != NO_REF);
    resolved.push_back(reason);
  }
  for (const SubVar v : analyzed_)
    seen_[v] = 0;
  analyzed_.clear();
  assert(!resolved.empty());

  // Clause construction and incremental reuse must follow Kitten's traversal
  // exactly.  LRAT antecedents have a separate ordering contract: replay the
  // same complete cone in trail order, as conflict analysis above does.  The
  // two orders name the same resolution DAG, but only the latter is a RUP
  // propagation sequence for the checker.
  chain_clear();
  record_cone(failed_var, nullptr);
  const std::vector<uint32_t> chain = chain_build(NO_REF);
  if (resolved.size() == 1) {
    // Kitten points at the sole antecedent rather than allocating a duplicate
    // clause.  Expose a synthetic proof target without consuming its id.
    target_ = {next_id_, export_literals(clause), chain};
    target_is_derived_ = false;
  } else {
    add_derived(clause, chain);
    target_ = derived_.back();
    target_is_derived_ = true;
  }
  have_derivation_ = true;
}

SubLit SubSolver::next_decision() {
  if (search_mode_ == SubSearchMode::Kitten) {
    SubVar v = queue_search_;
    while (v != NO_VAR && values_[sub_lit(v, false)] != 0)
      v = queue_prev_[v];
    if (v == NO_VAR)
      return NO_LIT;
    queue_update_search(v);
    return sub_lit(v, phase_[v] != 0);
  }
  while (decision_cursor_ < nvars_) {
    const SubVar v = decision_cursor_;
    if (values_[sub_lit(v, false)] == 0)
      return sub_lit(v, phase_[v] != 0);
    decision_cursor_++;
  }
  return NO_LIT;
}

SubResult SubSolver::solve(const std::vector<SubLit> &assumptions,
                           uint64_t ticks_budget, SubTickMode tick_mode) {
  // Kissat's `kitten_status` ceases to be SAT as soon as another solve starts.
  // In particular an UNSAT-under-assumptions result must disable the model
  // flipping API even though `model_` still stores the preceding SAT model.
  model_valid_ = false;
  if (!solved_) {
    // The original-id watershed is fixed here, not per add_clause: every id
    // handed out from now on names a lemma.
    solved_ = true;
    original_clauses_ = next_id_ - 1;
  }
  budget_ = ticks_budget;
  tick_mode_ = tick_mode;
  std::vector<SubLit> internal_assumptions;
  internal_assumptions.reserve(assumptions.size());
  for (const SubLit a : assumptions)
    internal_assumptions.push_back(import_literal(a));
  const auto over_budget = [&] {
    const uint64_t spent = tick_mode_ == SubTickMode::Kitten
                               ? kitten_ticks_
                               : ticks_;
    // Kitten polls its absolute tick limit with `>=` before choosing another
    // decision (`kitten.c:1572`).  The legacy entry meter predates that port
    // and retains its historical strict-overrun contract for BVE.
    return budget_ && (tick_mode_ == SubTickMode::Kitten
                           ? spent >= budget_
                           : spent > budget_);
  };
  conflicts_ = 0;
  if (inconsistent_) {
    if (!have_derivation_ && pending_refutation_ != NO_REF)
      refute_empty(pending_refutation_); // stated empty clause / clashing unit
    return SubResult::Unsat;
  }
  if (search_mode_ == SubSearchMode::Kitten) {
    clear_assignments();
    if (!propagate_units()) {
      refute_empty(conflict_ref_);
      return SubResult::Unsat;
    }
  } else {
    backtrack(0);
  }
  for (;;) {
    if (!propagate()) {
      const uint32_t current = static_cast<uint32_t>(frames_.size()) - 1;
      conflicts_++;
      if (current == 0) {
        // Level 0 carries no assumptions, so this is a global refutation.
        refute_empty(conflict_ref_);
        return SubResult::Unsat;
      }
      if (!analyze())
        return SubResult::Budget; // see analyze(): no conclusion, not Unsat
      if (over_budget())
        return SubResult::Budget;
      continue;
    }
    const uint32_t current = static_cast<uint32_t>(frames_.size()) - 1;
    if (current < internal_assumptions.size()) {
      // next assumption as its own decision level (MiniSat's shape)
      const SubLit a = internal_assumptions[current];
      if (values_[a] < 0) {
        if (search_mode_ == SubSearchMode::Kitten) {
          fail_assumptions(internal_assumptions);
          return SubResult::Unsat;
        }
        // Refuted under the assumptions: the target clause negates exactly `a`
        // plus every assumption decision in the reason cone of `¬a`.
        have_derivation_ = false;
        const SubVar v = sub_var_of(a);
        if (reason_[v] == NO_REF)
          return SubResult::Unsat; // an earlier assumption was `¬a`: the two
                                   // contradict each other, not the formula
        chain_clear();
        std::vector<SubLit> decisions;
        record_cone(v, &decisions);
        std::vector<SubLit> lits{sub_negate(a)};
        for (const SubLit d : decisions) {
          const SubLit n = sub_negate(d);
          if (std::find(lits.begin(), lits.end(), n) == lits.end())
            lits.push_back(n);
        }
        // The reason of `¬a` sits at the highest trail position in its own
        // cone, so sorting ascending already leaves it closing the chain.
        const std::vector<uint32_t> chain = chain_build(NO_REF);
        if (search_mode_ == SubSearchMode::Kitten && chain.size() > 1) {
          const SubRef target_ref = add_derived(lits, chain);
          (void)target_ref;
          target_ = derived_.back();
          target_is_derived_ = true;
        } else {
          // Kitten reuses a single antecedent as the failing core. Keep a
          // synthetic view for proof capture without consuming a clause id or
          // changing the incremental formula.
          const uint32_t target_id =
              search_mode_ == SubSearchMode::Kitten ? next_id_ : next_id_++;
          target_ = {target_id, export_literals(lits), chain};
          target_is_derived_ = false;
        }
        have_derivation_ = true;
        return SubResult::Unsat;
      }
      frames_.push_back(static_cast<uint32_t>(trail_.size()));
      if (values_[a] == 0)
        assign(a, NO_REF); // else: an empty level, keeping the index
      continue;
    }
    const SubLit d = next_decision();
    if (d == NO_LIT) { // every variable is assigned: a total model
      for (SubVar v = 0; v < imported_vars_; v++)
        model_[compact_identity_ ? v : export_[v]] =
            values_[sub_lit(v, false)] > 0 ? 1 : -1;
      model_valid_ = true;
      return SubResult::Sat;
    }
    if (over_budget())
      return SubResult::Budget;
    frames_.push_back(static_cast<uint32_t>(trail_.size()));
    assign(d, NO_REF);
  }
}

bool SubSolver::flip_model_literal(SubLit lit) {
  assert(solved_ && "a flip requires a SAT model");
  if (!model_valid_)
    return false; // Kissat: `kitten_status (kitten) != 10`
  const SubVar external = sub_var_of(lit);
  if (external >= nvars_ ||
      (!compact_identity_ && !import_[external]))
    return false;
  const SubVar v = compact_identity_ ? external : import_[external] - 1;
  if (level_[v] == 0)
    return false;
  lit = sub_lit(v, model_[external] < 0);
  assert(((lit & 1u) == 0) == (model_[external] > 0));
  std::vector<SubWatch> &ws = watches_[lit];
  kitten_ticks_ +=
      1 + (ws.size() * sizeof(SubWatch) >> KITTEN_CACHE_LINE_SHIFT);
  size_t i = 0, j = 0;
  const size_t n = ws.size();
  bool can_flip = true;
  for (; i < n;) {
    const SubWatch w = ws[i++];
    if (values_[w.blocking] > 0) {
      ws[j++] = w;
      continue;
    }
    SubLit *c = clause_lits(w.ref);
    const uint32_t size = clause_size(w.ref);
    // Unlike propagation's BLIT binary fast path, Kitten's model-flip scan
    // dereferences and charges every non-blocked clause, including binaries.
    kitten_ticks_++;
    if (size == 2) {
      ws[j++] = w;
      can_flip = false;
      break;
    }
    ticks_ += size;
    const SubLit other = c[0] ^ c[1] ^ lit;
    if (values_[other] > 0) {
      ws[j++] = w;
      continue;
    }
    uint32_t replacement = 2;
    while (replacement < size && values_[c[replacement]] < 0)
      replacement++;
    if (replacement == size) {
      ws[j++] = w;
      can_flip = false;
      break;
    }
    c[0] = other;
    c[1] = c[replacement];
    c[replacement] = lit;
    watches_[c[1]].push_back({other, w.ref});
  }
  while (i < n)
    ws[j++] = ws[i++];
  ws.resize(j);
  // Repeated backbone/partition filtering can revisit a variable after an
  // earlier successful flip moved its literal out of a watched position.
  // Kitten relies on its exact caller discipline here; retain the occurrence
  // safety check so this shared engine never turns its model into a non-model.
  if (can_flip && search_mode_ != SubSearchMode::Kitten)
    for (const SubRef ref : occurrences_[lit]) {
      const SubLit *c = clause_lits(ref);
      const uint32_t size = clause_size(ref);
      bool other_true = false;
      for (uint32_t k = 0; k < size; k++)
        if (c[k] != lit && values_[c[k]] > 0) {
          other_true = true;
          break;
        }
      if (!other_true) {
        can_flip = false;
        break;
      }
    }
  if (!can_flip)
    return false;
  values_[lit] = -1;
  values_[sub_negate(lit)] = 1;
  model_[external] = static_cast<int8_t>(-model_[external]);
  return true;
}

int8_t SubSolver::fixed_literal(SubLit lit) const {
  const SubVar external = sub_var_of(lit);
  if (external >= nvars_ ||
      (!compact_identity_ && !import_[external]))
    return 0;
  const SubVar internal =
      compact_identity_ ? external : import_[external] - 1;
  const SubLit mapped = sub_lit(internal, (lit & 1u) != 0);
  const int8_t value = values_[mapped];
  if (!value || level_[internal] != 0)
    return 0;
  return value;
}

void SubSolver::shrink_to_clausal_core(const std::vector<uint32_t> &core) {
  assert(search_mode_ == SubSearchMode::Kitten);
  assert(solved_ && have_derivation_ && !core.empty());

  // `kitten_shrink_to_clausal_core` retains the current literal orientation
  // of each core original, but drops all non-core originals and every learned
  // clause. Preserve caller-visible original ids even when the compacted arena
  // has gaps in that id space.
  if (core_keep_.size() <= original_clauses_)
    core_keep_.resize(static_cast<size_t>(original_clauses_) + 1, 0);
  assert(core_touched_.empty());
  for (const uint32_t id : core) {
    assert(id && id <= original_clauses_);
    if (!core_keep_[id]) {
      core_keep_[id] = 1;
      core_touched_.push_back(id);
    }
  }
  clear_assignments();
  size_t write = 0;
  for (size_t read = 0; read < arena_.size();) {
    const uint32_t id = arena_[read];
    const uint32_t size = arena_[read + 1];
    const size_t next = read + 2 + size;
    assert(next <= arena_.size());
    if (id <= original_clauses_ && core_keep_[id]) {
      const size_t words = next - read;
      if (write != read)
        std::memmove(arena_.data() + write, arena_.data() + read,
                     words * sizeof(SubLit));
      write += words;
    }
    read = next;
  }
  arena_.resize(write);
  for (const uint32_t id : core_touched_)
    core_keep_[id] = 0;
  core_touched_.clear();

  const size_t active_lits = 2 * static_cast<size_t>(imported_vars_);
  assert(active_lits <= watches_.size());
  for (size_t lit = 0; lit < active_lits; ++lit)
    watches_[lit].clear();
  units_.clear();
  inconsistent_ = false;
  pending_refutation_ = NO_REF;
  for (size_t off = 0; off < arena_.size();) {
    const uint32_t size = arena_[off + 1];
    const SubRef ref = static_cast<SubRef>(off + 2);
    if (!size) {
      inconsistent_ = true;
      pending_refutation_ = ref;
    } else {
      if (size == 1)
        units_.push_back(ref);
      else
        attach(ref);
    }
    off += 2 + size;
  }

  // Learned ids may be reused because the first derivation is discarded and
  // only the final refined derivation is exported. Original ids remain fixed.
  next_id_ = original_clauses_ + 1;
  derived_.clear();
  target_ = {};
  have_derivation_ = false;
  target_is_derived_ = false;
  conflict_ref_ = NO_REF;
  model_valid_ = false;
  solved_ = false;
  queue_update_search(queue_last_);
}

void SubSolver::shuffle_clauses() {
  assert(search_mode_ == SubSearchMode::Kitten && !solved_);

  // Exact `kitten_shuffle_clauses` order: VMTF queue, literal-indexed watch
  // stacks, then units. Its picker uses [begin,end), so each position i>0 is
  // exchanged with an earlier position and i==0 consumes no random number.
  for (SubVar i = 0; i < imported_vars_; ++i) {
    const SubVar v = pick_kitten_random(0, imported_vars_);
    queue_move_to_front(v);
  }
  queue_update_search(queue_last_);
  const auto shuffle = [&](auto &items) {
    for (uint32_t i = 0; i < items.size(); ++i) {
      const uint32_t j = pick_kitten_random(0, i);
      if (j != i)
        std::swap(items[i], items[j]);
    }
  };
  const size_t active_lits = 2 * static_cast<size_t>(imported_vars_);
  assert(active_lits <= watches_.size());
  for (size_t lit = 0; lit < active_lits; ++lit)
    shuffle(watches_[lit]);
  shuffle(units_);
}

bool SubSolver::derivation(SubDerivation &out) const {
  out.steps.clear();
  out.core.clear();
  if (!have_derivation_)
    return false;
  // Walk back from the target, marking the lemmas and originals it needs.
  std::vector<uint8_t> needed(next_id_ + 1, 0);
  std::vector<uint32_t> todo{target_.id};
  needed[target_.id] = 1;
  const auto step_of = [&](uint32_t id) -> const SubStep * {
    if (id == target_.id && !target_is_derived_)
      return &target_;
    // derived_ is in ascending id order, so a binary search finds it
    const auto it = std::lower_bound(
        derived_.begin(), derived_.end(), id,
        [](const SubStep &s, uint32_t v) { return s.id < v; });
    if (it == derived_.end() || it->id != id)
      return nullptr;
    return &*it;
  };
  while (!todo.empty()) {
    const uint32_t id = todo.back();
    todo.pop_back();
    const SubStep *s = step_of(id);
    if (!s) { // an original clause: it is part of the core
      out.core.push_back(id);
      continue;
    }
    for (const uint32_t a : s->chain)
      if (!needed[a]) {
        needed[a] = 1;
        todo.push_back(a);
      }
  }
  for (const SubStep &s : derived_)
    if (needed[s.id])
      out.steps.push_back(s);
  if (!target_is_derived_)
    out.steps.push_back(target_);
  std::sort(out.core.begin(), out.core.end());
  out.core.erase(std::unique(out.core.begin(), out.core.end()), out.core.end());
  return true;
}

} // namespace tactix
