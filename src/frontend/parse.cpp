#include "frontend/parse.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

void ingest_clause(Kernel &k, std::vector<Lit> &scratch) {
  // every input clause consumes one proof id in input order — including
  // tautologies/duplicates that are never stored (standard LRAT input
  // numbering)
  const uint64_t id = ++k.next_clause_id;

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
    if (k.proof) { // the input empty clause itself is the whole chain
      const uint64_t chain[1] = {id};
      k.proof->add_empty(++k.next_clause_id, chain, 1);
    }
  } else if (scratch.size() == 1) {
    const Lit u = scratch[0];
    if (k.values[u] > 0)
      return; // duplicate unit (its id is consumed, never referenced)
    if (k.values[u] < 0) { // contradicting unit: refuted at the root
      k.inconsistent = true;
      if (k.proof) {
        const uint64_t chain[2] = {k.unit_ids[var_of(u)], id};
        k.proof->add_empty(++k.next_clause_id, chain, 2);
      }
      return;
    }
    k.unit_ids[var_of(u)] = id;
    kernel_assign_unit(k, u);
  } else if (scratch.size() == 2) {
    // duplicate binary: id consumed, never stored — the duplicate-unit
    // precedent above (the checker keeps its never-referenced input copy;
    // no proof lines). Nothing is deleted during parse, so presence in
    // binary_ids ⟺ a live stored copy. kissat's dense mode likewise
    // dedups before its inprocessing ever runs; duplicated input pairs
    // otherwise inflate Factor's watch-score seeds and block BVE via the
    // singular-delete hazard — on mp1-blockpuzzle (137,700 duplicated
    // pairs) deduplication alone took the arrangement from 74 s to 5.3 s
    // vs kissat's 9.1 (probe 1561180, 2026-07-25).
    if (k.binary_ids.count(kernel_binary_key(scratch[0], scratch[1])))
      return;
    kernel_add_binary_clause(k, scratch[0], scratch[1], id);
  } else {
    kernel_add_large_clause(k, scratch.data(),
                            static_cast<uint32_t>(scratch.size()), false, 0,
                            id);
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
