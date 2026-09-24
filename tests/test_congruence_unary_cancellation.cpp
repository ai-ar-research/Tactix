// Root values do not justify erasing an independently cancelled XOR pair.
#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
using namespace tactix;
using Row = std::vector<int>;
using Values = std::array<int, 7>;
static Row sorted(Row row) { std::sort(row.begin(), row.end()); return row; }
static bool assign(Values &values, int lit) {
  int &v = values[std::abs(lit)];
  const int sign = lit > 0 ? 1 : -1;
  if (v == -sign) return false;
  v = sign;
  return true;
}
// -1 satisfied; 0 conflict; otherwise the number of unassigned literals.
static int reduce(const Row &row, Values &values, int &unit) {
  int open = 0;
  for (int lit : row) {
    const int v = values[std::abs(lit)] * (lit > 0 ? 1 : -1);
    if (v > 0) return -1;
    if (!v) { ++open; unit = lit; }
  }
  return open;
}
static bool rup(const std::map<Row, unsigned> &db, const Row &lemma) {
  Values values{};
  for (int lit : lemma) if (!assign(values, -lit)) return true;
  bool changed;
  do {
    changed = false;
    for (const auto &[row, copies] : db) {
      if (!copies) continue;
      int unit = 0;
      const int open = reduce(row, values, unit);
      if (!open) return true;
      if (open == 1) { assert(assign(values, unit)); changed = true; }
    }
  } while (changed);
  return false;
}
static bool lrat(const std::map<uint64_t, Row> &ids, const Row &lemma,
                 const std::vector<uint64_t> &chain) {
  Values values{};
  for (int lit : lemma) if (!assign(values, -lit)) return true;
  for (const uint64_t id : chain) {
    assert(ids.count(id));
    int unit = 0;
    const int open = reduce(ids.at(id), values, unit);
    if (!open) return true;
    if (open != 1) return false;
    assert(assign(values, unit));
  }
  return false;
}
static uint64_t pair(int a, int b) {
  Lit x = internalize(a), y = internalize(b);
  if (y < x) std::swap(x, y);
  return (static_cast<uint64_t>(x) << 32) | y;
}

int main(int argc, char **argv) {
  assert(argc == 1 || argc == 2); // optional checker artifact directory
  unsigned cases = 0;
  for (bool root : {false, true})
  for (bool opposite_pair : {false, true})
  for (bool output_sign : {false, true})
  for (unsigned shape = 0; shape < 3; ++shape) // equivalence / literal / rep clash
  for (bool canonical : {false, true})
  for (unsigned mode = 0; mode < 4; ++mode) {
    std::vector<Row> rows;
    const int output = output_sign ? -6 : 6;
    for (unsigned mask = 0; mask < 16; ++mask) {
      Row clause;
      bool parity = false;
      for (unsigned i = 0; i < 4; ++i) {
        const bool bit = mask & (1u << i);
        parity ^= bit;
        clause.push_back(bit ? -int(i + 1) : int(i + 1));
      }
      clause.push_back(parity ? output : -output);
      rows.push_back(clause);
    }
    const int bridge = opposite_pair ? -3 : 3;
    rows.push_back({-2, bridge}); rows.push_back({2, -bridge});
    const bool negated_equivalence = root ^ opposite_pair ^ output_sign;
    const int expected = negated_equivalence ? -1 : 1; // 6 == expected
    if (shape) {
      rows.push_back({-6, -expected}); rows.push_back({6, expected});
    }
    // Keep all gate clauses physically unchanged at input. Late attachment is
    // a separate API obligation; this regression attaches before parsing.
    rows.push_back({root ? 4 : -4});
    unsigned models = 0;
    for (unsigned bits = 0; bits < 64; ++bits) {
      const auto value = [&](int lit) {
        return bool(bits & (1u << (std::abs(lit) - 1))) == (lit > 0);
      };
      bool model = true;
      for (const Row &row : rows) {
        bool satisfied = false;
        for (int lit : row) satisfied |= value(lit);
        model &= satisfied;
      }
      if (model) { ++models; assert(value(6) == value(expected)); }
    }
    assert(shape ? models == 0 : models > 0);
    FILE *input = std::tmpfile(), *sink = std::tmpfile();
    assert(input && sink);
    std::fprintf(input, "p cnf 6 %zu\n", rows.size());
    std::map<Row, unsigned> population;
    std::map<uint64_t, Row> ids;
    for (const Row &row : rows) {
      ++population[sorted(row)]; ids[ids.size() + 1] = sorted(row);
      for (int lit : row) std::fprintf(input, "%d ", lit);
      std::fputs("0\n", input);
    }
    std::rewind(input);
    Kernel k;
    Proof proof;
    if (mode) {
      const auto format = mode == 1 ? ProofFormat::DsrAscii :
                          mode == 2 ? ProofFormat::DratAscii : ProofFormat::Lrat;
      assert(proof.open(("/proc/self/fd/" + std::to_string(fileno(sink))).c_str(), format));
      proof.bind_variable_map(&k.internal_to_external); k.proof = &proof;
    }
    assert(!parse_dimacs(input, k));
    assert(congruence_guard(k));
    const auto &rule = tactix_rules()[tactix_rule_id("Congruence")];
    uint64_t wanted = shape == 1 ? pair(1, -1) : pair(expected, 6);
    uint64_t actions[128];
    const unsigned count = rule.semantic_instances(k, actions, 128);
    // The extractor may orient the same equivalence as -a == -b. Require
    // exactly this semantic result, with either signed orientation.
    if (shape != 1 && std::find(actions, actions + count, wanted) == actions + count)
      wanted = pair(-expected, -6);
    if (std::find(actions, actions + count, wanted) == actions + count) {
      std::fprintf(stderr, "missing action root=%d opposite=%d output=%d shape=%u canonical=%d mode=%u wanted=%llu:",
                   root, opposite_pair, output_sign, shape, canonical, mode,
                   static_cast<unsigned long long>(wanted));
      for (unsigned i = 0; i < count; ++i)
        std::fprintf(stderr, " %llu", static_cast<unsigned long long>(actions[i]));
      std::fputc('\n', stderr);
      assert(false);
    }
    const auto result = canonical ? congruence_apply(k) : rule.apply_semantic_instance(k, wanted);
    assert(result == (shape ? ApplyResult::Unsat : ApplyResult::Ok));
    assert(!k.stats.congruence_unjustified);
    if (!shape) {
      assert(kernel_binary_live(k, internalize(-6), internalize(expected)));
      assert(kernel_binary_live(k, internalize(6), internalize(-expected)));
      kernel_check_invariants(k);
    }
    if (mode) {
      proof.flush(); std::rewind(sink);
      std::string text; char buffer[4096];
      while (size_t n = std::fread(buffer, 1, sizeof buffer, sink)) text.append(buffer, n);
      if (argc == 2 && shape) {
        const std::string stem = std::string(argv[1]) + "/case-" + std::to_string(cases);
        std::ofstream cnf(stem + ".cnf"), out(stem + (mode == 3 ? ".lrat" : ".dsr"));
        cnf << "p cnf 6 " << rows.size() << '\n';
        for (const Row &row : rows) {
          for (int lit : row) cnf << lit << ' ';
          cnf << "0\n";
        }
        out << text;
      }
      std::istringstream lines(text); std::string line;
      while (std::getline(lines, line)) {
        std::istringstream fields(line); uint64_t id = 0;
        if (mode == 3) fields >> id;
        fields >> std::ws;
        const bool deletion = fields.peek() == 'd';
        if (deletion) fields.get();
        if (mode == 3 && deletion) {
          while (fields >> id && id) {
            assert(ids.count(id) && population[ids[id]]);
            --population[ids[id]]; ids.erase(id);
          }
          continue;
        }
        Row row; int lit;
        while (fields >> lit && lit) row.push_back(lit);
        row = sorted(row);
        if (deletion) { assert(population[row]); --population[row]; }
        else {
          if (!rup(population, row)) {
            std::fprintf(stderr, "non-RUP root=%d opposite=%d output=%d shape=%u canonical=%d mode=%u: %s\n",
                         root, opposite_pair, output_sign, shape, canonical, mode, line.c_str());
            assert(false);
          }
          if (mode == 3) {
            std::vector<uint64_t> chain; uint64_t hint;
            while (fields >> hint && hint) chain.push_back(hint);
            if (!lrat(ids, row, chain)) {
              std::fprintf(stderr, "invalid LRAT root=%d opposite=%d output=%d shape=%u canonical=%d: %s\n",
                           root, opposite_pair, output_sign, shape, canonical, line.c_str());
              std::fputs(text.c_str(), stderr);
              assert(false);
            }
            ids[id] = row;
          }
          ++population[row];
        }
      }
    }
    k.proof = nullptr; std::fclose(input); std::fclose(sink); ++cases;
  }
  std::printf("Unary XOR cancellation: %u fixed cases, every RUP/deletion/LRAT hint checked\n", cases);
}
