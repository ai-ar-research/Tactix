#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/lucky/stconn_proof.hpp"

#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;

struct Generator {
  int width = 3, columns = 3;
  std::vector<std::vector<int>> clauses;
  std::vector<int> clause;
  bool satisfied = false;

  void canonical(int i, int j, int direction, int &left, int &down,
                 int &right, int &up) const {
    if (direction == 0)
      left = right = i, down = j, up = j + 1;
    else if (direction == 1)
      left = i, right = i + 1, down = up = j;
    else if (direction == 2)
      left = right = i, down = j - 1, up = j;
    else
      left = i - 1, right = i, down = up = j;
  }

  int forced(int color, int i, int j, int direction) const {
    int left, down, right, up;
    canonical(i, j, direction, left, down, right, up);
    direction &= 1;
    if (color == 1) {
      if ((left == 0 && down == 0) ||
          (right == columns - 1 && up == width - 1))
        return -1;
      if (((left == -1 && down == width - 1) ||
           (left == columns - 1 && down == 0)) &&
          direction == 1)
        return 1;
    } else {
      if ((left == 0 && up == width - 1) ||
          (right == columns - 1 && down == 0))
        return -1;
      if (((left == -1 && down == 0) ||
           (left == columns - 1 && down == width - 1)) &&
          direction == 1)
        return 1;
    }
    if (left == -1 || right == columns || down == -1 || up == width)
      return -1;
    return 0;
  }

  int variable(int color, int i, int j, int direction) const {
    int left, down, right, up;
    canonical(i, j, direction, left, down, right, up);
    direction &= 1;
    int displacement = 0;
    if (color == 0) {
      displacement = 2 * width * columns - width - columns - 4;
      down = width - 1 - up;
    }
    const int index = direction
                          ? down * (columns - 1) + left
                          : down * columns + left +
                                (width * columns - width - 2);
    return index + displacement;
  }

  void add(int color, int i, int j, int direction, bool positive) {
    const int value = forced(color, i, j, direction);
    if (value) {
      if ((value == 1) == positive)
        satisfied = true;
    } else {
      const int v = variable(color, i, j, direction);
      clause.push_back(positive ? v : -v);
    }
  }

  void finish() {
    if (!satisfied)
      clauses.push_back(clause);
    clause.clear();
    satisfied = false;
  }

  void build() {
    for (int i = 0; i < columns; ++i)
      for (int j = 0; j < width; ++j)
        for (int color = 0; color < 2; ++color)
          for (int direction = 0; direction < 4; ++direction) {
            for (int other = 0; other < 4; ++other)
              if (other != direction)
                add(color, i, j, other, false);
            finish();
            for (int other = 0; other < 4; ++other)
              add(color, i, j, other, direction != other);
            finish();
          }
    for (int i = 0; i < columns; ++i)
      for (int j = 0; j < width; ++j) {
        for (int direction = 0; direction < 2; ++direction) {
          add(0, i, j, direction, false);
          add(1, i, j, direction, false);
          finish();
        }
        for (int first = 0; first < 4; ++first)
          for (int second = 0; second < 4; ++second)
            if (first != second) {
              add(0, i, j, first, false);
              add(1, i, j, second, false);
              finish();
            }
      }
  }
};

static Kernel fixture(bool corrupt = false) {
  Generator generator;
  generator.build();
  if (corrupt)
    generator.clauses.back().push_back(1);
  const int variables =
      2 * (2 * generator.width * generator.columns - generator.width -
           generator.columns - 4);
  std::ostringstream dimacs;
  dimacs << "p cnf " << variables << ' ' << generator.clauses.size() << '\n';
  for (const auto &clause : generator.clauses) {
    for (const int literal : clause)
      dimacs << literal << ' ';
    dimacs << "0\n";
  }
  FILE *input = std::tmpfile();
  assert(input);
  const std::string text = dimacs.str();
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel k;
  assert(parse_dimacs(input, k) == nullptr);
  std::fclose(input);
  return k;
}

int main() {
  {
    Kernel k = fixture();
    assert(!k.input_unit_clause_count);
    assert(stconn_proof_guard(k));
    assert(stconn_proof_apply(k) == ApplyResult::Unsat);
    assert(k.inconsistent);
    assert(k.stats.stconn_proof_calls == 1);
    assert(k.stats.stconn_proof_recognized == 1);
    assert(k.stats.stconn_proof_width == 3);
    assert(k.stats.stconn_proof_columns == 3);
    assert(k.stats.stconn_proof_rup_clauses > 1);
    assert(k.stats.stconn_proof_unsat == 1);
  }
  {
    Kernel k = fixture(true);
    assert(stconn_proof_apply(k) == ApplyResult::NoOp);
    assert(!k.inconsistent && !k.stats.stconn_proof_recognized);
  }
  {
    Kernel k = fixture();
    strategy::StconnProofParameters parameters;
    parameters.maximum_rup_clauses = 1;
    strategy::ScopedStconnProofParameters posed(k, parameters);
    assert(stconn_proof_apply(k) == ApplyResult::NoOp);
    assert(!k.inconsistent && k.stats.stconn_proof_budget_exhausted == 1);
  }
  {
    Kernel k = fixture();
    const RuleVTable &lucky = tactix_rules()[tactix_rule_id("Lucky")];
    assert(lucky.apply_instance(k, 15) == ApplyResult::Unsat);
  }
  return 0;
}
