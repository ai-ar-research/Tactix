// The C default strategy (examples/default_strategy) against DefaultPolicy
// (plan §5.6 acceptance, §8.4 correspondence gate): on every correspondence
// fixture both solve a fresh C session under the same configuration and must
// agree on verdict, model, certificate bytes and trajectory counters. A
// fixture whose schedule reaches a path the C port does not cover yet is
// listed below; the list is ratcheted both ways.
#include "unit_test.hpp"
#include "default_correspondence.hpp"

#define tactix tactix_c_handle
#include "tactix_default.h"
#undef tactix

#include <chrono>
#include <iostream>
#include <map>

using namespace correspondence;

namespace {

// Fixture -> the unported path its C schedule reaches.
const std::map<std::string, std::string> EXPECTED_UNPORTED = {
};

// A fixed LCG, so every build sees the same formulas.
struct Lcg {
  uint64_t state;
  uint32_t operator()() {
    state = state * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<uint32_t>(state >> 33);
  }
};

// Tseitin AND/OR/XOR gate definitions over fresh variables above `next`.
struct Gates {
  std::vector<std::vector<int>> out;
  int next;
  int gate_and(int a, int b) {
    const int g = ++next;
    out.push_back({-g, a});
    out.push_back({-g, b});
    out.push_back({g, -a, -b});
    return g;
  }
  int gate_or(int a, int b) { return -gate_and(-a, -b); }
  int gate_xor(int a, int b) {
    const int g = ++next;
    out.push_back({-g, a, b});
    out.push_back({-g, -a, -b});
    out.push_back({g, -a, b});
    out.push_back({g, a, -b});
    return g;
  }
};

// A random 3-CNF near the satisfiability threshold.
std::vector<std::vector<int>> random_3cnf(int variables, int clauses, uint64_t seed) {
  std::vector<std::vector<int>> out;
  Lcg next{seed};
  for (int c = 0; c < clauses; ++c) {
    std::vector<int> clause;
    while (clause.size() < 3) {
      const int v = static_cast<int>(next() % variables) + 1;
      bool repeated = false;
      for (int l : clause) repeated |= l == v || l == -v;
      if (!repeated) clause.push_back(next() & 1 ? v : -v);
    }
    out.push_back(clause);
  }
  return out;
}

// A gate-level miter of two ripple-carry adders: XOR/majority gates against an
// AND/OR decomposition of the same sums and carries, their outputs XOR-ed and
// disjoined (UNSAT). Gate definitions, equivalences and shared structure.
std::vector<std::vector<int>> adder_miter(int bits) {
  Gates g{{}, 2 * bits};
  const auto gate_and = [&](int a, int b) { return g.gate_and(a, b); };
  const auto gate_or = [&](int a, int b) { return g.gate_or(a, b); };
  const auto gate_xor = [&](int a, int b) { return g.gate_xor(a, b); };
  std::vector<int> first, second;
  int carry1 = 0, carry2 = 0;
  for (int i = 0; i < bits; ++i) {
    const int a = i + 1, b = bits + i + 1;
    // Encoding one: sum = a ^ b ^ c, carry = majority via XOR gates.
    const int half = gate_xor(a, b);
    first.push_back(carry1 ? gate_xor(half, carry1) : half);
    carry1 = carry1 ? gate_or(gate_and(a, b), gate_and(half, carry1)) : gate_and(a, b);
    // Encoding two: the same functions through AND/OR of literals.
    const int both = gate_and(a, b), neither = gate_and(-a, -b);
    const int differ = -gate_or(both, neither);
    second.push_back(carry2 ? gate_or(gate_and(differ, -carry2), gate_and(-differ, carry2))
                            : differ);
    carry2 = carry2 ? gate_or(both, gate_and(differ, carry2)) : both;
  }
  first.push_back(carry1);
  second.push_back(carry2);
  std::vector<int> any;
  for (size_t i = 0; i < first.size(); ++i) any.push_back(gate_xor(first[i], second[i]));
  g.out.push_back(any);
  return g.out;
}

// A commutativity miter of two array multipliers, a*b against b*a (UNSAT):
// partial-product AND gates and full-adder rows that survive preprocessing.
std::vector<std::vector<int>> multiplier_miter(int bits) {
  Gates g{{}, 2 * bits};
  const auto gate_and = [&](int a, int b) { return g.gate_and(a, b); };
  const auto gate_or = [&](int a, int b) { return g.gate_or(a, b); };
  const auto gate_xor = [&](int a, int b) { return g.gate_xor(a, b); };
  const auto multiply = [&](const std::vector<int> &x, const std::vector<int> &y) {
    std::vector<int> row(2 * bits, 0);
    for (int i = 0; i < bits; ++i) {
      int carry = 0;
      for (int j = 0; j < bits; ++j) {
        const int product = gate_and(x[j], y[i]);
        const int current = row[i + j];
        if (!current && !carry) {
          row[i + j] = product;
          continue;
        }
        const int addend = current ? current : carry;
        const int third = current && carry ? carry : 0;
        const int half = gate_xor(product, addend);
        const int half_carry = gate_and(product, addend);
        if (third) {
          row[i + j] = gate_xor(half, third);
          carry = gate_or(half_carry, gate_and(half, third));
        } else {
          row[i + j] = half;
          carry = half_carry;
        }
      }
      row[i + bits] = carry;
    }
    return row;
  };
  std::vector<int> a, b;
  for (int i = 0; i < bits; ++i) a.push_back(i + 1), b.push_back(bits + i + 1);
  const std::vector<int> first = multiply(a, b), second = multiply(b, a);
  std::vector<int> any;
  for (size_t i = 0; i < first.size(); ++i)
    if (first[i] && second[i]) any.push_back(gate_xor(first[i], second[i]));
  g.out.push_back(any);
  return g.out;
}

// A Tseitin parity system on a random 3-regular multigraph (edge variables,
// vertex charges with odd total: UNSAT), or with even total (SAT).
std::vector<std::vector<int>> tseitin_3regular(int vertices, bool odd, uint64_t seed) {
  std::vector<int> stubs;
  for (int v = 0; v < vertices; ++v)
    for (int k = 0; k < 3; ++k) stubs.push_back(v);
  Lcg next{seed};
  for (size_t i = stubs.size(); i > 1; --i)
    std::swap(stubs[i - 1], stubs[next() % i]);
  std::vector<std::vector<int32_t>> incident(vertices);
  for (size_t i = 0; i + 1 < stubs.size(); i += 2) {
    const int32_t edge = static_cast<int32_t>(i / 2 + 1);
    incident[stubs[i]].push_back(edge);
    incident[stubs[i + 1]].push_back(edge);
  }
  std::vector<std::pair<std::vector<int32_t>, bool>> rows;
  for (int v = 0; v < vertices; ++v) rows.push_back({incident[v], odd && v == 0});
  return xors(rows);
}

// A random 3-CNF with parity chains over its variables (XOR structure the
// persistent affine search recognizes beside clausal search).
std::vector<std::vector<int>> parity_3cnf(int variables, int clauses, int chains, uint64_t seed) {
  std::vector<std::vector<int>> out = random_3cnf(variables, clauses, seed);
  Lcg next{seed ^ 0x5bd1e995u};
  std::vector<std::pair<std::vector<int32_t>, bool>> rows;
  for (int c = 0; c < chains; ++c) {
    std::vector<int32_t> row;
    while (row.size() < 3) {
      const int32_t v = static_cast<int32_t>(next() % variables) + 1;
      if (std::find(row.begin(), row.end(), v) == row.end()) row.push_back(v);
    }
    rows.push_back({row, (next() & 1) != 0});
  }
  for (auto &clause : xors(rows)) out.push_back(clause);
  return out;
}

// A random 3-CNF with planted equivalence chains over its variables.
std::vector<std::vector<int>> equivalences_3cnf(int variables, int clauses, int chains,
                                               uint64_t seed) {
  std::vector<std::vector<int>> out = random_3cnf(variables, clauses, seed);
  Lcg next{seed * 0x9e3779b97f4a7c15ull + 1};
  for (int c = 0; c < chains; ++c) {
    const int a = static_cast<int>(next() % variables) + 1;
    const int b = static_cast<int>(next() % variables) + 1;
    if (a == b) continue;
    const int lb = next() & 1 ? b : -b;
    out.push_back({-a, lb});
    out.push_back({a, -lb});
  }
  return out;
}

// The shipped structured-root stages, which the command line runs.
void shipped_stages(DefaultStrategyConfig &c, Kernel &) {
  c.inprocess.structured_root_stages =
      DefaultStrategyConfig::structured_root_preprocessing_stages();
  c.inprocess.structured_root_bounded_backtracking_stages =
      DefaultStrategyConfig::bounded_backtracking_structured_root_preprocessing_stages();
}

// The correspondence fixtures: the shared set and the public-session setups
// (routine), plus the generated, profile and structured families (extended).
std::vector<Fixture> all_fixtures(bool extended) {
  std::vector<Fixture> out = correspondence::fixtures();
  if (extended) {
    const auto add = [&](std::string name, std::vector<std::vector<int>> dimacs) {
      for (unsigned profile = 1; profile <= 5; ++profile) {
        Fixture fixture;
        fixture.name = name + "-profile-" + std::to_string(profile);
        fixture.dimacs = dimacs;
        fixture.configure = [=](DefaultStrategyConfig &c, Kernel &) {
          signature_profile(c, profile);
        };
        out.push_back(std::move(fixture));
      }
    };
    add("pigeons-8-7", pigeonhole_dimacs(8, 7));
    for (uint64_t seed : {1u, 2u, 3u})
      add("random-3cnf-150-" + std::to_string(seed), random_3cnf(150, 640, seed));
    // The full default inprocessing program without startup fast elimination,
    // then the default and shipped configurations.
    const auto add_program = [&](std::string name, std::vector<std::vector<int>> dimacs) {
      Fixture fixture;
      fixture.name = name + "-program";
      fixture.dimacs = dimacs;
      fixture.configure = [](DefaultStrategyConfig &c, Kernel &) {
        c.inprocess.initial_fast_elimination = false;
      };
      out.push_back(fixture);
      fixture.name = name + "-default";
      fixture.configure = nullptr;
      out.push_back(fixture);
      fixture.name = name + "-shipped";
      fixture.configure = shipped_stages;
      out.push_back(std::move(fixture));
    };
    add_program("pigeons-6-5", pigeonhole_dimacs(6, 5));
    add_program("pigeons-8-7", pigeonhole_dimacs(8, 7));
    for (uint64_t seed : {1u, 2u, 3u, 4u})
      add_program("random-3cnf-150-" + std::to_string(seed), random_3cnf(150, 640, seed));
    add_program("random-3cnf-300-1", random_3cnf(300, 1280, 1));
    for (const char *name : {"online-config-3.cnf", "online-config-4.cnf",
                             "sweep_interrupted_direction.cnf", "sweep-retarget.cnf",
                             "pbp-substitute-terminal.cnf"})
      add_program(name, file_dimacs(fixtures_dir() + name));
    for (int bits : {4, 8, 12}) add_program("adder-miter-" + std::to_string(bits), adder_miter(bits));
    for (int bits : {4, 5, 6})
      add_program("multiplier-miter-" + std::to_string(bits), multiplier_miter(bits));
    for (uint64_t seed : {1u, 2u})
      add_program("equivalences-3cnf-" + std::to_string(seed),
                  equivalences_3cnf(200, 820, 60, seed));
    for (const char *name : {"pigeonhole-9-8.cnf", "random3sat-sat.cnf", "tseitin-40.cnf"})
      add_program(name, file_dimacs(std::string(TACTIX_EXAMPLE_CNF_DIR) + name));
    // XOR structure under the shipped configuration with structural provenance
    // bound (an "affine-" fixture), as the command line binds it: the
    // persistent affine search admits its matrix.
    const auto add_affine = [&](std::string name, std::vector<std::vector<int>> dimacs) {
      Fixture fixture;
      fixture.name = "affine-" + name;
      fixture.dimacs = std::move(dimacs);
      fixture.configure = shipped_stages;
      out.push_back(fixture);
      fixture.name += "-no-root-closure";
      fixture.configure = [](DefaultStrategyConfig &c, Kernel &) {
        c.affine_root_closure.enabled = false;
      };
      out.push_back(std::move(fixture));
    };
    for (uint64_t seed : {1u, 2u}) {
      add_affine("tseitin-3regular-odd-" + std::to_string(seed), tseitin_3regular(24, true, seed));
      add_affine("tseitin-3regular-even-" + std::to_string(seed), tseitin_3regular(24, false, seed));
    }
    for (uint64_t seed : {1u, 2u, 3u})
      add_affine("parity-3cnf-" + std::to_string(seed), parity_3cnf(120, 470, 40, seed));
    add_affine("multiplier-miter-5", multiplier_miter(5));
  }
  // Public session setup: retention advice through startup elimination, ER
  // definitions (their outputs are retained), and queued region queries run
  // before preprocessing.
  const auto add_session = [&](std::string name, std::vector<std::vector<int>> dimacs,
                               std::function<void(tactix_c_handle *)> session) {
    Fixture fixture;
    fixture.name = "session-" + name;
    fixture.dimacs = std::move(dimacs);
    fixture.session = std::move(session);
    out.push_back(std::move(fixture));
  };
  const auto retain = [](int from, int to, int step) {
    return [=](tactix_c_handle *s) {
      for (int v = from; v <= to; v += step) assert(tactix_set_variable_retention(s, v, 1) == 0);
    };
  };
  add_session("retention-adder", adder_miter(10), retain(1, 60, 3));
  add_session("retention-multiplier", multiplier_miter(5), retain(1, 40, 2));
  add_session("retention-3cnf", random_3cnf(120, 500, 7), retain(1, 120, 5));
  const auto extend = [](tactix_c_handle *s) {
    assert(tactix_apply(s, tactix_rule_id("Propagate")) != 20);
    int previous = 1;
    for (int v = 2; v <= 12; ++v) {
      const int gate = v % 3 == 0 ? TACTIX_EXTENSION_XOR : v % 3 == 1 ? TACTIX_EXTENSION_AND
                                                                       : TACTIX_EXTENSION_OR;
      const int output = tactix_define_extension(s, gate, previous, v % 2 ? v : -v);
      assert(output > 0);
      previous = output;
    }
  };
  add_session("extension-3cnf", random_3cnf(100, 420, 11), extend);
  add_session("extension-adder", adder_miter(8), extend);
  const auto regions = [](bool interrupted) {
    return [=](tactix_c_handle *s) {
      assert(tactix_regions_begin(s) == 0);
      std::vector<int> first, second;
      for (int v = 1; v <= 30; ++v) first.push_back(v);
      for (int v = 20; v <= 60; ++v) second.push_back(v);
      const int a = tactix_region_add(s, first.data(), static_cast<int>(first.size()));
      const int b = tactix_region_add(s, second.data(), static_cast<int>(second.size()));
      assert(a >= 0 && b >= 0);
      const int probe = tactix_rule_id("Probe"), sweep = tactix_rule_id("Sweep");
      for (int v = 1; v <= 8; ++v) assert(tactix_region_queue(s, probe, v % 2 ? v : -v, a, 0) == 0);
      for (int v = 21; v <= 26; ++v)
        assert(tactix_region_queue(s, sweep, v % 2 ? -v : v, b, interrupted ? 1 : 2000) == 0);
    };
  };
  std::vector<std::vector<int>> failed = random_3cnf(80, 270, 5);
  for (int v = 1; v <= 7; v += 2) failed.insert(failed.end(), {{-v, v + 1}, {-v, -(v + 1)}});
  // A program listing only some rules: the registry appends the rest under
  // the reference program's phases.
  for (const char *name : {"random-3cnf-150-1", "adder-miter-8"}) {
    Fixture fixture;
    fixture.name = std::string("unlisted-") + name;
    fixture.dimacs = !std::strcmp(name, "adder-miter-8") ? adder_miter(8) : random_3cnf(150, 640, 1);
    fixture.configure = [](DefaultStrategyConfig &c, Kernel &) {
      std::vector<strategy::InprocessStep> kept;
      for (const auto &step : c.inprocess.program)
        if (step.rule != "Vivify" && step.rule != "TransitiveReduce" && step.rule != "Backbone")
          kept.push_back(step);
      c.inprocess.program = kept;
    };
    out.push_back(std::move(fixture));
  }
  add_session("regions-3cnf", failed, regions(false));
  add_session("regions-tight-sweep", failed, regions(true));
  // Every fixture again under the command line's deferred startup lifetime.
  const size_t eager = out.size();
  for (size_t i = 0; i < eager; ++i) {
    Fixture fixture = out[i];
    fixture.name += "-deferred";
    fixture.deferred = true;
    out.push_back(std::move(fixture));
  }
  return out;
}

// DefaultPolicy as tactix_solve runs it, with the session's region program.
int solve_policy(tactix_c_handle *s, const DefaultStrategyConfig &config) {
  DefaultPolicy policy(config);
  return policy.solve(api_detail::kernel_for_fixed_testing(s), nullptr, api_detail::regions(s));
}

std::string unported;

int solve_c(tactix_c_handle *s, const DefaultStrategyConfig &config) {
  api_detail::strategy_configuration(s) = default_policy_configuration(config);
  const int verdict = tactix_default_solve(s, nullptr);
  const char *path = tactix_default_unported();
  unported = path ? path : "";
  return verdict;
}

} // namespace

int main(int argc, char **argv) {
  const bool extended = argc > 1 && !std::strcmp(argv[1], "--extended");
  const std::string scratch = scratch_dir();
  int mismatches = 0, matched = 0;
  double policy_seconds = 0, c_seconds = 0;
  const auto timed = [&](double &seconds, const Fixture &fixture, const Solver &solve) {
    const auto started = std::chrono::steady_clock::now();
    std::string line = emit(fixture, scratch, solve);
    seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    return line;
  };
  for (const Fixture &fixture : all_fixtures(extended)) {
    const std::string reference = timed(policy_seconds, fixture, solve_policy);
    const std::string actual = timed(c_seconds, fixture, solve_c);
    const auto expected = EXPECTED_UNPORTED.find(fixture.name);
    if (!unported.empty() || expected != EXPECTED_UNPORTED.end()) {
      const std::string wanted = expected == EXPECTED_UNPORTED.end() ? "" : expected->second;
      std::cout << fixture.name << ": unported '" << unported << "'"
                << (unported == wanted ? "" : " (expected '" + wanted + "')") << '\n';
      mismatches += unported != wanted;
      continue;
    }
    if (reference != actual) {
      std::cout << fixture.name << " differs:\n  policy: " << reference
                << "\n  c:      " << actual << '\n';
      ++mismatches;
    } else {
      ++matched;
      if (std::getenv("TACTIX_CORRESPONDENCE_VERBOSE")) std::cout << actual << '\n';
    }
  }
  rmdir(scratch.c_str());
  std::cout << "matched=" << matched << " unported=" << EXPECTED_UNPORTED.size()
            << " mismatches=" << mismatches << " policy-seconds=" << policy_seconds
            << " c-seconds=" << c_seconds << '\n';
  return mismatches ? 1 : 0;
}
