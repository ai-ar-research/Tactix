// Witness-free expansion of permutation-witness SR additions
// (docs/calculus.md, SR witness-free expansion). External checker acceptance
// of the expanded text is in tests/micro_proofs.sh; this test pins scope,
// budget, the stream-level route, and solver-state identity with native lines.
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

using namespace tactix;

namespace {

// (1 2) is an automorphism of the four-clause core; padding on 5.. is fixed.
std::string formula(uint32_t padding) {
  std::ostringstream text;
  text << "p cnf " << 4 + padding + 1 << ' ' << 4 + padding << "\n"
       << "1 2 0\n-1 3 0\n-2 3 0\n-3 4 0\n";
  for (uint32_t i = 0; i < padding; ++i)
    text << 5 + i << ' ' << -int(6 + i) << " 0\n";
  return text.str();
}

Lit lit(int external) { return internalize(external); }

struct Run {
  Kernel k;
  Proof proof;
  std::string proof_path;
  std::vector<std::vector<Lit>> clauses() const {
    std::vector<std::vector<Lit>> result;
    for (Lit l = 0; l < 2 * k.nvars; ++l)
      for (const WatchEntry watch : k.watches.entries(l))
        if (watch.binary() && l < watch.other())
          result.push_back({l, watch.other()});
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *clause = clause_at(k.arena, ref);
      if (!clause->garbage()) {
        std::vector<Lit> lits(clause->begin(), clause->end());
        std::sort(lits.begin(), lits.end());
        result.push_back(lits);
      }
      ref += clause_actual_words(clause);
    }
    std::sort(result.begin(), result.end());
    return result;
  }
};

void open(Run &run, const std::string &cnf_text, bool native) {
  char cnf_path[] = "/tmp/tactix-sr-expansion-cnf-XXXXXX";
  const int fd = mkstemp(cnf_path);
  assert(fd >= 0);
  close(fd);
  { std::ofstream(cnf_path) << cnf_text; }
  char proof_path[] = "/tmp/tactix-sr-expansion-proof-XXXXXX";
  const int pfd = mkstemp(proof_path);
  assert(pfd >= 0);
  close(pfd);
  run.proof_path = proof_path;
  if (native)
    setenv("TACTIX_SR_NATIVE_WITNESS", "1", 1);
  else
    unsetenv("TACTIX_SR_NATIVE_WITNESS");
  assert(run.proof.open(proof_path, ProofFormat::DsrAscii));
  unsetenv("TACTIX_SR_NATIVE_WITNESS");
  run.k.proof = &run.proof;
  run.proof.bind_variable_map(&run.k.internal_to_external);
  FILE *cnf = fopen(cnf_path, "r");
  assert(cnf && !parse_dimacs(cnf, run.k));
  fclose(cnf);
  std::remove(cnf_path);
  for (Var v = 0; v < run.k.nvars; ++v)
    assert(run.k.internal_to_external[v] == v);
}

SubstitutionRedundancyApplicationStatus apply(Run &run, std::vector<Lit> clause,
    std::vector<std::pair<Lit, Lit>> substitution,
    std::vector<Lit> witness = {}) {
  SubstitutionRedundancyInstance instance;
  instance.clause = std::move(clause);
  instance.witness = std::move(witness);
  instance.substitution = std::move(substitution);
  SubstitutionRedundancyParameters parameters;
  return substitution_redundancy_apply_detailed(run.k, instance, parameters)
      .status;
}

// A witnessed DSR line repeats its first literal (the pivot) as the witness
// separator; witness-free lines never repeat a literal.
uint64_t witnessed_lines(Run &run) {
  run.proof.flush();
  std::ifstream stream(run.proof_path);
  std::string text;
  uint64_t witnessed = 0;
  while (std::getline(stream, text)) {
    std::istringstream line(text);
    std::vector<std::string> tokens;
    for (std::string token; line >> token;)
      tokens.push_back(token);
    if (tokens.empty() || tokens[0] == "d")
      continue;
    witnessed += std::count(tokens.begin() + 1, tokens.end(), tokens[0]) > 0;
  }
  return witnessed;
}

void expansion_matches_native_state() {
  Run expanded, native;
  open(expanded, formula(40), false);
  open(native, formula(40), true);
  const std::vector<Lit> clause = {lit(-1), lit(2)};
  const std::vector<std::pair<Lit, Lit>> swap = {{lit(1), lit(2)},
                                                 {lit(2), lit(1)}};
  assert(apply(expanded, clause, swap) ==
         SubstitutionRedundancyApplicationStatus::Applied);
  assert(apply(native, clause, swap) ==
         SubstitutionRedundancyApplicationStatus::Applied);
  assert(expanded.k.stats.sr_expanded == 1 &&
         !expanded.k.stats.sr_expansion_native);
  // T = 3 touched clauses and m = 2 renamed variables.
  assert(expanded.k.stats.sr_expansion_additions ==
         Proof::sr_expansion_additions(3, 2, 2, false));
  assert(!native.k.stats.sr_expanded && native.k.stats.sr_expansion_native == 1);
  assert(!witnessed_lines(expanded) &&
         std::string(expanded.proof.verification_route()) == "dsr-cake_lpr");
  assert(witnessed_lines(native) == 1 &&
         std::string(native.proof.verification_route()) == "dsr-trestle");
  assert(expanded.clauses() == native.clauses());
  assert(expanded.k.next_clause_id == native.k.next_clause_id);
  for (Var v = 0; v < expanded.k.nvars; ++v)
    assert(expanded.k.value(make_lit(v, false)) ==
           native.k.value(make_lit(v, false)));
  std::remove(expanded.proof_path.c_str());
  std::remove(native.proof_path.c_str());
}

void over_budget_and_witnessed_streams_stay_native() {
  // Without padding the expansion (27 additions) exceeds the population (4).
  Run run;
  open(run, formula(0), false);
  assert(apply(run, {lit(-1), lit(2)}, {{lit(1), lit(2)}, {lit(2), lit(1)}}) ==
         SubstitutionRedundancyApplicationStatus::Applied);
  assert(!run.k.stats.sr_expanded && run.k.stats.sr_expansion_native == 1);
  assert(witnessed_lines(run) == 1);
  std::remove(run.proof_path.c_str());

  // A default (true) pivot image is outside the permutation scope: 5 is pure,
  // so (5 7) is SR with the implicit witness {5}. The stream is then
  // witnessed, and a later in-budget permutation line stays native.
  Run padded;
  open(padded, formula(40), false);
  assert(apply(padded, {lit(5), lit(7)}, {}) ==
         SubstitutionRedundancyApplicationStatus::Applied);
  assert(!padded.k.stats.sr_expanded &&
         padded.k.stats.sr_expansion_native == 1);
  assert(witnessed_lines(padded) == 1);
  assert(apply(padded, {lit(-1), lit(2)},
               {{lit(1), lit(2)}, {lit(2), lit(1)}}) ==
         SubstitutionRedundancyApplicationStatus::Applied);
  assert(!padded.k.stats.sr_expanded &&
         padded.k.stats.sr_expansion_native == 2);
  assert(witnessed_lines(padded) == 2);
  std::remove(padded.proof_path.c_str());
}

void root_assigned_moved_variables() {
  // Variables 3 and 4 are root-true; (3 4) maps a true literal to a true one
  // and is left in place, so only the renamed swap (1 2) is expanded.
  std::string text = formula(40);
  text.replace(text.find('\n') + 1, 0, "3 0\n");
  text.replace(text.find("p cnf ") + 6, text.find('\n') - 6,
               std::to_string(45) + ' ' + std::to_string(45));
  Run run;
  open(run, text, false);
  assert(run.k.value(lit(3)) > 0);
  assert(apply(run, {lit(-1), lit(2)},
               {{lit(1), lit(2)}, {lit(2), lit(1)},
                {lit(3), lit(4)}, {lit(4), lit(3)}}) ==
         SubstitutionRedundancyApplicationStatus::Applied);
  assert(run.k.stats.sr_expanded == 1 && !witnessed_lines(run));
  std::remove(run.proof_path.c_str());
}

// A root-unit clause enters through (z | C) and (-z | C). With F symmetric
// under 1 -> -1, the unit (-1) is SR with that negation map.
void root_unit_clause_expands() {
  std::string text = "p cnf 45 42\n1 3 0\n-1 3 0\n";
  for (uint32_t i = 0; i < 40; ++i)
    text += std::to_string(5 + i) + ' ' + std::to_string(-int(6 + i)) + " 0\n";
  Run expanded, native;
  open(expanded, text, false);
  open(native, text, true);
  for (Run *run : {&expanded, &native})
    assert(apply(*run, {lit(-1)}, {{lit(1), lit(-1)}}) ==
           SubstitutionRedundancyApplicationStatus::Applied);
  assert(expanded.k.stats.sr_expanded == 1 && !witnessed_lines(expanded));
  assert(expanded.k.stats.sr_expansion_additions ==
         Proof::sr_expansion_additions(2, 1, 1, true));
  assert(native.k.stats.sr_expansion_native == 1 && witnessed_lines(native) == 1);
  assert(expanded.k.value(lit(-1)) > 0 && native.k.value(lit(-1)) > 0);
  assert(expanded.clauses() == native.clauses());
  std::remove(expanded.proof_path.c_str());
  std::remove(native.proof_path.c_str());
}

} // namespace

int main() {
  expansion_matches_native_state();
  over_budget_and_witnessed_streams_stay_native();
  root_assigned_moved_variables();
  root_unit_clause_expands();
  std::puts("sr_expansion: ok");
  return 0;
}
