// Temporary Congruence clauses must disappear in every clausal backend.
#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/congruence/congruence.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
using namespace tactix;
using Row = std::vector<int>;

static Row key(Row row) {
  std::sort(row.begin(), row.end());
  return row;
}
static void xor_gate(std::vector<Row> &rows, int o, int a, int b) {
  rows.insert(rows.end(), {{-o, a, b}, {o, -a, b},
                           {o, a, -b}, {-o, -a, -b}});
}
static void and_gate(std::vector<Row> &rows, int o, int a, int b) {
  rows.insert(rows.end(), {{-o, a}, {-o, b}, {o, -a, -b}});
}
static void ite_gate(std::vector<Row> &rows, int o, int c, int t, int e) {
  rows.insert(rows.end(), {{-c, -t, o}, {-c, t, -o},
                           {c, -e, o}, {c, e, -o}});
}
int main() {
  for (unsigned shape = 0; shape < 3; ++shape)
    for (auto format : {ProofFormat::DsrAscii, ProofFormat::DratAscii}) {
      std::vector<Row> rows;
      if (shape == 0) {
        xor_gate(rows, 3, 1, 2);
        xor_gate(rows, 4, 1, 2);
      } else if (shape == 1) {
        ite_gate(rows, 4, 1, 2, 3);
        ite_gate(rows, 5, 1, 2, 3);
      } else {
        and_gate(rows, 3, 1, 2);
        and_gate(rows, 4, 1, 2);
        xor_gate(rows, 5, 3, 4);
      }
      FILE *input = std::tmpfile(), *output = std::tmpfile();
      assert(input && output);
      std::fprintf(input, "p cnf 5 %zu\n", rows.size());
      std::map<Row, int> population;
      for (const Row &row : rows) {
        ++population[key(row)];
        for (int literal : row)
          std::fprintf(input, "%d ", literal);
        std::fputs("0\n", input);
      }
      std::rewind(input);
      Kernel k;
      Proof proof;
      assert(proof.open(("/proc/self/fd/" + std::to_string(fileno(output))).c_str(), format));
      proof.bind_variable_map(&k.internal_to_external);
      k.proof = &proof;
      assert(!parse_dimacs(input, k));
      assert(congruence_apply(k) == ApplyResult::Ok);
      assert(k.stats.congruence_merges);
      if (shape == 2)
        assert(k.stats.congruence_units);
      proof.flush();
      std::rewind(output);
      std::string text;
      char buffer[4096];
      while (size_t size = std::fread(buffer, 1, sizeof buffer, output))
        text.append(buffer, size);
      std::istringstream lines(text);
      std::string line;
      unsigned retired = 0;
      while (std::getline(lines, line)) {
        const bool deletion = line.starts_with("d ");
        std::istringstream fields(deletion ? line.substr(2) : line);
        Row row;
        int literal;
        while (fields >> literal && literal)
          row.push_back(literal);
        int &copies = population[key(row)];
        copies += deletion ? -1 : 1;
        retired += deletion;
        assert(copies >= 0);
      }
      assert(retired); // proves the formerly missing backend path was reached
      for (const auto &[row, copies] : population) {
        if (!copies || row.size() <= 1)
          continue;
        bool live = row.size() == 2 &&
            kernel_binary_live(k, internalize(row[0]), internalize(row[1]));
        for (Ref ref = 0; !live && ref < k.arena.size();) {
          const Clause *clause = clause_at(k.arena, ref);
          if (!clause->garbage()) {
            Row actual;
            for (uint32_t i = 0; i < clause->size; ++i)
              actual.push_back(kernel_externalize(k, clause->lits[i]));
            live = key(actual) == row;
          }
          ref += clause_actual_words(clause);
        }
        assert(live && "proof-only ladder survived Congruence");
      }
      std::fclose(input);
      std::fclose(output);
    }
}
