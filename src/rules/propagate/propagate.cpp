// Transliterated from kissat/src/proplit.h (MIT, Biere et al.): blocking
// literals, binary clauses inlined in watch lists, in-place watch-list
// compaction, `searched` scan resumption. We diverge in one place: watch
// pushes to *other* literals' lists are safe immediately here (per-literal
// std::vectors), so kissat's delayed-watch mechanism is unnecessary.
#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

namespace tactix {

bool propagate_guard(const Kernel &k) {
  return !k.conflict_valid && !k.inconsistent &&
         k.propagate_head < k.trail.size();
}

// returns true iff a conflict was found (recorded in the kernel)
static bool propagate_literal(Kernel &k, Lit lit) {
  const Lit not_lit = negate(lit);
  WatchList &ws = k.watches[not_lit];
  uint32_t *q = ws.data();
  const uint32_t *p = q;
  const uint32_t *const end = ws.data() + ws.size();
  bool conflict = false;

  while (p != end) {
    const Watch head{*q++ = *p++};
    const Lit blocking = head.blocking();
    const int8_t blocking_value = k.values[blocking];
    if (head.binary()) {
      if (blocking_value > 0)
        continue;
      if (blocking_value < 0) { // conflict: clause (not_lit, blocking)
        k.conflict_valid = true;
        k.conflict_ref = INVALID_REF;
        k.conflict_bin[0] = not_lit;
        k.conflict_bin[1] = blocking;
        conflict = true;
        break;
      }
      kernel_assign_binary(k, blocking, not_lit);
      k.stats.propagations++;
    } else {
      const Ref ref = *q++ = *p++;
      if (blocking_value > 0)
        continue;
      Clause *c = clause_at(k.arena, ref);
      if (c->garbage()) {
        q -= 2;
        continue;
      }
      Lit *lits = c->lits;
      const Lit other = lits[0] ^ lits[1] ^ not_lit;
      const int8_t other_value = k.values[other];
      if (other_value > 0) { // satisfied: remember as blocking literal
        Watch w = blocking_watch(other);
        q[-2] = w.raw;
        continue;
      }
      // scan for a non-false replacement, resuming at c->searched
      const Lit *const end_lits = lits + c->size;
      Lit *const searched = lits + c->searched;
      Lit *r;
      Lit replacement = INVALID_LIT;
      int8_t replacement_value = -1;
      for (r = searched; r != end_lits; r++) {
        replacement = *r;
        replacement_value = k.values[replacement];
        if (replacement_value >= 0)
          break;
      }
      if (replacement_value < 0) {
        for (r = lits + 2; r != searched; r++) {
          replacement = *r;
          replacement_value = k.values[replacement];
          if (replacement_value >= 0)
            break;
        }
      }
      if (replacement_value >= 0) { // move the watch
        c->searched = static_cast<uint32_t>(r - lits);
        lits[0] = other;
        lits[1] = replacement;
        *r = not_lit;
        q -= 2;
        push_large_watch(k.watches[replacement], other, ref);
      } else if (other_value < 0) { // conflict
        k.conflict_valid = true;
        k.conflict_ref = ref;
        conflict = true;
        break;
      } else { // unit: assign `other` with this clause as reason
        kernel_assign_ref(k, other, ref);
        k.stats.propagations++;
      }
    }
  }
  while (p != end)
    *q++ = *p++;
  ws.resize(static_cast<size_t>(q - ws.data()));
  return conflict;
}

ApplyResult propagate_apply(Kernel &k) {
  if (k.inconsistent)
    return ApplyResult::Unsat;
  while (k.propagate_head < k.trail.size()) {
    const Lit lit = k.trail[k.propagate_head++];
    if (propagate_literal(k, lit)) {
      k.stats.conflicts++;
      if (k.level == 0) { // root conflict: the formula is refuted
        k.inconsistent = true;
        k.conflict_valid = false;
        if (k.proof)
          k.proof->add_empty();
        return ApplyResult::Unsat;
      }
      return ApplyResult::Conflict;
    }
  }
  return ApplyResult::Ok;
}

TACTIX_REGISTER_RULE(Propagate, RuleTier::Micro, propagate_guard,
                     propagate_apply);

} // namespace tactix
