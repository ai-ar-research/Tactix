// Lucky structured SAT constructors (Cerny, Fermat, balanced signature, lookup
// circuit, SIMON): the positive case through the Lucky instance, a shape
// rejection that leaves the trail untouched, and the parameter bounds.
#include "unit_test.hpp"
#include "dimacs_text.hpp"
#include "frontend/parse.hpp"
#include "rule_test_utils.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/lucky/opportunities.hpp"
#include "rules/propagate/propagate.hpp"

#include <sstream>

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

static const RuleVTable &lucky() {
  return tactix_rules()[tactix_rule_id("Lucky")];
}

// A recognized fixture publishes a model through Lucky instance `instance`.
static void assert_instance_sat(Kernel &k, uint64_t instance) {
  assert(apply_rule_instance(lucky(), k, instance) == ApplyResult::Sat);
  assert(k.unassigned_active == 0);
}

// An unrecognized fixture declines without touching the trail.
template <class Apply>
static void assert_declines(Kernel &k, Apply apply, const uint64_t &recognized) {
  const size_t trail = k.trail.size();
  assert(apply(k) == ApplyResult::NoOp);
  assert(recognized == 0 && k.trail.size() == trail);
}

TEST(cerny_cases) {
  {
    Kernel k = cerny_fixture();
    assert_instance_sat(k, 12);
    assert(k.stats.cerny_model_recognized == 1 && k.stats.cerny_model_states == 3 &&
           k.stats.cerny_model_length == 4 && k.stats.cerny_model_sat == 1);
  }
  Kernel k = cerny_fixture(true);
  assert_declines(k, cerny_model_apply, k.stats.cerny_model_recognized);
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

TEST(fermat_cases) {
  {
    Kernel k = fermat_fixture();
    assert_instance_sat(k, 11);
    assert(k.stats.fermat_model_recognized == 1 &&
           k.stats.fermat_model_candidates == 1 && k.stats.fermat_model_sat == 1);
  }
  {
    Kernel k = fermat_fixture(true);
    assert_declines(k, fermat_model_apply, k.stats.fermat_model_recognized);
  }
  Kernel k = fermat_fixture();
  strategy::FermatModelParameters parameters;
  parameters.maximum_bits = 9;
  strategy::ScopedRuleParameters<strategy::FermatModelParameters> posed(k, parameters);
  assert_declines(k, fermat_model_apply, k.stats.fermat_model_recognized);
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

static const strategy::LocalSearchHint &balance_hint(const Kernel &k) {
  return k.local_search_hints[static_cast<size_t>(
      strategy::LocalSearchKind::BalancedSignature)];
}

TEST(balanced_signature_cases) {
  {
    Kernel k = balance_fixture();
    assert_instance_sat(k, 10);
    assert(k.stats.balanced_signature_recognized == 1 &&
           k.stats.balanced_signature_forced_ttf == 0 &&
           k.stats.balanced_signature_candidates == 1 &&
           k.stats.balanced_signature_sat == 1);
    assert(balance_hint(k).available() && balance_hint(k).external_phases[43] == 0);
  }
  {
    Kernel k = balance_fixture(true, true);
    assert(balanced_signature_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.balanced_signature_forced_ttf == 1 &&
           k.stats.balanced_signature_candidates == 1 &&
           k.stats.balanced_signature_steps > 0 && k.unassigned_active == 0);
  }
  {
    // A third positive literal is not a TTF guard, even when it is root
    // false.  Falling back to TTO cannot meet all three fixed-row clauses
    // because TTO always selects row zero, so model completion must reject it.
    Kernel k = balance_fixture(true, true, false);
    assert(balanced_signature_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.balanced_signature_recognized == 1 &&
           k.stats.balanced_signature_forced_ttf == 0 &&
           k.stats.balanced_signature_candidates == 1 &&
           k.stats.balanced_signature_sat == 0 && k.unassigned_active != 0);
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
    assert(k.stats.balanced_signature_restarts > 0 &&
           k.stats.balanced_signature_restarts <= 2);
    assert(k.stats.balanced_signature_steps > 0 &&
           k.stats.balanced_signature_steps <= 6);
  }
  {
    Kernel k = balance_fixture(true, true);
    volatile sig_atomic_t stop = 1;
    k.stop = &stop;
    assert(balanced_signature_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.balanced_signature_interrupted == 1 &&
           k.stats.balanced_signature_steps == 0 &&
           k.stats.balanced_signature_candidates == 0 && !balance_hint(k).available());
  }
  {
    // Invalid native parameters decline before recognition.
    Kernel k = balance_fixture();
    strategy::BalancedSignatureModelParameters parameters;
    parameters.steps_per_restart = 0;
    strategy::ScopedRuleParameters<strategy::BalancedSignatureModelParameters> posed(k, parameters);
    assert(balanced_signature_model_apply(k) == ApplyResult::NoOp);
    assert(k.trail.size() == 7 && !k.stats.balanced_signature_recognized);
  }
  Kernel k = balance_fixture(false);
  assert_declines(k, balanced_signature_model_apply,
                  k.stats.balanced_signature_recognized);
  assert(!balance_hint(k).available());
}

static Kernel lookup_fixture(bool mixed_width = false) {
  std::ostringstream dimacs;
  dimacs << "p cnf 4 " << 14 + mixed_width << '\n';
  for (unsigned input = 0; input < 4; ++input)
    for (unsigned output = 0; output < 4; ++output)
      if (output != input)
        dimacs << ((input & 1) ? -1 : 1) << ' ' << ((input & 2) ? -2 : 2) << ' '
               << ((output & 1) ? -3 : 3) << ' ' << ((output & 2) ? -4 : 4)
               << " 0\n";
  dimacs << "3 0\n-4 0\n";
  if (mixed_width)
    dimacs << "1 2 0\n";
  return parse_dimacs_text(dimacs.str());
}

// One 8-input/1-output lookup gate whose output is its highest input bit.
// Fixing the output high forces enumeration beyond the first 128 input
// assignments and exercises every AVX2 table-selection level.
static Kernel wide_lookup_fixture() {
  std::ostringstream dimacs;
  dimacs << "p cnf 9 257\n";
  for (unsigned input = 0; input < 256; ++input) {
    for (unsigned bit = 0; bit < 8; ++bit)
      dimacs << ((input >> bit) & 1 ? -int(bit + 1) : int(bit + 1)) << ' ';
    dimacs << (((input >> 7) & 1) ? 9 : -9) << " 0\n";
  }
  dimacs << "9 0\n";
  return parse_dimacs_text(dimacs.str());
}

static bool avx2() {
#if defined(__x86_64__) || defined(__i386__)
  __builtin_cpu_init();
  return __builtin_cpu_supports("avx2");
#else
  return false;
#endif
}

TEST(lookup_circuit_cases) {
  {
    Kernel k = lookup_fixture();
    assert_instance_sat(k, 13);
    assert(k.stats.lookup_circuit_model_recognized == 1 &&
           k.stats.lookup_circuit_model_gates == 1 &&
           k.stats.lookup_circuit_model_support == 2);
    assert(!avx2() || k.stats.lookup_circuit_model_vector_evaluations == 1);
  }
  {
    Kernel k = lookup_fixture(true);
    assert_declines(k, lookup_circuit_model_apply,
                    k.stats.lookup_circuit_model_recognized);
  }
  {
    Kernel k = wide_lookup_fixture();
    assert_instance_sat(k, 13);
    assert(k.stats.lookup_circuit_model_support == 8);
    assert(!avx2() || k.stats.lookup_circuit_model_vector_evaluations >= 3);
    assert(k.value(internalize(8)) > 0 && k.value(internalize(9)) > 0);
  }
  Kernel k = lookup_fixture();
  strategy::LookupCircuitModelParameters parameters;
  parameters.maximum_fanin = 1;
  strategy::ScopedRuleParameters<strategy::LookupCircuitModelParameters> posed(k, parameters);
  assert(lookup_circuit_model_apply(k) == ApplyResult::NoOp);
}

static void xor_gate(std::ostringstream &out, uint32_t x, uint32_t y,
                     uint32_t z) {
  out << x << ' ' << y << " -" << z << " 0\n" << x << " -" << y << ' ' << z
      << " 0\n" << '-' << x << ' ' << y << ' ' << z << " 0\n" << '-' << x
      << " -" << y << " -" << z << " 0\n";
}

// One complete SIMON32 round under key zero, with the key's identity
// permutation into a secondary word.
static Kernel simon_fixture(bool extra_binary = false) {
  std::ostringstream dimacs;
  dimacs << "p cnf 288 " << 736 + extra_binary << '\n';
  for (uint32_t bit = 0; bit < 32; ++bit)
    dimacs << 1 + bit << " -" << 33 + bit << " 0\n-" << 1 + bit << ' '
           << 33 + bit << " 0\n";
  for (uint32_t bit = 0; bit < 32; ++bit) {
    const uint32_t a = 65 + ((bit + 31) & 31), b = 65 + ((bit + 24) & 31);
    const uint32_t conjunction = 129 + bit, first_xor = 161 + bit,
                   second_xor = 193 + bit, copy = 257 + bit, left = 65 + bit;
    dimacs << a << " -" << conjunction << " 0\n" << b << " -" << conjunction
           << " 0\n-" << a << " -" << b << ' ' << conjunction << " 0\n";
    xor_gate(dimacs, 65 + ((bit + 30) & 31), conjunction, first_xor);
    xor_gate(dimacs, 97 + bit, first_xor, second_xor);
    xor_gate(dimacs, second_xor, 1 + bit, 225 + bit);
    dimacs << copy << " -" << left << " 0\n-" << copy << ' ' << left << " 0\n";
  }
  // Zero initial state and the zero target for key zero.
  for (uint32_t variable = 65; variable <= 128; ++variable)
    dimacs << '-' << variable << " 0\n";
  for (uint32_t variable = 225; variable <= 288; ++variable)
    dimacs << '-' << variable << " 0\n";
  // Satisfied by key zero, yet breaks the binary population the shape expects.
  if (extra_binary)
    dimacs << "-1 -2 0\n";
  return parse_dimacs_text(dimacs.str());
}

TEST(simon_cases) {
  {
    Kernel k = simon_fixture();
    assert_instance_sat(k, 14);
    assert(k.stats.simon_model_recognized == 1 && k.stats.simon_model_rounds == 1 &&
           k.stats.simon_model_candidates == 1 && k.stats.simon_model_sat == 1);
  }
  {
    Kernel k = simon_fixture(true);
    assert_declines(k, simon_model_apply, k.stats.simon_model_recognized);
  }
  Kernel k = simon_fixture();
  strategy::SimonModelParameters parameters;
  parameters.maximum_rounds = 0;
  strategy::ScopedRuleParameters<strategy::SimonModelParameters> posed(k, parameters);
  assert(simon_model_apply(k) == ApplyResult::NoOp);
}
