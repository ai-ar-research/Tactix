#include "unit_test.hpp"
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/propagate/propagate.hpp"
#include <memory>
#include <unistd.h>

enum Dispatch { Policy, Canonical, CanonicalInstance, Zero, Jump, Middle,
                Last, Overshoot, Wide, U32Max, DispatchCount };

static uint64_t requested(Dispatch d, unsigned jump, unsigned depth) {
  switch (d) {
  case Policy: case Canonical: case Zero: return 0;
  case CanonicalInstance: return INSTANCE_CANONICAL;
  case Jump: return jump;
  case Middle: return jump + 1;
  case Last: return depth - 1;
  case Overshoot: return depth + 9;
  case Wide: return UINT64_MAX - 1;
  case U32Max: return UINT32_MAX;
  default: assert(false); return 0;
  }
}

static void fixture(bool otfs, unsigned size, unsigned jump, unsigned depth,
                    unsigned mode, bool extra, bool reuse, Dispatch dispatch) {
  char path[] = "/tmp/tactix-reuse-target-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  assert(proof.open(path, ProofFormat::DsrAscii));
  Kernel k;
  std::unique_ptr<ScopedHeuristics> hg;
  if (mode != 2) hg = std::make_unique<ScopedHeuristics>(k);
  kernel_reserve_vars(k, depth + 32);
  proof.bind_variable_map(&k.internal_to_external);
  k.proof = &proof;
  if (k.h) k.h->stable = mode == 1;
  k.conflict_reuse = reuse;
  k.conflict_reuse_chrono_levels = 100;
  std::vector<std::vector<int>> clauses;
  auto add = [&](const std::vector<int> &external) {
    clauses.push_back(external);
    const uint64_t id = clauses.size();
    std::vector<Lit> lits;
    for (int v : external) lits.push_back(internalize(v));
    Ref ref = INVALID_REF;
    if (lits.size() == 1) {
      k.unit_ids[var_of(lits[0])] = id;
      kernel_assign_unit(k, lits[0]);
      heuristics_deactivate_var(k, var_of(lits[0]));
    } else if (lits.size() == 2) {
      kernel_add_binary_clause(k, lits[0], lits[1], id);
    } else {
      ref = kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
    }
    k.next_clause_id = id;
    return ref;
  };
  Ref conflict = INVALID_REF, reason = INVALID_REF;
  Lit forced;
  if (otfs) {
    assert(jump > 0 && size == 3);
    reason = add({-1, -2, 3});
    conflict = add({-1, -2, -3});
    forced = internalize(-2);
    // Together these pairs entail 1 and 2; the OTFS driver entails (-1 -2).
    add({1, 7}); add({1, -7}); add({2, 8}); add({2, -8});
  } else {
    for (unsigned v = size - 1; v; --v)
      if (v != 1 || !jump) add({-static_cast<int>(v)});
    std::vector<int> driver;
    for (unsigned v = 1; v <= size; ++v) driver.push_back(v);
    conflict = add(driver);
    forced = internalize(size);
    add({-static_cast<int>(size), 8}); add({-static_cast<int>(size), -8});
    if (jump) { add({-1, 7}); add({-1, -7}); }
  }
  assert(!analyze_guard(k));
  for (unsigned level = 1; level < depth; ++level) {
    kernel_assign_decision(k, internalize(level == jump ? (otfs ? 1 : -1)
                                                        : 16 + level));
    if (otfs) assert(propagate_apply(k) == ApplyResult::Ok);
  }
  kernel_assign_decision(k, negate(forced));
  if (otfs) {
    // Actual propagation establishes the reason's watched implied literal.
    // Raw assignment alone would violate OTFS's watched-reason invariant.
    assert(propagate_apply(k) == ApplyResult::Conflict);
    assert(k.conflict_ref == conflict && k.value(internalize(3)) > 0);
    const Clause *r = clause_at(k.arena, reason);
    assert(r->lits[0] == internalize(3) || r->lits[1] == internalize(3));
  }
  if (extra)
    for (unsigned i = 0; i != 3; ++i)
      kernel_assign_decision(k, internalize(16 + depth + i));
  k.conflict_ref = conflict;
  if (!otfs && size == 2) {
    k.conflict_bin[0] = internalize(1);
    k.conflict_bin[1] = internalize(2);
  }
  k.conflict_valid = true;
  const auto reconstruction = k.reconstruction;
  const auto &vt = tactix_rules().at(tactix_rule_id("ConflictAnalysis"));
  assert(vt.guard(k));
  std::vector<uint64_t> offered(k.level + 1, UINT64_MAX);
  assert(vt.instances(k, offered.data(), 0) == 0 && offered[0] == UINT64_MAX);
  assert(vt.instances(k, offered.data(), offered.size()) == k.level);
  for (unsigned i = 0; i < k.level; ++i) assert(offered[i] == i);
  assert(offered.back() == UINT64_MAX);
  const auto inst = requested(dispatch, jump, depth);
  const ApplyResult result = dispatch == Policy ? analyze_apply(k)
      : dispatch == Canonical ? vt.apply(k) : vt.apply_instance(k, inst);
  assert(result == ApplyResult::Ok);
  const bool policy_target = dispatch == Policy;
  const unsigned wanted = policy_target
      ? (depth - 1 - jump > 100 ? depth - 1 : jump)
      : dispatch == Canonical || inst == INSTANCE_CANONICAL ? jump
      : static_cast<unsigned>(std::clamp<uint64_t>(inst, jump, depth - 1));
  assert(k.level == wanted && k.value(forced) > 0);
  const Assigned &a = k.assigned[var_of(forced)];
  assert(a.level == jump && !k.pending.valid && !k.conflict_valid);
  assert(k.stats.conflict_reused == 1);
  assert(k.stats.chronological == (policy_target && depth - 1 - jump > 100));
  if (otfs) assert(k.stats.otfs_strengthened == 1);
  else assert(k.stats.learned == 0);
  if (!jump) {
    assert(a.reason == UNIT_REASON && !a.binary && !a.jumped);
    assert(k.unit_ids[var_of(forced)] > clauses.size());
    if (k.h) {
      assert(!heuristics_queue_contains(k, var_of(forced)));
      assert(!heuristics_heap_contains(k, var_of(forced)));
    }
  }
  for (unsigned level = 1; level < depth; ++level) {
    if (level == jump) continue;
    const auto value = k.value(internalize(16 + level));
    assert(value == (level <= wanted ? 1 : 0));
  }
  if (extra)
    for (unsigned i = 0; i != 3; ++i)
      assert(!k.value(internalize(16 + depth + i)));
  assert(k.reconstruction == reconstruction);
  assert(k.analyzed.empty() && k.bump_order.empty() && k.otfs_retired.empty());
  for (auto seen : k.seen) assert(!seen);
  kernel_check_invariants(k);
  // These are fixed contradictory formulas, not arbitrary generated solves.
  // Drain a bounded fixed sequence, deciding only the gadget's variables when
  // needed. The bound is a test timeout guard, not a solver-policy budget.
  ApplyResult finish = ApplyResult::Ok;
  for (unsigned round = 0; round < 128 && finish != ApplyResult::Unsat; ++round) {
    finish = propagate_apply(k);
    if (finish == ApplyResult::Conflict) finish = vt.apply(k);
    else if (finish == ApplyResult::Ok) {
      bool decided = false;
      for (int v : {1, 2, 3, 4, 7, 8, 9, 10}) {
        const Lit lit = internalize(v);
        if (!k.value(lit)) {
          kernel_assign_decision(k, lit);
          decided = true;
          break;
        }
      }
      assert(decided);
    }
  }
  assert(finish == ApplyResult::Unsat && k.inconsistent);
  assert(k.reconstruction == reconstruction);
  proof.flush();
  k.proof = nullptr;
  assert(std::remove(path) == 0);
}

int main() {
  unsigned count = 0;
  for (bool otfs : {false, true})
    for (unsigned size : {2u, 3u, 4u}) {
      if ((size == 3) != otfs) continue;
      for (unsigned jump : {0u, 1u, 3u}) {
        if (otfs && !jump) continue;
        for (unsigned depth : {7u, 103u})
          for (unsigned mode : {0u, 1u, 2u})
            for (bool extra : {false, true})
              for (bool reuse : {false, true}) {
                if (!reuse && !otfs) continue;
                for (unsigned d = 0; d < DispatchCount; ++d, ++count)
                  fixture(otfs, size, jump, depth, mode, extra, reuse,
                          static_cast<Dispatch>(d));
              }
      }
    }
  assert(count == 1200);
  std::printf("PASS: %u reuse target fixtures\n", count);
}
