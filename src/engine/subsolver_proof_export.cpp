// Bounded exception island for exporting an embedded refutation. The main
// solver, including Kitten search, is compiled without exceptions. A very
// large clausal core can exhaust the process's allocation limit while being
// copied into an optional proof capture; that must decline the capture rather
// than terminate the solver. No exception crosses `derivation`'s noexcept
// boundary, and no kernel/formula state is changed here.
#include "engine/subsolver.hpp"

#include <algorithm>
#include <cassert>
#include <new>

namespace tactix {

const SubStep *SubSolver::step_of(uint32_t id) const {
  if (id == target_.id && !target_is_derived_)
    return &target_;
  const auto it = std::lower_bound(
      derived_.begin(), derived_.end(), id,
      [](const SubStep &s, uint32_t v) { return s.id < v; });
  if (it == derived_.end() || it->id != id)
    return nullptr;
  return &*it;
}

bool SubSolver::derivation(SubDerivation &out, bool with_core) const noexcept {
  out.steps.clear();
  out.core.clear();
  out.core_clauses.clear();
  out.resource_exhausted = false;
  if (!have_derivation_)
    return false;
  try {
    // Walk back from the target, marking the lemmas and originals it needs.
    std::vector<uint8_t> needed(next_id_ + 1, 0);
    std::vector<uint32_t> todo{target_.id};
    needed[target_.id] = 1;
    while (!todo.empty()) {
      const uint32_t id = todo.back();
      todo.pop_back();
      const SubStep *s = step_of(id);
      if (!s) {
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
    out.core.erase(std::unique(out.core.begin(), out.core.end()),
                   out.core.end());
    if (with_core)
      compute_clausal_core(out);
    return true;
  } catch (const std::bad_alloc &) {
    // Only the optional capture has been touched. Release its partial prefix
    // when the caller's SubDerivation is destroyed and stop this environment.
    out.steps.clear();
    out.core.clear();
    out.core_clauses.clear();
    out.resource_exhausted = true;
    return false;
  }
}

// `kitten_compute_clausal_core` (`kitten.c:1978-2008`): DFS post-order over
// the refuting lemma's antecedents, preserving Kitten's core-clause order.
void SubSolver::compute_clausal_core(SubDerivation &out) const {
  constexpr uint32_t SENTINEL = UINT32_MAX;
  assert(next_id_ < SENTINEL);
  std::vector<uint8_t> in_core(static_cast<size_t>(next_id_) + 2, 0);
  std::vector<uint32_t> resolved{core_root_};
  std::vector<size_t> slot(static_cast<size_t>(next_id_) + 2, SIZE_MAX);
  while (!resolved.empty()) {
    const uint32_t c_id = resolved.back();
    resolved.pop_back();
    if (c_id == SENTINEL) {
      const uint32_t d_id = resolved.back();
      resolved.pop_back();
      assert(!in_core[d_id]);
      in_core[d_id] = 1;
      const SubStep *d = step_of(d_id);
      slot[d_id] = out.core_clauses.size();
      out.core_clauses.push_back(
          {d_id, d != nullptr, d ? d->lits : std::vector<SubLit>{}});
      continue;
    }
    if (in_core[c_id])
      continue;
    resolved.push_back(c_id);
    resolved.push_back(SENTINEL);
    const SubStep *c = step_of(c_id);
    if (!c)
      continue;
    for (const uint32_t a : c->core_antecedents())
      if (!in_core[a])
        resolved.push_back(a);
  }
  // Core literals use the live watch pose; proof steps retain creation pose.
  for (size_t off = 0; off < arena_.size();) {
    const uint32_t id = arena_[off];
    const uint32_t size = arena_[off + 1];
    const size_t next = off + 2 + size;
    if (id < slot.size() && slot[id] != SIZE_MAX) {
      std::vector<SubLit> live;
      live.reserve(size);
      for (size_t i = off + 2; i < next; ++i)
        live.push_back(export_literal(arena_[i]));
      out.core_clauses[slot[id]].lits = std::move(live);
    }
    off = next;
  }
}

} // namespace tactix
