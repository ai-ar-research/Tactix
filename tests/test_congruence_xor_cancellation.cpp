// A reduced XOR key is not itself a proof that cancelled inputs disappeared.
// Check every emitted RUP step, including unused lemmas, against a tiny
// independent clause-multiset oracle (not the kernel's propagation routine).
#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"
#include <algorithm>
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

static Row sorted(Row row) {
  std::sort(row.begin(), row.end());
  return row;
}
static void gate(std::vector<Row> &rows, int output, Row inputs) {
  for (unsigned mask = 0; mask < (1u << inputs.size()); ++mask) {
    Row clause;
    bool parity = false;
    for (unsigned i = 0; i < inputs.size(); ++i) {
      bool bit = mask & (1u << i);
      parity ^= bit;
      clause.push_back(bit ? -inputs[i] : inputs[i]);
    }
    clause.push_back(parity ? output : -output);
    rows.push_back(clause);
  }
}
static bool rup(const std::map<Row, unsigned> &db, const Row &clause) {
  int values[9] = {};
  const auto assign = [&](int literal) {
    int &value = values[std::abs(literal)];
    int sign = literal > 0 ? 1 : -1;
    if (value == -sign) return false;
    value = sign;
    return true;
  };
  for (int literal : clause)
    if (!assign(-literal)) return true;
  bool changed;
  do {
    changed = false;
    for (const auto &[row, count] : db) {
      if (!count) continue;
      bool satisfied = false;
      unsigned unassigned = 0;
      int unit = 0;
      for (int literal : row) {
        int v = values[std::abs(literal)] * (literal > 0 ? 1 : -1);
        satisfied |= v > 0;
        if (!v) { ++unassigned; unit = literal; }
      }
      if (satisfied) continue;
      if (!unassigned) return true;
      if (unassigned == 1) {
        if (!assign(unit)) return true;
        changed = true;
      }
    }
  } while (changed);
  return false;
}

int main(int argc, char **argv) {
  assert(argc == 1 || argc == 2); // optional external-checker artifact directory
  unsigned cases = 0;
  for (unsigned shape = 0; shape != 3; ++shape)
  for (unsigned signs = 0; signs != 4; ++signs)
  for (bool clash : {false, true})
  for (bool canonical : {false, true})
  for (unsigned mode = 0; mode != 4; ++mode) {
    std::vector<Row> rows;
    const bool first_cancel = shape != 1, second_cancel = shape != 0;
    const int first_sign = first_cancel && (signs & 1) ? -1 : 1;
    const int second_sign = second_cancel && (signs & 2) ? -1 : 1;
    const int a = 7, b = clash ? -7 : 8;
    gate(rows, a * first_sign, first_cancel ? Row{1, 2, 3, 4} : Row{1, 2});
    gate(rows, b * second_sign, second_cancel ? Row{1, 2, 5, 6} : Row{1, 2});
    if (first_cancel) {
      rows.push_back({-3, first_sign * 4});
      rows.push_back({3, -first_sign * 4});
    }
    if (second_cancel) {
      rows.push_back({-5, second_sign * 6});
      rows.push_back({5, -second_sign * 6});
    }
    unsigned models = 0;
    for (unsigned bits = 0; bits != 256; ++bits) {
      const auto value = [&](int lit) {
        return bool(bits & (1u << (std::abs(lit) - 1))) == (lit > 0);
      };
      bool model = true;
      for (const Row &row : rows) {
        bool satisfied = false;
        for (int lit : row) satisfied |= value(lit);
        model &= satisfied;
      }
      if (model) { ++models; assert(value(a) == value(b)); }
    }
    assert(clash ? models == 0 : models > 0);
    FILE *input = std::tmpfile(), *output = std::tmpfile();
    assert(input && output);
    std::fprintf(input, "p cnf 8 %zu\n", rows.size());
    std::map<Row, unsigned> population;
    std::map<uint64_t, Row> ids;
    for (const Row &row : rows) {
      ++population[sorted(row)];
      ids[ids.size() + 1] = sorted(row);
      for (int lit : row) std::fprintf(input, "%d ", lit);
      std::fputs("0\n", input);
    }
    std::rewind(input);
    Kernel k;
    Proof proof;
    if (mode) {
      const auto format = mode == 1 ? ProofFormat::DsrAscii :
                          mode == 2 ? ProofFormat::DratAscii : ProofFormat::Lrat;
      assert(proof.open(("/proc/self/fd/" + std::to_string(fileno(output))).c_str(), format));
      proof.bind_variable_map(&k.internal_to_external);
      k.proof = &proof;
    }
    assert(!parse_dimacs(input, k));
    const auto &rule = tactix_rules()[tactix_rule_id("Congruence")];
    assert(congruence_guard(k));
    Lit ia = internalize(a), ib = internalize(b);
    if (ib < ia) std::swap(ia, ib);
    const uint64_t wanted = (static_cast<uint64_t>(ia) << 32) | ib;
    uint64_t actions[128];
    const unsigned count = rule.semantic_instances(k, actions, 128);
    assert(std::find(actions, actions + count, wanted) != actions + count);
    const auto result = canonical ? congruence_apply(k) :
                                   rule.apply_semantic_instance(k, wanted);
    assert(result == (clash ? ApplyResult::Unsat : ApplyResult::Ok));
    assert(!k.stats.congruence_unjustified);
    assert(k.inconsistent == clash);
    if (!clash) {
      assert(kernel_binary_live(k, internalize(-a), internalize(b)));
      assert(kernel_binary_live(k, internalize(a), internalize(-b)));
      kernel_check_invariants(k);
    }
    if (mode) {
      proof.flush();
      std::rewind(output);
      std::string text;
      char buffer[4096];
      while (size_t n = std::fread(buffer, 1, sizeof buffer, output))
        text.append(buffer, n);
      if (argc == 2 && clash) {
        const std::string stem = std::string(argv[1]) + "/case-" + std::to_string(cases);
        std::ofstream cnf(stem + ".cnf"), certificate(stem + (mode == 3 ? ".lrat" : ".dsr"));
        assert(cnf && certificate);
        cnf << "p cnf 8 " << rows.size() << '\n';
        for (const Row &row : rows) {
          for (int lit : row) cnf << lit << ' ';
          cnf << "0\n";
        }
        certificate << text;
      }
      std::istringstream lines(text);
      std::string line;
      unsigned additions = 0;
      while (std::getline(lines, line)) {
        std::istringstream fields(line);
        uint64_t id = 0;
        if (mode == 3) fields >> id;
        fields >> std::ws;
        const bool deletion = fields.peek() == 'd';
        if (deletion) fields.get();
        if (mode == 3 && deletion) {
          while (fields >> id && id) {
            assert(ids.count(id));
            assert(population[ids[id]]);
            --population[ids[id]];
            ids.erase(id);
          }
          continue;
        }
        Row row;
        int literal;
        while (fields >> literal && literal) row.push_back(literal);
        row = sorted(row);
        if (deletion) {
          assert(population[row]);
          --population[row];
        } else {
          if (!rup(population, row)) {
            std::fprintf(stderr, "non-RUP shape=%u signs=%u clash=%d canonical=%d mode=%u: %s\n",
                         shape, signs, clash, canonical, mode, line.c_str());
            assert(false && "XOR matching lemma lacks cancelled-input cases");
          }
          ++population[row];
          if (mode == 3) ids[id] = row;
          ++additions;
        }
      }
      assert(additions);
    }
    k.proof = nullptr;
    std::fclose(input);
    std::fclose(output);
    ++cases;
  }
  std::printf("XOR cancelled-input proofs: %u fixed cases\n", cases);
}
