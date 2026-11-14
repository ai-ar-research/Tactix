#pragma once
// DIMACS fixtures: render clauses as text and parse text into a Kernel
// through a stream, as the command line reads a file.
#ifdef NDEBUG
#error "dimacs_text.hpp asserts have side effects; #undef NDEBUG before including it"
#endif

#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

template <class Clauses>
std::string dimacs_text(uint32_t variables, const Clauses &clauses) {
  std::ostringstream output;
  output << "p cnf " << variables << ' ' << clauses.size() << '\n';
  for (const auto &clause : clauses) {
    for (int32_t literal : clause) output << literal << ' ';
    output << "0\n";
  }
  return output.str();
}

// What a successful parse must also leave at the root.
enum class ParsedRoot {
  Any,
  EmptyTrail, // no root unit, propagation at fixpoint
  Consistent, // not inconsistent, propagation at fixpoint
};

inline void parse_dimacs_text(const std::string &text, tactix::Kernel &kernel,
                              ParsedRoot root = ParsedRoot::Any) {
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  assert(tactix::parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  if (root == ParsedRoot::EmptyTrail)
    assert(kernel.trail.empty() && tactix::kernel_propagation_fixpoint(kernel));
  if (root == ParsedRoot::Consistent)
    assert(!kernel.inconsistent && tactix::kernel_propagation_fixpoint(kernel));
}

inline tactix::Kernel parse_dimacs_text(const std::string &text,
                                        ParsedRoot root = ParsedRoot::Any) {
  tactix::Kernel kernel;
  parse_dimacs_text(text, kernel, root);
  return kernel;
}

// Ingest zero-terminated external DIMACS clauses, then finish input, as the
// parser does.
inline void input_clauses(tactix::Kernel &kernel, std::initializer_list<int32_t> raw) {
  std::vector<tactix::Lit> clause;
  for (const int32_t literal : raw) {
    if (literal) {
      clause.push_back(tactix::internalize(literal));
    } else {
      tactix::ingest_clause(kernel, clause);
      clause.clear();
    }
  }
  assert(clause.empty());
  tactix::finish_input(kernel);
}
