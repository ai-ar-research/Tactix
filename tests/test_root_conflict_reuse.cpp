#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/propagate/propagate.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>

using namespace tactix;

// The recorded conflict has one nonroot literal and otherwise root-false
// literals. Root assignments may follow decisions on the chronological trail.
// The two final binary inputs make the resulting root implication refute F,
// providing complete DRAT/LRAT micro-proofs for an independent checker.
static void fixture(unsigned size, unsigned depth, unsigned mode, bool late,
                    bool enabled, bool explicit_target, ProofFormat format,
                    bool legacy, const std::string &stem) {
  const bool keep = !stem.empty();
  char temporary[] = "/tmp/tactix-root-reuse-XXXXXX";
  const int fd = keep ? -1 : mkstemp(temporary);
  assert(keep || fd >= 0);
  if (fd >= 0) close(fd);
  const std::string path = keep ? stem + ".proof" : temporary;
  Proof proof;
  assert(proof.open(path.c_str(), format));
  Kernel k;
  std::unique_ptr<ScopedHeuristics> hg;
  if (mode != 2) hg = std::make_unique<ScopedHeuristics>(k);
  kernel_reserve_vars(k, size + depth + 2);
  proof.bind_variable_map(&k.internal_to_external);
  k.proof = &proof;
  if (k.h) k.h->stable = mode == 1;
  k.conflict_reuse = enabled;
  k.conflict_reuse_chrono_levels = 100;
  assert(!analyze_guard(k));
  std::vector<Lit> clause;
  for (unsigned v = 1; v <= size; ++v) clause.push_back(internalize(v));
  Ref ref = INVALID_REF;
  if (size == 2) kernel_add_binary_clause(k, clause[0], clause[1], size);
  else ref = kernel_add_large_clause(k, clause.data(), size, true, 2, size);
  const Lit forced = clause.back(), side = internalize(size + depth + 1);
  kernel_add_binary_clause(k, negate(forced), side, size + 1);
  kernel_add_binary_clause(k, negate(forced), negate(side), size + 2);
  k.next_clause_id = size + 2;
  auto roots = [&] {
    // Reverse order ensures LRAT hints follow the actual trail, not clause order.
    for (unsigned v = size - 1; v; --v) {
      k.unit_ids[v - 1] = v;
      kernel_assign_unit(k, internalize(-static_cast<int>(v)));
      heuristics_deactivate_var(k, v - 1);
    }
  };
  if (!late) roots();
  for (unsigned i = 1; i < depth; ++i)
    kernel_assign_decision(k, internalize(size + i));
  if (late) roots();
  kernel_assign_decision(k, negate(forced));
  k.conflict_ref = ref;
  k.conflict_bin[0] = clause[0];
  k.conflict_bin[1] = clause[1];
  k.conflict_valid = true;
  const double increment = k.h ? k.h->scinc : 0;
  const auto reconstruction = k.reconstruction;
  const auto root_count = k.stats.root_assignments;
  const auto &vt = tactix_rules().at(tactix_rule_id("ConflictAnalysis"));
  assert(vt.guard(k) && vt.instances && vt.apply_instance);
  uint64_t offered[2] = {UINT64_MAX, UINT64_MAX};
  assert(vt.instances(k, offered, 0) == 0 && offered[0] == UINT64_MAX);
  assert(vt.instances(k, offered, 2) == (depth == 1 ? 1 : 2));
  assert(offered[0] == 0 && (depth == 1 || offered[1] == 1));
  // Preserve the native-policy transition controls independently of public
  // target selection. The dedicated reuse-target suite covers canonical and
  // arbitrary explicit API targets, including their predecessor discrepancy.
  assert((explicit_target ? vt.apply_instance(k, 0)
                          : enabled ? analyze_apply(k) : vt.apply(k)) == ApplyResult::Ok);
  if (!explicit_target && enabled && legacy)
    assert(backjump_apply(k) == ApplyResult::Ok);
  const bool reused = enabled && !legacy;
  const unsigned target = depth - 1 > 100 && reused && !explicit_target ? depth - 1 : 0;
  assert(k.level == target && k.value(forced) > 0);
  const Assigned &a = k.assigned[var_of(forced)];
  assert(a.level == 0 && a.reason == UNIT_REASON && !a.binary && !a.jumped);
  assert(k.unit_ids[var_of(forced)] == size + 3);
  assert(k.stats.root_assignments == root_count + 1);
  assert(k.stats.conflict_reused == reused);
  assert(k.stats.learned == !reused && k.stats.learned_units == !reused);
  assert(k.reconstruction == reconstruction);
  assert(!k.pending.valid && !k.conflict_valid && !analyze_guard(k));
  assert(k.analyzed.empty() && k.bump_order.empty());
  for (auto seen : k.seen) assert(!seen);
  if (k.h) {
    assert(!heuristics_queue_contains(k, var_of(forced)));
    assert(!heuristics_heap_contains(k, var_of(forced)));
    if (mode == 1) assert(reused ? k.h->scinc == increment : k.h->scinc > increment);
  }
  proof.flush();
  std::ifstream input(path);
  std::string first;
  assert(static_cast<bool>(std::getline(input, first)));
  std::ostringstream expected;
  if (format == ProofFormat::Lrat) expected << size + 3 << ' ';
  expected << size << " 0";
  if (format == ProofFormat::Lrat) {
    for (unsigned v = size - 1; v; --v) expected << ' ' << v;
    expected << ' ' << size << " 0";
  }
  assert(first == expected.str());
  if (enabled && !explicit_target && mode != 2 && format == ProofFormat::DratAscii)
    std::printf("PARITY %u %u %u %u %u %d %u %u %u %u %u %.17g %llu %llu\n",
        size, depth, mode, late, k.level, k.value(forced), a.level,
        a.reason == UNIT_REASON, a.binary, heuristics_var_active(k, var_of(forced)),
        heuristics_heap_contains(k, var_of(forced)), k.h->scinc,
        static_cast<unsigned long long>(k.stats.learned),
        static_cast<unsigned long long>(k.stats.chronological));
  kernel_check_invariants(k);
  auto result = propagate_apply(k);
  if (result == ApplyResult::Conflict) result = analyze_apply(k);
  assert(result == ApplyResult::Unsat && k.inconsistent);
  proof.flush();
  k.proof = nullptr;
  if (keep) {
    std::ofstream cnf(stem + ".cnf");
    cnf << "p cnf " << k.nvars << ' ' << size + 2 << '\n';
    for (unsigned v = 1; v < size; ++v) cnf << '-' << v << " 0\n";
    for (unsigned v = 1; v <= size; ++v) cnf << v << ' ';
    cnf << "0\n-" << size << ' ' << size + depth + 1 << " 0\n-";
    cnf << size << " -" << size + depth + 1 << " 0\n";
  } else assert(std::remove(path.c_str()) == 0);
}

int main(int argc, char **argv) {
  const bool legacy = argc > 1 && std::string(argv[1]) == "--legacy";
  const std::string directory = argc > 2 ? argv[2] : "";
  unsigned count = 0;
  for (unsigned size : {2u, 4u})
    for (unsigned depth : {1u, 7u, 103u})
      for (unsigned mode : {0u, 1u, 2u})
        for (bool late : {false, true})
          for (bool enabled : {false, true})
            for (bool explicit_target : {false, true})
              for (ProofFormat format : {ProofFormat::DratAscii, ProofFormat::Lrat}) {
                const std::string stem = directory.empty() ? "" : directory + "/" +
                    std::to_string(count) + (format == ProofFormat::Lrat ? "-lrat" : "-drat");
                fixture(size, depth, mode, late, enabled, explicit_target, format, legacy, stem);
                ++count;
              }
  assert(count == 288);
  std::printf("PASS: %u root-conflict fixtures; %s reuse transition\n", count,
              legacy ? "legacy learned-unit" : "reference zero-jump");
}
