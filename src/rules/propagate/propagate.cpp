// Transliterated from kissat/src/proplit.h (MIT, Biere et al.): blocking
// literals, binary clauses inlined in watch lists, in-place watch-list
// compaction, `searched` scan resumption, and DELAYED watch relocation —
// with the flat watch arena, pushing to another literal's list during a
// scan can move the arena, so relocations queue up in kernel.delayed and
// flush after each literal's scan (kissat's kissat_watch_large_delayed).
#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

namespace tactix {

bool propagate_guard(const Kernel &k) {
  return !k.conflict_valid && !k.inconsistent &&
         k.propagate_head < k.trail.size();
}

static inline void flush_delayed(Kernel &k) {
  for (size_t i = 0; i < k.delayed.size(); i += 3) {
    const Lit lit = k.delayed[i];
    const Lit blocking = k.delayed[i + 1];
    const Ref ref = k.delayed[i + 2];
    k.watches.push_large(lit, blocking, ref);
  }
  k.delayed.clear();
}

// returns true iff a conflict was found (recorded in the kernel)
static bool propagate_literal(Kernel &k, Lit lit) {
  const Lit not_lit = negate(lit);
  assert(k.delayed.empty());
  // hoisted data pointers (kissat proplit.h idiom): the inlined assigns
  // store through k, so without locals the compiler must re-derive the
  // vector data pointers after every assignment; the underlying arrays
  // never move during a scan (no clause allocation, values sized at
  // reserve time)
  const int8_t *const values = k.values.data();
  uint32_t *const arena = k.arena.data();
  uint32_t *const begin = k.watches.begin(not_lit);
  uint32_t *q = begin;
  const uint32_t *p = q;
  const uint32_t *const end = k.watches.end(not_lit);
  bool conflict = false;

  while (p != end) {
    const Watch head{*q++ = *p++};
    const Lit blocking = head.blocking();
    const int8_t blocking_value = values[blocking];
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
      Clause *c = clause_at(arena, ref);
      if (c->garbage()) {
        q -= 2;
        continue;
      }
      Lit *lits = c->lits;
      const Lit other = lits[0] ^ lits[1] ^ not_lit;
      const int8_t other_value = values[other];
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
        replacement_value = values[replacement];
        if (replacement_value >= 0)
          break;
      }
      if (replacement_value < 0) {
        for (r = lits + 2; r != searched; r++) {
          replacement = *r;
          replacement_value = values[replacement];
          if (replacement_value >= 0)
            break;
        }
      }
      if (replacement_value >= 0) { // move the watch (delayed: see header)
        c->searched = static_cast<uint32_t>(r - lits);
        lits[0] = other;
        lits[1] = replacement;
        *r = not_lit;
        q -= 2;
        k.delayed.push_back(replacement);
        k.delayed.push_back(other);
        k.delayed.push_back(ref);
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
  k.watches.truncate(not_lit, static_cast<uint32_t>(q - begin));
  flush_delayed(k);
  return conflict;
}

ApplyResult propagate_apply(Kernel &k) {
  if (k.inconsistent)
    return ApplyResult::Unsat;
  while (k.propagate_head < k.trail.size()) {
    const Lit lit = k.trail[k.propagate_head];
    if (propagate_literal(k, lit)) {
      // do NOT advance past a literal whose scan broke on the conflict:
      // if the backjump level keeps this literal (possible under the
      // generalized analysis level), the unscanned tail of its watch list
      // must be revisited — otherwise its implications are lost forever
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
    k.propagate_head++;
  }
  return ApplyResult::Ok;
}

TACTIX_REGISTER_RULE(Propagate, RuleTier::Micro, propagate_guard, nullptr,
                     propagate_apply);

} // namespace tactix
