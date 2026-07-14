#include "frontend/parse.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

void ingest_clause(Kernel &k, std::vector<Lit> &scratch) {
  // grow the kernel to cover every mentioned variable
  Var maxv = 0;
  for (const Lit l : scratch)
    maxv = std::max(maxv, var_of(l));
  if (!scratch.empty())
    kernel_reserve_vars(k, maxv + 1);

  // set semantics: dedupe; tautologies contribute nothing
  std::sort(scratch.begin(), scratch.end());
  scratch.erase(std::unique(scratch.begin(), scratch.end()), scratch.end());
  for (size_t i = 1; i < scratch.size(); i++)
    if (scratch[i] == negate(scratch[i - 1]))
      return; // tautology

  if (k.inconsistent)
    return; // already refuted; further clauses don't matter

  if (scratch.empty()) { // empty input clause: immediate refutation
    k.inconsistent = true;
    if (k.proof)
      k.proof->add_empty();
  } else if (scratch.size() == 1) {
    const Lit u = scratch[0];
    if (k.values[u] > 0)
      return; // duplicate unit
    if (k.values[u] < 0) { // contradicting unit: refuted at the root
      k.inconsistent = true;
      if (k.proof)
        k.proof->add_empty();
      return;
    }
    kernel_assign_unit(k, u);
  } else if (scratch.size() == 2) {
    kernel_add_binary_clause(k, scratch[0], scratch[1]);
  } else {
    kernel_add_large_clause(k, scratch.data(),
                            static_cast<uint32_t>(scratch.size()), false, 0);
  }
}

const char *parse_dimacs(FILE *f, Kernel &k) {
  std::vector<Lit> scratch;
  int c = getc(f);
  for (;;) {
    // skip whitespace
    while (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      c = getc(f);
    if (c == EOF)
      break;
    if (c == 'c') { // comment line
      while (c != '\n' && c != EOF)
        c = getc(f);
      continue;
    }
    if (c == 'p') { // header: "p cnf <vars> <clauses>" (counts advisory)
      while (c != '\n' && c != EOF) {
        if (c >= '0' && c <= '9') { // first number = declared variables
          uint64_t n = 0;
          while (c >= '0' && c <= '9') {
            n = n * 10 + (c - '0');
            if (n > (1u << 28))
              return "variable count out of range";
            c = getc(f);
          }
          kernel_reserve_vars(k, static_cast<uint32_t>(n));
          while (c != '\n' && c != EOF) // ignore the clause count
            c = getc(f);
          break;
        }
        c = getc(f);
      }
      continue;
    }
    // a literal
    bool neg = false;
    if (c == '-') {
      neg = true;
      c = getc(f);
    }
    if (c < '0' || c > '9')
      return "expected literal";
    uint64_t n = 0;
    while (c >= '0' && c <= '9') {
      n = n * 10 + (c - '0');
      if (n > (1u << 28))
        return "literal out of range";
      c = getc(f);
    }
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != EOF)
      return "unexpected character after literal";
    if (n == 0) {
      ingest_clause(k, scratch);
      scratch.clear();
    } else {
      if (neg)
        scratch.push_back(internalize(-static_cast<int>(n)));
      else
        scratch.push_back(internalize(static_cast<int>(n)));
    }
  }
  if (!scratch.empty()) // tolerate a missing final 0
    ingest_clause(k, scratch);
  return nullptr;
}

} // namespace tactix
