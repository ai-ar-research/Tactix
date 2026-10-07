#include "engine/subsolver.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;

namespace {

int dimacs(SubLit literal) {
  const int variable = static_cast<int>(sub_var_of(literal) + 1);
  return (literal & 1u) ? -variable : variable;
}

SubLit internal(int literal) {
  const uint32_t variable = static_cast<uint32_t>(literal < 0 ? -literal : literal);
  return sub_lit(variable - 1, literal < 0);
}

uint64_t line_bytes(const SubStep &step) {
  uint64_t bytes = 2; // `0\n`
  for (const SubLit literal : step.lits)
    bytes += std::to_string(dimacs(literal)).size() + 1;
  return bytes;
}

void json_array(std::ofstream &out, const std::vector<int> &values) {
  out << '[';
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    out << values[i];
  }
  out << ']';
}

void metadata(const char *path, uint32_t target, uint32_t first,
              uint32_t second, uint32_t cube,
              const std::vector<int> &assumptions, const char *result,
              const SubSolver &solver, const std::vector<int> &target_clause,
              size_t steps, uint64_t proof_bytes, const char *reason) {
  std::ofstream out(path);
  if (!out) std::exit(30);
  out << "{\n  \"schema\":\"rank569-frontier-cube-case-v1\",\n"
      << "  \"target\":" << target << ",\n"
      << "  \"frontier\":[" << first << ',' << second << "],\n"
      << "  \"cube\":" << cube << ",\n"
      << "  \"assumptions\":";
  json_array(out, assumptions);
  out << ",\n  \"result\":\"" << result << "\",\n"
      << "  \"ticks\":" << solver.ticks() << ",\n"
      << "  \"kitten_ticks\":" << solver.kitten_ticks() << ",\n"
      << "  \"conflicts\":" << solver.conflicts() << ",\n"
      << "  \"derivation_steps\":" << steps << ",\n"
      << "  \"target_clause\":";
  json_array(out, target_clause);
  out << ",\n  \"proof_bytes\":" << proof_bytes << ",\n"
      << "  \"reason\":\"" << reason << "\"\n}\n";
  if (!out) std::exit(31);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 10) {
    std::fprintf(stderr,
        "usage: %s INPUT TARGET FRONTIER_A FRONTIER_B CUBE TICKS BYTES PROOF META\n",
        argv[0]);
    return 2;
  }
  const uint32_t target = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 10));
  const uint32_t first = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10));
  const uint32_t second = static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 10));
  const uint32_t cube = static_cast<uint32_t>(std::strtoul(argv[5], nullptr, 10));
  const uint64_t tick_cap = std::strtoull(argv[6], nullptr, 10);
  const uint64_t byte_cap = std::strtoull(argv[7], nullptr, 10);
  if (!target || !first || !second || target == first || target == second ||
      first == second || cube > 3 || !tick_cap || !byte_cap)
    return 3;

  std::ifstream in(argv[1]);
  if (!in) return 4;
  uint32_t variables = 0, declared_clauses = 0;
  std::vector<std::vector<SubLit>> clauses;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == 'c') continue;
    if (line[0] == 'p') {
      char p = 0;
      std::string cnf;
      std::istringstream header(line);
      header >> p >> cnf >> variables >> declared_clauses;
      continue;
    }
    std::istringstream clause_line(line);
    std::vector<SubLit> clause;
    int literal = 0;
    while (clause_line >> literal && literal) {
      const uint32_t external = static_cast<uint32_t>(literal < 0 ? -literal : literal);
      if (!external || external > variables) return 5;
      clause.push_back(internal(literal));
    }
    clauses.push_back(std::move(clause));
  }
  if (!variables || clauses.size() != declared_clauses || target > variables ||
      first > variables || second > variables)
    return 6;

  const int first_assumption = (cube & 1u) ? static_cast<int>(first)
                                            : -static_cast<int>(first);
  const int second_assumption = (cube & 2u) ? static_cast<int>(second)
                                             : -static_cast<int>(second);
  const std::vector<int> assumptions_dimacs = {
      static_cast<int>(target), first_assumption, second_assumption};
  std::vector<SubLit> assumptions;
  for (const int literal : assumptions_dimacs) assumptions.push_back(internal(literal));

  SubSolver solver;
  solver.reset(variables, SubSearchMode::Kitten);
  solver.start_kitten_session();
  for (const auto &clause : clauses) solver.add_clause(clause);
  solver.randomize_phases();
  const SubResult result = solver.solve(assumptions, tick_cap, SubTickMode::Kitten);
  if (result != SubResult::Unsat) {
    metadata(argv[9], target, first, second, cube, assumptions_dimacs,
             result == SubResult::Budget ? "BUDGET" : "SAT", solver, {}, 0, 0,
             result == SubResult::Budget ? "tick-cap" : "satisfying-assumptions");
    return result == SubResult::Budget ? 10 : 11;
  }

  SubDerivation derivation;
  if (!solver.derivation(derivation) || derivation.resource_exhausted ||
      derivation.steps.empty()) {
    metadata(argv[9], target, first, second, cube, assumptions_dimacs,
             "INVALID", solver, {}, 0, 0, "missing-derivation");
    return 12;
  }
  std::vector<int> target_clause;
  for (const SubLit literal : derivation.steps.back().lits)
    target_clause.push_back(dimacs(literal));
  std::vector<int> allowed;
  for (const int literal : assumptions_dimacs) allowed.push_back(-literal);
  std::sort(allowed.begin(), allowed.end());
  auto normalized = target_clause;
  std::sort(normalized.begin(), normalized.end());
  if (std::adjacent_find(normalized.begin(), normalized.end()) != normalized.end() ||
      !std::includes(allowed.begin(), allowed.end(),
                     normalized.begin(), normalized.end())) {
    metadata(argv[9], target, first, second, cube, assumptions_dimacs,
             "INVALID", solver, target_clause, derivation.steps.size(), 0,
             "target-not-negated-assumption-subset");
    return 13;
  }

  uint64_t proof_bytes = 0;
  for (const SubStep &step : derivation.steps) {
    const uint64_t amount = line_bytes(step);
    if (amount > byte_cap - std::min(byte_cap, proof_bytes)) {
      metadata(argv[9], target, first, second, cube, assumptions_dimacs,
               "PROOF_CAP", solver, target_clause, derivation.steps.size(),
               proof_bytes, "serialized-proof-cap");
      return 14;
    }
    proof_bytes += amount;
  }
  std::ofstream proof(argv[8]);
  if (!proof) return 15;
  for (const SubStep &step : derivation.steps) {
    for (const SubLit literal : step.lits) proof << dimacs(literal) << ' ';
    proof << "0\n";
  }
  proof.close();
  if (!proof || std::filesystem::file_size(argv[8]) != proof_bytes) return 16;
  metadata(argv[9], target, first, second, cube, assumptions_dimacs,
           "UNSAT", solver, target_clause, derivation.steps.size(), proof_bytes,
           "complete-assumption-refutation");
  return 0;
}
