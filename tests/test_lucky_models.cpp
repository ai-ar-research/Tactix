// Lucky structured models: Cerny, Fermat and balanced-signature recognition,
// SAT completion, shape rejection, guards and the Lucky instance surface.
#undef NDEBUG
#include "dimacs_text.hpp"
#include "frontend/parse.hpp"
#include "rule_test_utils.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/lucky/opportunities.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>

using namespace tactix;

static Kernel cerny_fixture(bool extra_positive_binary = false) {
  constexpr uint32_t states = 3, length = 4, base = states * length;
  const uint32_t clauses = 30 + extra_positive_binary;
  std::ostringstream dimacs;
  dimacs << "p cnf 20 " << clauses << '\n';
  for (uint32_t time = 0; time < length; ++time)
    dimacs << base + 2 * time + 1 << ' ' << base + 2 * time + 2
           << " 0\n";

  const uint32_t transition[states][2] = {{1, 0}, {2, 1}, {0, 0}};
  for (uint32_t letter = 0; letter < 2; ++letter) {
    bool emitted[states] = {};
    for (uint32_t source = 0; source < states; ++source) {
      const uint32_t destination = transition[source][letter];
      if (!emitted[destination]) {
        emitted[destination] = true;
        dimacs << destination + 1 << " -" << base + letter + 1 << " 0\n";
      }
    }
  }
  for (uint32_t time = 1; time < length; ++time)
    for (uint32_t source = 0; source < states; ++source)
      for (uint32_t letter = 0; letter < 2; ++letter)
        dimacs << '-' << (time - 1) * states + source + 1 << ' '
               << time * states + transition[source][letter] + 1 << " -"
               << base + 2 * time + letter + 1 << " 0\n";
  for (uint32_t first = 0; first < states; ++first)
    for (uint32_t second = first + 1; second < states; ++second)
      dimacs << '-' << (length - 1) * states + first + 1 << " -"
             << (length - 1) * states + second + 1 << " 0\n";
  if (extra_positive_binary)
    dimacs << "1 2 0\n";

  return parse_dimacs_text(dimacs.str());
}

static void cerny_cases() {
  {
    Kernel k = cerny_fixture();
    assert(k.input_positive_binary_count == 4);
    assert(k.input_ternary_clause_count == 18);
    assert(cerny_model_guard(k));
    assert(cerny_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.cerny_model_calls == 1);
    assert(k.stats.cerny_model_recognized == 1);
    assert(k.stats.cerny_model_states == 3);
    assert(k.stats.cerny_model_length == 4);
    assert(k.stats.cerny_model_sat == 1);
    assert(k.unassigned_active == 0);
  }
  {
    Kernel k = cerny_fixture(true);
    const size_t trail = k.trail.size();
    assert(cerny_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.cerny_model_recognized == 0);
    assert(k.trail.size() == trail);
  }
  {
    Kernel k = cerny_fixture();
    k.level = 1;
    assert(!cerny_model_guard(k));
  }
  {
    Kernel k = cerny_fixture();
    const RuleVTable &lucky = tactix_rules()[tactix_rule_id("Lucky")];
    assert(lucky.apply_instance(k, 12) == ApplyResult::Sat);
  }
}

static Kernel fermat_fixture(bool third_long = false) {
  std::ostringstream dimacs;
  constexpr unsigned width = 11;
  const unsigned direct_bits = 6;
  const unsigned units = 2 * width + 1;
  const unsigned clauses = 2 + 2 * direct_bits + 2 + units + third_long;
  dimacs << "p cnf 122 " << clauses << '\n';
  for (unsigned i = 1; i <= width; ++i)
    dimacs << '-' << i << ' ';
  dimacs << "0\n";
  for (unsigned i = 1; i <= width; ++i)
    dimacs << '-' << width + i << ' ';
  dimacs << "0\n";

  // source_start=82, source_end=100.  Direct equivalences mark zero bits;
  // their absence marks bits 1..3, so the recovered odd constant is 15.
  for (unsigned bit = 4; bit <= 9; ++bit) {
    const unsigned source = 82 + 2 * bit;
    const unsigned auxiliary = 102 + bit;
    dimacs << '-' << source << ' ' << auxiliary << " 0\n";
    dimacs << source << " -" << auxiliary << " 0\n";
  }
  dimacs << "-100 -101 0\n100 101 0\n";

  // 15=3*5 gives a=(3+5)/2=4 and b=(5-3)/2=1.
  for (unsigned bit = 0; bit < width; ++bit)
    dimacs << (bit == 2 ? int(bit + 1) : -int(bit + 1)) << " 0\n";
  for (unsigned bit = 0; bit < width; ++bit)
    dimacs << (bit == 0 ? int(width + bit + 1)
                        : -int(width + bit + 1))
           << " 0\n";
  dimacs << "-101 0\n";
  if (third_long) {
    for (unsigned i = 1; i <= width; ++i)
      dimacs << i << ' ';
    dimacs << "0\n";
  }

  return parse_dimacs_text(dimacs.str());
}

static void fermat_cases() {
  {
    Kernel k = fermat_fixture();
    assert(k.input_long_clause_count == 2);
    assert(k.input_fermat_long_shape && k.input_fermat_width == 11);
    assert(fermat_model_guard(k));
    assert(fermat_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.fermat_model_calls == 1);
    assert(k.stats.fermat_model_recognized == 1);
    assert(k.stats.fermat_model_candidates == 1);
    assert(k.stats.fermat_model_sat == 1);
    assert(k.unassigned_active == 0);
  }
  {
    Kernel k = fermat_fixture(true);
    const size_t trail = k.trail.size();
    assert(!k.input_fermat_long_shape);
    assert(fermat_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.fermat_model_recognized == 0);
    assert(k.trail.size() == trail);
  }
  {
    Kernel k = fermat_fixture();
    strategy::FermatModelParameters parameters;
    parameters.maximum_bits = 9;
    strategy::ScopedRuleParameters<strategy::FermatModelParameters> posed(k, parameters);
    assert(fermat_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.fermat_model_recognized == 0);
  }
  {
    Kernel k = fermat_fixture();
    const RuleVTable &lucky = tactix_rules()[tactix_rule_id("Lucky")];
    assert(lucky.apply_instance(k, 11) == ApplyResult::Sat);
  }
}

static Kernel balance_fixture(bool complete_shape = true,
                              bool forced_ttf = false,
                              bool valid_guard = true) {
  // N=3, W=2: P=(3^2-1)/2=4, pair block=18, signature boundary=42.
  Kernel k;
  // Variable 44 is deliberately outside the recognized shape and remains
  // unconstrained when the partial hint is published.
  kernel_reserve_vars(k, 44);
  for (const int unit : {-1, -5, -9, -10, -14, -18})
    kernel_assign_unit(k, internalize(unit));
  if (complete_shape)
    kernel_assign_unit(k, internalize(43));
  if (forced_ttf) {
    // Each clause requires ball b to use ternary row b+1, in either
    // orientation.  Exercise both input encodings recognized by LymphoSAT:
    // a binary pair and a ternary pair guarded by a positive input unit.
    add_external_clause(k, {valid_guard ? -43 : 44, 25, 26});
    kernel_add_binary_clause(k, internalize(33), internalize(34));
    kernel_add_binary_clause(k, internalize(41), internalize(42));
    if (!valid_guard)
      kernel_assign_unit(k, internalize(-44));
  }
  assert(propagate_service(k) == ApplyResult::Ok);
  return k;
}

static void balanced_signature_cases() {
  {
    Kernel k = balance_fixture();
    assert(balanced_signature_model_guard(k));
    const ApplyResult result = balanced_signature_model_apply(k);
    if (result != ApplyResult::Sat)
      std::fprintf(stderr,
                   "balance result=%u calls=%llu recognized=%llu candidates=%llu "
                   "restarts=%llu steps=%llu completion=%llu hints=%llu\n",
                   unsigned(result),
                   (unsigned long long)k.stats.balanced_signature_calls,
                   (unsigned long long)k.stats.balanced_signature_recognized,
                   (unsigned long long)k.stats.balanced_signature_candidates,
                   (unsigned long long)k.stats.balanced_signature_restarts,
                   (unsigned long long)k.stats.balanced_signature_steps,
                   (unsigned long long)k.stats.balanced_signature_completion_rounds,
                   (unsigned long long)k.stats.local_search_hints_offered);
    assert(result == ApplyResult::Sat);
    assert(k.stats.balanced_signature_calls == 1);
    assert(k.stats.balanced_signature_recognized == 1);
    assert(k.stats.balanced_signature_forced_ttf == 0);
    assert(k.stats.balanced_signature_candidates == 1);
    assert(k.stats.balanced_signature_sat == 1);
    assert(k.unassigned_active == 0);
    assert(k.local_search_hints[static_cast<size_t>(
               strategy::LocalSearchKind::BalancedSignature)]
               .available());
    assert(k.local_search_hints[static_cast<size_t>(
               strategy::LocalSearchKind::BalancedSignature)]
               .external_phases[43] == 0);
  }
  {
    Kernel k = balance_fixture(true, true);
    assert(balanced_signature_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.balanced_signature_recognized == 1);
    assert(k.stats.balanced_signature_forced_ttf == 1);
    assert(k.stats.balanced_signature_candidates == 1);
    assert(k.stats.balanced_signature_sat == 1);
    assert(k.stats.balanced_signature_steps > 0);
    assert(k.unassigned_active == 0);
  }
  {
    // A third positive literal is not a TTF guard, even when it is root
    // false.  Falling back to TTO cannot meet all three fixed-row clauses
    // because TTO always selects row zero, so model completion must reject it.
    Kernel k = balance_fixture(true, true, false);
    assert(balanced_signature_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.balanced_signature_recognized == 1);
    assert(k.stats.balanced_signature_forced_ttf == 0);
    assert(k.stats.balanced_signature_candidates == 1);
    assert(k.stats.balanced_signature_sat == 0);
    assert(k.unassigned_active != 0);
  }
  {
    Kernel k = balance_fixture(true, true);
    strategy::BalancedSignatureModelParameters parameters;
    parameters.restarts = 2;
    parameters.steps_per_restart = 3;
    parameters.samples_per_step = 1;
    strategy::ScopedRuleParameters<strategy::BalancedSignatureModelParameters> posed(k, parameters);
    const ApplyResult result = balanced_signature_model_apply(k);
    assert(result == ApplyResult::NoOp || result == ApplyResult::Sat);
    assert(k.stats.balanced_signature_restarts <= 2);
    assert(k.stats.balanced_signature_steps <= 6);
    assert(k.stats.balanced_signature_restarts > 0);
    assert(k.stats.balanced_signature_steps > 0);
  }
  {
    Kernel k = balance_fixture(true, true);
    volatile sig_atomic_t stop = 1;
    k.stop = &stop;
    assert(balanced_signature_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.balanced_signature_recognized == 1);
    assert(k.stats.balanced_signature_forced_ttf == 1);
    assert(k.stats.balanced_signature_interrupted == 1);
    assert(k.stats.balanced_signature_steps == 0);
    assert(k.stats.balanced_signature_candidates == 0);
    assert(!k.local_search_hints[static_cast<size_t>(
                strategy::LocalSearchKind::BalancedSignature)]
                .available());
  }
  {
    Kernel k = balance_fixture(false);
    const size_t trail = k.trail.size();
    assert(balanced_signature_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.balanced_signature_recognized == 0);
    assert(k.stats.balanced_signature_forced_ttf == 0);
    assert(k.trail.size() == trail);
    assert(!k.local_search_hints[static_cast<size_t>(
                strategy::LocalSearchKind::BalancedSignature)]
                .available());
  }
  {
    Kernel k = balance_fixture();
    k.level = 1;
    assert(!balanced_signature_model_guard(k));
    k.level = 0;
    k.large_watches_connected = false;
    assert(!balanced_signature_model_guard(k));
  }
  {
    Kernel k = balance_fixture();
    strategy::BalancedSignatureModelParameters parameters;
    parameters.steps_per_restart = 0;
    strategy::ScopedRuleParameters<strategy::BalancedSignatureModelParameters> posed(k, parameters);
    assert(balanced_signature_model_apply(k) == ApplyResult::NoOp);
    assert(k.trail.size() == 7);
  }
  {
    Kernel k = balance_fixture();
    const int lucky_id = tactix_rule_id("Lucky");
    assert(lucky_id >= 0);
    const RuleVTable &lucky = tactix_rules()[lucky_id];
    uint64_t instances[LUCKY_ACTIVE_INSTANCE_COUNT] = {};
    const uint32_t count = lucky.instances(
        k, instances, LUCKY_ACTIVE_INSTANCE_COUNT);
    assert(count == LUCKY_ACTIVE_INSTANCE_COUNT);
    assert(std::find(instances, instances + count, uint64_t{10}) !=
           instances + count);
    assert(lucky.apply_instance(k, 10) == ApplyResult::Sat);
  }
}

int main() {
  cerny_cases();
  fermat_cases();
  balanced_signature_cases();
}
