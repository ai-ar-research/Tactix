// XOR matching with cancelled inputs: a reduced XOR key is not a proof that the
// cancelled inputs disappeared, and root values do not justify erasing an
// independently cancelled pair. Every emitted DSR line, including unused
// lemmas, is replayed against an independent clause-multiset RUP oracle.
#undef NDEBUG
#include "clause_oracle.hpp"
#include "dimacs_text.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"
#include "temporary_file.hpp"
#include "test_io.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;
using Row = std::vector<int>;
using Population = std::map<Row, unsigned>;

static Row sorted(Row row) {
  std::sort(row.begin(), row.end());
  return row;
}

// The parity clauses of output = XOR(inputs).
static void gate(std::vector<Row> &rows, int output, const Row &inputs) {
  for (unsigned mask = 0; mask < (1u << inputs.size()); ++mask) {
    Row clause;
    bool parity = false;
    for (unsigned i = 0; i < inputs.size(); ++i) {
      const bool bit = mask & (1u << i);
      parity ^= bit;
      clause.push_back(bit ? -inputs[i] : inputs[i]);
    }
    clause.push_back(parity ? output : -output);
    rows.push_back(clause);
  }
}

static uint64_t pair(int a, int b) {
  Lit x = internalize(a), y = internalize(b);
  if (y < x) std::swap(x, y);
  return (static_cast<uint64_t>(x) << 32) | y;
}

struct PopulationRows {
  const Row &operator()(const Row &row) const { return row; }
  const Row &operator()(const Population::value_type &entry) const { return entry.first; }
};

// Replay the DSR text over the input multiset: deletions must name a live
// copy and every addition must be RUP.
static void audit(const std::vector<Row> &rows, const std::string &text,
                  bool require_additions) {
  Population population;
  for (const Row &row : rows) ++population[sorted(row)];
  std::istringstream lines(text);
  std::string line;
  unsigned additions = 0;
  while (std::getline(lines, line)) {
    std::istringstream fields(line);
    fields >> std::ws;
    const bool deletion = fields.peek() == 'd';
    if (deletion) fields.get();
    Row row;
    int literal;
    while (fields >> literal && literal) row.push_back(literal);
    row = sorted(row);
    if (deletion) {
      const auto live = population.find(row);
      assert(live != population.end());
      if (!--live->second) population.erase(live);
    } else {
      if (!rup_implied(population, row, PopulationRows{})) {
        std::fprintf(stderr, "non-RUP lemma: %s\n", line.c_str());
        assert(false && "XOR matching lemma lacks cancelled-input cases");
      }
      ++population[row];
      ++additions;
    }
  }
  assert(additions || !require_additions);
}

// Close the equivalence a == b (or refute it when unsat) through the canonical
// or the semantic action at the first available `wanted` orientation.
static void run(const std::vector<Row> &rows, uint32_t variables, int a, int b,
                bool unsat, const std::vector<uint64_t> &wanted_options,
                bool canonical, bool with_proof, bool require_additions) {
  const std::vector<uint64_t> solutions = models(rows, variables);
  for (const uint64_t bits : solutions)
    assert(literal_value(a, bits) == literal_value(b, bits));
  assert(unsat ? solutions.empty() : !solutions.empty());
  TemporaryFile proof_file;
  Kernel k;
  Proof proof;
  if (with_proof) {
    assert(proof.open(proof_file.path.c_str(), ProofFormat::DsrAscii));
    proof.bind_variable_map(&k.internal_to_external);
    k.proof = &proof;
  }
  const std::string cnf = dimacs_text(variables, rows);
  parse_dimacs_text(cnf, k);
  assert(congruence_guard(k));
  const auto &rule = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t actions[128];
  const unsigned count = rule.semantic_instances(k, actions, 128);
  // The extractor may orient one equivalence either way.
  const auto wanted = std::find_first_of(wanted_options.begin(), wanted_options.end(),
                                         actions, actions + count);
  assert(wanted != wanted_options.end());
  const auto result =
      canonical ? congruence_apply(k) : rule.apply_semantic_instance(k, *wanted);
  assert(result == (unsat ? ApplyResult::Unsat : ApplyResult::Ok));
  assert(!k.stats.congruence_unjustified);
  assert(k.inconsistent == unsat);
  if (!unsat) {
    assert(kernel_binary_live(k, internalize(-a), internalize(b)));
    assert(kernel_binary_live(k, internalize(a), internalize(-b)));
    kernel_check_invariants(k);
  }
  if (with_proof) {
    proof.flush();
    audit(rows, read_file(proof_file.path), require_additions);
  }
  k.proof = nullptr;
}

// Two XOR gates over 1,2 whose extra inputs cancel through equivalences.
static unsigned matching() {
  unsigned cases = 0;
  for (unsigned shape = 0; shape != 3; ++shape)
  for (unsigned signs = 0; signs != 4; ++signs)
  for (bool clash : {false, true})
  for (bool canonical : {false, true})
  for (bool with_proof : {false, true}) {
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
    run(rows, 8, a, b, clash, {pair(a, b)}, canonical, with_proof, true);
    ++cases;
  }
  return cases;
}

// One 4-input XOR whose inputs 2,3 cancel; the root value of 4 is not a
// justification for erasing that pair. Shape 1/2 adds a literal/representative clash.
static unsigned unary() {
  unsigned cases = 0;
  for (bool root : {false, true})
  for (bool opposite_pair : {false, true})
  for (bool output_sign : {false, true})
  for (unsigned shape = 0; shape < 3; ++shape)
  for (bool canonical : {false, true})
  for (bool with_proof : {false, true}) {
    std::vector<Row> rows;
    gate(rows, output_sign ? -6 : 6, {1, 2, 3, 4});
    const int bridge = opposite_pair ? -3 : 3;
    rows.push_back({-2, bridge});
    rows.push_back({2, -bridge});
    const int expected = root ^ opposite_pair ^ output_sign ? -1 : 1; // 6 == expected
    if (shape) {
      rows.push_back({-6, -expected});
      rows.push_back({6, expected});
    }
    // Every gate clause is physically unchanged at input.
    rows.push_back({root ? 4 : -4});
    const std::vector<uint64_t> wanted =
        shape == 1 ? std::vector<uint64_t>{pair(1, -1)}
                   : std::vector<uint64_t>{pair(expected, 6), pair(-expected, -6)};
    run(rows, 6, 6, expected, shape != 0, wanted, canonical, with_proof, false);
    ++cases;
  }
  return cases;
}

int main() {
  const unsigned matched = matching();
  const unsigned cancelled = unary();
  std::printf("XOR cancelled-input proofs: %u matching and %u unary cases\n", matched,
              cancelled);
}
